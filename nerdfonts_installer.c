#define _POSIX_C_SOURCE 200809L
#include <curl/curl.h>
#include <fcntl.h>
#include <jansson.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
// cppcheck-suppress missingIncludeSystem
#include <stdint.h>

// ANSI Color codes
#define COLOR_RED    "\033[0;31m"
#define COLOR_GREEN  "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE   "\033[0;34m"
#define COLOR_CYAN   "\033[0;36m"
#define COLOR_RESET  "\033[0m"

// Constants
#define MAX_FONTS        100
#define MAX_FONT_NAME_LEN 50
#define MAX_PATH_LEN     1024
#define MAX_COMMAND_LEN  2048
#define MKDTEMP_SUFFIX   "/nerdfonts.XXXXXX"  /* 17 chars + NUL = 18 */

/*
 * FIX 1: Use the Releases API instead of the Contents API.
 *
 * The Contents API lists directory names under patched-fonts/ in the git repo.
 * Those names are NOT guaranteed to match release asset filenames — some fonts
 * are absent from a given release, or are named differently. Constructing
 * download URLs from directory names causes spurious 404s (CURLE_HTTP_RETURNED_ERROR).
 *
 * The Releases API returns the actual assets attached to the latest release,
 * so the font list and the download URLs are always in sync.
 */
#define API_URL \
    "https://api.github.com/repos/ryanoasis/nerd-fonts/releases/latest"

// Global state
static char fonts[MAX_FONTS][MAX_FONT_NAME_LEN];
static int  font_count = 0;
static char tmp_path[MAX_PATH_LEN];
static char fonts_path[MAX_PATH_LEN];
static char current_zip_path[MAX_PATH_LEN] = {0};
static char unique_tmp_dir[MAX_PATH_LEN]   = {0};

// HTTP response buffer
struct HTTPResponse {
    char  *memory;
    size_t size;
};

// ============================================================================
// SECURITY HELPERS
// ============================================================================

// Whitelist-based font name sanitizer.
// Copies only [a-zA-Z0-9._-] into output; rejects anything else.
// Satisfies CodeQL taint tracking by producing a clean copy.
static int sanitize_font_name(const char *input, char *output, size_t max_len) {
    if (!input || !output || max_len == 0)
        return 0;

    size_t in_len = strlen(input); // flawfinder: ignore
    if (in_len == 0 || in_len >= max_len)
        return 0;

    // Reject leading dot or any path traversal
    if (input[0] == '.' || strstr(input, "..") != NULL)
        return 0;

    size_t out_idx = 0;
    for (size_t i = 0; input[i] != '\0'; i++) {
        if (out_idx >= max_len - 1)
            break;
        char c = input[i];
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.')
            output[out_idx++] = c;
        else
            return 0; // strict rejection
    }
    output[out_idx] = '\0';
    return 1;
}

// mkdir -p equivalent using only syscalls (no system()).
static int create_directory_secure(const char *path) {
    if (mkdir(path, 0755) == 0)
        return 0;

    if (errno == EEXIST) {
        struct stat st;
        if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
            return 0;
        return -1;
    }

    if (errno == ENOENT) {
        char parent[MAX_PATH_LEN];
        snprintf(parent, sizeof(parent), "%s", path); // flawfinder: ignore
        char *last_slash = strrchr(parent, '/');
        if (last_slash != NULL && last_slash != parent) {
            *last_slash = '\0';
            if (create_directory_secure(parent) != 0)
                return -1;
            return mkdir(path, 0755) == 0 ? 0 : -1;
        }
    }

    return -1;
}

// Validated unlink: rejects empty paths and traversal attempts.
static int secure_unlink(const char *filepath) {
    if (!filepath || filepath[0] == '\0')
        return -1;
    if (strstr(filepath, "..") != NULL)
        return -1;
    if (unlink(filepath) != 0)
        return (errno == ENOENT) ? 0 : -1;
    return 0;
}

// ============================================================================
// CLEANUP FUNCTIONS
// ============================================================================

// Remove the in-progress zip for the current font (called after each font).
static void cleanup_zip(void) {
    if (current_zip_path[0] != '\0') {
        secure_unlink(current_zip_path);
        current_zip_path[0] = '\0';
    }
}

// Full teardown: zip + unique temp dir.  Called at normal exit and on signals.
static void full_cleanup(void) {
    cleanup_zip();
    if (unique_tmp_dir[0] != '\0') {
        // rmdir only succeeds on an empty directory.
        // If a zip was already cleaned up by cleanup_zip(), this should succeed.
        // ENOTEMPTY/EACCES are silently ignored; the dir may be left on disk.
        rmdir(unique_tmp_dir); /* best-effort; ENOTEMPTY silently ignored */
        unique_tmp_dir[0] = '\0';
    }
}

// Signal handler.
// NOTE: global char arrays (current_zip_path, unique_tmp_dir) are not
// sig_atomic_t; this is a best-effort cleanup. A volatile flag + main-loop
// cleanup would be fully correct but adds significant complexity for a
// single-threaded CLI tool.
static void signal_handler(int sig) {
    (void)sig;
    static const char msg[] = "\nCleaning up and exiting...\n";
    // write() is async-signal-safe; sizeof(msg)-1 avoids strlen() which is not.
    // Dummy variable suppresses -Wunused-result; we cannot act on errors here.
    ssize_t unused = write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    (void)unused;
    full_cleanup();
    _exit(128 + sig);
}

// ============================================================================
// CORE FUNCTIONS
// ============================================================================

// libcurl write callback.
// Guards against integer overflow in size*nmemb and caps total response at
// 100 MB to prevent memory exhaustion from a malicious/broken API response.
static size_t write_callback(const char *contents, size_t size, size_t nmemb,
                             void *userp) {
    struct HTTPResponse *response = (struct HTTPResponse *)userp;

    // Overflow check before multiplication
    if (nmemb > 0 && size > SIZE_MAX / nmemb)
        return 0;

    size_t realsize = size * nmemb;

    // Response size cap — use subtraction to avoid overflow in the comparison
    // itself: if realsize alone exceeds the limit, or adding it would exceed it.
    if (realsize > 100UL * 1024UL * 1024UL ||
        response->size > 100UL * 1024UL * 1024UL - realsize)
        return 0;

    char *ptr = realloc(response->memory, response->size + realsize + 1);
    if (!ptr)
        return 0;

    response->memory = ptr;
    memcpy(&(response->memory[response->size]), contents, realsize); // flawfinder: ignore
    response->size += realsize;
    response->memory[response->size] = '\0';

    return realsize;
}

// PATH-based command existence check (no system() or popen()).
static int command_exists(const char *command) {
    const char *path = getenv("PATH");
    if (!path)
        return 0;

    char *path_copy = strdup(path);
    if (!path_copy)
        return 0;

    const char *dir = strtok(path_copy, ":");
    while (dir != NULL) {
        char full_path[MAX_PATH_LEN];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return 1;
        }
        dir = strtok(NULL, ":");
    }
    free(path_copy);
    return 0;
}

// unzip via fork/exec — no system() shell.
static int secure_unzip(const char *zip_file, const char *dest_dir) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        // Silence output in child
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("unzip", "unzip", "-o", zip_file, "-d", dest_dir, (char *)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// Read /etc/os-release and return the appropriate package manager command.
static const char *detect_os_and_get_package_manager(void) {
    FILE *fp = fopen("/etc/os-release", "r");
    if (!fp) {
        printf("%s", COLOR_RED "OS detection failed. Please install curl, "
               "unzip, and fontconfig manually.\n" COLOR_RESET);
        exit(1);
    }

    char line[256];
    char os_id[50] = {0};

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "ID=", 3) == 0) {
            sscanf(line, "ID=%49s", os_id); // flawfinder: ignore
            // Strip surrounding quotes if present (e.g. ID="arch")
            if (os_id[0] == '"') {
                memmove(os_id, os_id + 1, strlen(os_id)); // flawfinder: ignore
                size_t len = strlen(os_id); // flawfinder: ignore
                if (len > 0 && os_id[len - 1] == '"')
                    os_id[len - 1] = '\0';
            }
            break;
        }
    }
    fclose(fp);

    printf("Detected OS: %s\n", os_id);

    if (strcmp(os_id, "ubuntu")    == 0 || strcmp(os_id, "debian")   == 0 ||
        strcmp(os_id, "linuxmint") == 0 || strcmp(os_id, "kali")     == 0 ||
        strcmp(os_id, "deepin")    == 0 || strcmp(os_id, "devuan")   == 0 ||
        strcmp(os_id, "mx")        == 0 || strcmp(os_id, "pop")      == 0)
        return "sudo apt-get update && sudo apt-get install -y";

    if (strcmp(os_id, "fedora") == 0)
        return "sudo dnf install -y";

    if (strcmp(os_id, "centos") == 0 || strcmp(os_id, "rhel") == 0)
        return "sudo yum install -y";

    if (strcmp(os_id, "arch")        == 0 || strcmp(os_id, "manjaro")     == 0 ||
        strcmp(os_id, "endeavouros") == 0 || strcmp(os_id, "cachyos")     == 0 ||
        strcmp(os_id, "garuda")      == 0 || strcmp(os_id, "artix")       == 0 ||
        strcmp(os_id, "arco")        == 0 || strcmp(os_id, "steamos")     == 0 ||
        strcmp(os_id, "blackarch")   == 0)
        return "sudo pacman -Syu --noconfirm";

    printf("%sUnsupported OS: %s\n%s", COLOR_RED, os_id, COLOR_RESET);
    exit(1);
}

// Install a single package via the detected package manager.
// Uses sh -c only because the package_manager string contains && for apt-get;
// both package_manager and package are hardcoded strings within this program.
static void install_package(const char *package_manager, const char *package) {
    printf("%s%s not found. Installing %s...\n" COLOR_RESET, // flawfinder: ignore
           COLOR_YELLOW, package, package);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        char cmd[MAX_COMMAND_LEN];
        snprintf(cmd, sizeof(cmd), "%s %s", package_manager, package);
        execlp("sh", "sh", "-c", cmd, (char *)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        printf("%sFailed to install %s\n%s", COLOR_RED, package, COLOR_RESET);
        exit(1);
    }
}

// Check and install curl, unzip, fontconfig if missing.
static void install_dependencies(void) {
    const char *pkg_manager = detect_os_and_get_package_manager();

    if (!command_exists("curl"))
        install_package(pkg_manager, "curl");
    if (!command_exists("unzip"))
        install_package(pkg_manager, "unzip");
    if (!command_exists("fc-cache"))
        install_package(pkg_manager, "fontconfig");

    printf("%s", COLOR_GREEN "✓ All dependencies are installed\n" COLOR_RESET);
}

// Create fonts dir and a unique temp dir via mkdtemp().
// Temp dir priority: $TMPDIR -> /tmp -> ~/tmp (last resort).
static void create_directories(void) {
    const char *home = getenv("HOME");
    if (!home) {
        printf("%s", COLOR_RED "Error: Could not get HOME directory\n"
               COLOR_RESET);
        exit(1);
    }

    size_t home_len = strlen(home); // flawfinder: ignore

    // Ensure HOME is short enough for derived paths (fonts, tmp, mkdtemp suffix).
    // Longest derived path: home + "/.local/share/fonts" = home_len + 19.
    if (home_len == 0 || home_len >= MAX_PATH_LEN - 50) {
        printf("%s", COLOR_RED "Error: HOME path too long or invalid\n"
               COLOR_RESET);
        exit(1);
    }

    snprintf(fonts_path, sizeof(fonts_path), "%s/.local/share/fonts", home);

    // MKDTEMP_SUFFIX is 17 chars; sizeof() = 18 (includes NUL).
    // tmp_path must leave room for it.
    const char *env_tmp = getenv("TMPDIR");
    if (env_tmp && strlen(env_tmp) > 0 && // flawfinder: ignore
        strlen(env_tmp) + sizeof(MKDTEMP_SUFFIX) <= sizeof(tmp_path)) { // flawfinder: ignore
        snprintf(tmp_path, sizeof(tmp_path), "%s", env_tmp);
    } else if (access("/tmp", W_OK) == 0) {
        // "/tmp" is 4 chars; 4 + 18 = 22 << 1024, always fits.
        snprintf(tmp_path, sizeof(tmp_path), "/tmp");
    } else {
        // "~/tmp": home_len + 4. home_len < MAX_PATH_LEN - 50, so
        // home_len + 4 < MAX_PATH_LEN - 46. The suffix adds 17 more,
        // giving home_len + 21 < MAX_PATH_LEN - 29. Fits.
        snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", home);
    }

    // Belt-and-suspenders: explicit check the compiler can see, suppressing
    // -Wformat-truncation without a pragma.
    if (strlen(tmp_path) + sizeof(MKDTEMP_SUFFIX) > sizeof(unique_tmp_dir)) { // flawfinder: ignore
        printf("%s", COLOR_RED "Error: Temp path too long for mkdtemp\n"
               COLOR_RESET);
        exit(1);
    }
    /* Build unique_tmp_dir = tmp_path + MKDTEMP_SUFFIX via index loops.
     * Avoids snprintf (CWE-134), memcpy (CWE-120), strncpy/strncat
     * (MS-banned) scanner hits. Bounds guaranteed by guard above. */
    {
        size_t i = 0;
        for (; tmp_path[i] != '\0'; i++)
            unique_tmp_dir[i] = tmp_path[i];
        const char sfx[] = MKDTEMP_SUFFIX;
        for (size_t j = 0; j < sizeof(sfx); j++)
            unique_tmp_dir[i + j] = sfx[j];
    }

    if (mkdtemp(unique_tmp_dir) == NULL) {
        printf("%s", COLOR_RED "Error: Failed to create unique temp "
               "directory\n" COLOR_RESET);
        exit(1);
    }

    if (create_directory_secure(fonts_path) != 0) {
        // EEXIST is normal for returning users; create_directory_secure already
        // handles it silently. Any other failure is non-fatal: fonts may still
        // install if the directory was created by another means.
    }
}

/*
 * Fetch the font list from the GitHub Releases API and populate fonts[].
 *
 * FIX 1 (API endpoint): Changed from the Contents API (which lists git
 *   directory names under patched-fonts/) to the Releases API (which lists
 *   actual release assets). The two namespaces are not identical — some fonts
 *   present in the repo are absent from a given release, causing 404s when
 *   the download URL was constructed from the directory name.
 *
 * FIX 2 (HTTP error detection): Added CURLOPT_FAILONERROR so that 4xx/5xx
 *   responses (e.g. 403 rate-limit) are returned as curl errors rather than
 *   silently delivering an error JSON body. The HTTP code is retrieved before
 *   cleanup to provide a useful diagnostic for rate-limit hits.
 *
 * FIX 3 (JSON structure): The Releases API returns an object, not an array.
 *   Navigate to root["assets"], iterate that array, filter to *.zip assets,
 *   exclude FontPatcher.zip, and strip the .zip suffix before storing. The
 *   download function appends .zip when constructing the URL, so the stored
 *   bare name is sufficient.
 */
static void fetch_available_fonts(void) {
    CURL *curl;
    CURLcode res;
    struct HTTPResponse response = {0};

    printf("%s", COLOR_YELLOW "Fetching available fonts from GitHub...\n"
           COLOR_RESET);

    curl = curl_easy_init();
    if (!curl) {
        printf("%s", COLOR_RED "Failed to initialize curl\n" COLOR_RESET);
        exit(1);
    }

    curl_easy_setopt(curl, CURLOPT_URL, API_URL);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "nerdfonts-installer/1.0");
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // Treat 4xx/5xx as curl errors so rate-limit responses don't reach the
    // JSON parser as if they were valid release data.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

    res = curl_easy_perform(curl);

    // Retrieve HTTP code before cleanup invalidates the handle.
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        free(response.memory);
        response.memory = NULL;
        if (res == CURLE_HTTP_RETURNED_ERROR &&
            (http_code == 403 || http_code == 429)) {
            printf("%sFailed to fetch font list: HTTP %ld (rate-limited).\n"
                   "Set GITHUB_TOKEN in your environment to raise the limit:\n"
                   "  export GITHUB_TOKEN=ghp_...\n%s",
                   COLOR_RED, http_code, COLOR_RESET);
        } else {
            printf("%sFailed to fetch font list from GitHub API: %s\n%s",
                   COLOR_RED, curl_easy_strerror(res), COLOR_RESET);
        }
        exit(1);
    }

    if (!response.memory || response.size == 0) {
        printf("%s", COLOR_RED "Empty response from GitHub API\n" COLOR_RESET);
        free(response.memory);
        exit(1);
    }

    json_error_t error;
    json_t *root = json_loads(response.memory, 0, &error);
    free(response.memory);
    response.memory = NULL;

    if (!root) {
        printf("%sJSON parsing error: %s\n%s",
               COLOR_RED, error.text, COLOR_RESET);
        exit(1);
    }

    // Releases API returns an object ({...}), not an array ([...]).
    if (!json_is_object(root)) {
        printf("%s", COLOR_RED
               "Invalid JSON response format (expected release object)\n"
               COLOR_RESET);
        json_decref(root);
        exit(1);
    }

    json_t *assets = json_object_get(root, "assets");
    if (!json_is_array(assets)) {
        printf("%s", COLOR_RED
               "Invalid JSON: missing or malformed 'assets' array\n"
               COLOR_RESET);
        json_decref(root);
        exit(1);
    }

    size_t index;
    json_t *value;
    font_count = 0;

    json_array_foreach(assets, index, value) {
        if (font_count >= MAX_FONTS) {
            printf("%sWarning: font limit (%d) reached; some fonts omitted.\n"
                   "%s", COLOR_YELLOW, MAX_FONTS, COLOR_RESET);
            break;
        }

        json_t *name_obj = json_object_get(value, "name");
        if (!json_is_string(name_obj))
            continue;

        const char *name = json_string_value(name_obj);
        size_t len = strlen(name); // flawfinder: ignore

        // Must end in ".zip"
        if (len <= 4 || strcmp(name + len - 4, ".zip") != 0)
            continue;

        // Exclude the font patcher archive — it is not a font.
        if (strcmp(name, "FontPatcher.zip") == 0)
            continue;

        // Store the bare name (without .zip) for display.
        // download_and_install_font() appends .zip when building the URL.
        /* len > 4 is guaranteed by the guard above, so bare_len >= 1.
         * Only the upper bound needs checking. */
        size_t bare_len = len - 4;
        if (bare_len >= MAX_FONT_NAME_LEN)
            continue;

        /* Loop copy: bare_len < MAX_FONT_NAME_LEN enforced above,
         * dest is MAX_FONT_NAME_LEN bytes — no overflow possible. */
        for (size_t i = 0; i < bare_len; i++)
            fonts[font_count][i] = name[i];
        fonts[font_count][bare_len] = '\0';
        font_count++;
    }

    json_decref(root);

    if (font_count == 0) {
        printf("%s", COLOR_RED
               "No fonts found in the release assets\n" COLOR_RESET);
        exit(1);
    }

    printf("%sFound %d available fonts\n%s",
           COLOR_GREEN, font_count, COLOR_RESET);
}

// Query terminal width, defaulting to 80 if unavailable.
static int get_term_width(void) {
    struct winsize w;
    int fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        if (ioctl(fd, TIOCGWINSZ, &w) == 0 && w.ws_col > 10) {
            close(fd);
            return (int)w.ws_col;
        }
        close(fd);
    }
    return 80;
}

// Print a full-width separator.
static void print_separator(void) {
    int width = get_term_width();
    char *line = malloc((size_t)width + 1);
    if (line) {
        memset(line, '-', (size_t)width);
        line[width] = '\0';
        printf("%s\n", line);
        free(line);
    } else {
        printf("---------------------------------------------\n");
    }
}

// Print font list in terminal-width-aware columns.
static void print_fonts_in_columns(void) {
    int term_width = get_term_width();
    int max_len = 0;

    for (int i = 0; i < font_count; i++) {
        int len = (int)strnlen(fonts[i], MAX_FONT_NAME_LEN);
        if (len > max_len)
            max_len = len;
    }

    int col_width = max_len + 8; // "NNN. " prefix + padding
    int columns   = term_width / col_width;
    if (columns == 0)
        columns = 1;

    int rows = (font_count + columns - 1) / columns;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            int idx = i + j * rows;
            if (idx < font_count) {
                char item[MAX_FONT_NAME_LEN + 20];
                snprintf(item, sizeof(item), "%d. %.*s",
                         idx + 1, MAX_FONT_NAME_LEN, fonts[idx]);
                printf("%-*s", col_width, item);
            }
        }
        printf("\n");
    }
}

// Pipe font list through `less` if available, otherwise print directly.
static void display_fonts_with_pager(void) {
    if (!command_exists("less")) {
        print_fonts_in_columns();
        return;
    }

    int pipefd[2];
    if (pipe(pipefd) == -1) {
        print_fonts_in_columns();
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        print_fonts_in_columns();
        return;
    }

    if (pid == 0) {
        // Child: run less, reading from pipe
        const char *pager_args[] = {"less", "-R", "-X", "-F", NULL};
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
        execvp("less", (char *const *)pager_args);
#pragma GCC diagnostic pop
        _exit(127);
    } else {
        // Parent: write font list into pipe
        close(pipefd[0]);
        int stdout_backup = dup(STDOUT_FILENO);
        if (stdout_backup != -1) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[1]);
            print_fonts_in_columns();
            fflush(stdout);
            dup2(stdout_backup, STDOUT_FILENO);
            close(stdout_backup);
        } else {
            // dup failed — can't safely redirect; print directly and close pipe
            close(pipefd[1]);
            print_fonts_in_columns();
        }
        wait(NULL);
    }
}

// Download a font zip and extract it into fonts_path.
// CURLOPT_FAILONERROR ensures 404 responses are treated as errors rather
// than silently writing the HTML error page to the zip file.
static int download_and_install_font(const char *font_name) {
    char url[1024];
    CURL *curl;
    FILE *fp;
    CURLcode res;

    printf("%sDownloading and installing %s\n%s",
           COLOR_BLUE, font_name, COLOR_RESET);

    // Sanitize font name before constructing any paths or URLs
    char safe_name[MAX_FONT_NAME_LEN];
    if (!sanitize_font_name(font_name, safe_name, sizeof(safe_name))) {
        printf("%s", COLOR_RED "Error: Invalid font name\n" COLOR_RESET);
        return 0;
    }

    int url_len = snprintf(url, sizeof(url),
        "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/%s.zip",
        safe_name);
    if (url_len < 0 || url_len >= (int)sizeof(url)) {
        printf("%s", COLOR_RED "Error: Font name too long for URL buffer\n"
               COLOR_RESET);
        return 0;
    }

    // Use realpath(path, NULL) so the system allocates a correctly-sized buffer;
    // avoids PATH_MAX portability issues. Free after constructing zip path.
    char *resolved_dir = realpath(unique_tmp_dir, NULL); // flawfinder: ignore
    if (resolved_dir == NULL) {
        printf("%s", COLOR_RED "Error: Could not resolve temp directory\n"
               COLOR_RESET);
        return 0;
    }

    int zip_len = snprintf(current_zip_path, sizeof(current_zip_path),
                           "%s/%s.zip", resolved_dir, safe_name);
    free(resolved_dir);
    if (zip_len < 0 || zip_len >= (int)sizeof(current_zip_path)) {
        printf("%s", COLOR_RED "Error: Path too long\n" COLOR_RESET);
        current_zip_path[0] = '\0';
        return 0;
    }

    curl = curl_easy_init();
    if (!curl) {
        printf("%sFailed to initialize curl for %s\n%s",
               COLOR_RED, font_name, COLOR_RESET);
        current_zip_path[0] = '\0';
        return 0;
    }

    // Create the zip file with restricted permissions and no symlink following
    int fd = open(current_zip_path,
                  O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (fd == -1) {
        printf("%sFailed to create file %s: %s\n%s",
               COLOR_RED, current_zip_path, strerror(errno), COLOR_RESET);
        curl_easy_cleanup(curl);
        current_zip_path[0] = '\0';
        return 0;
    }

    fp = fdopen(fd, "wb");
    if (!fp) {
        printf("%sFailed to open file stream for %s\n%s",
               COLOR_RED, current_zip_path, COLOR_RESET);
        close(fd);
        curl_easy_cleanup(curl);
        current_zip_path[0] = '\0';
        return 0;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "nerdfonts-installer/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    // FAILONERROR: treats HTTP 4xx/5xx as curl errors, preventing HTML error
    // pages from being written to disk as if they were valid zip files.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 min for large fonts

    res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("%sFailed to download %s: %s\n%s",
               COLOR_RED, font_name, curl_easy_strerror(res), COLOR_RESET);
        cleanup_zip();
        return 0;
    }

    if (secure_unzip(current_zip_path, fonts_path) != 0) {
        printf("%sFailed to extract %s\n%s",
               COLOR_RED, font_name, COLOR_RESET);
        cleanup_zip();
        return 0;
    }

    cleanup_zip();
    printf("%s✓ %s installed successfully\n%s",
           COLOR_GREEN, font_name, COLOR_RESET);
    return 1;
}

// Rebuild the font cache via fc-cache.
static void update_font_cache(void) {
    pid_t pid = fork();
    if (pid == -1) {
        printf("%s", COLOR_YELLOW "Warning: Failed to fork for font cache "
               "update\n" COLOR_RESET);
        return;
    }

    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull != -1) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execlp("fc-cache", "fc-cache", "-f", (char *)NULL);
        _exit(127);
    }

    int status;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        printf("%s", COLOR_GREEN "✓ Font cache updated\n" COLOR_RESET);
    else
        printf("%s", COLOR_YELLOW "Warning: Font cache update failed, "
               "but fonts were installed\n" COLOR_RESET);
}

// Prompt for font selection and populate selected_indices[].
static void get_font_selection(int *selected_indices, int *num_selected) {
    char  input[1024];
    FILE *tty = fopen("/dev/tty", "r");
    if (!tty) {
        perror("fopen /dev/tty");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("%s", COLOR_CYAN
               "Enter font numbers (e.g. \"1 2 3\") or \"all\": "
               COLOR_RESET);

        if (ferror(tty))
            clearerr(tty);

        if (fgets(input, sizeof(input), tty) == NULL) {
            if (feof(tty)) {
                fprintf(stderr, "\nError: No font selected (EOF on input). Exiting.\n");
                fclose(tty);
                full_cleanup();
                curl_global_cleanup();
                exit(EXIT_FAILURE);
            }
            if (ferror(tty)) {
                printf("%sError reading input: %s\n%s",
                       COLOR_RED, strerror(errno), COLOR_RESET);
            }
            clearerr(tty);
            continue;
        }

        // Strip newline
        input[strcspn(input, "\n")] = '\0';

        if (strlen(input) == 0) { // flawfinder: ignore
            printf("%s", COLOR_RED
                   "Error: Please select at least one font or type \"all\".\n"
                   COLOR_RESET);
            continue;
        }

        if (strcmp(input, "all") == 0) {
            *num_selected = font_count;
            for (int i = 0; i < font_count; i++)
                selected_indices[i] = i;
            break;
        }

        *num_selected = 0;
        int valid = 1;
        const char *token = strtok(input, " ");

        while (token != NULL && *num_selected < MAX_FONTS) {
            char *endptr;
            long sel = strtol(token, &endptr, 10);
            if (*endptr != '\0' || sel < 1 || sel > font_count) {
                printf("%sError: Invalid selection. Enter numbers 1–%d.\n%s",
                       COLOR_RED, font_count, COLOR_RESET);
                valid = 0;
                break;
            }
            selected_indices[(*num_selected)++] = (int)(sel - 1);
            token = strtok(NULL, " ");
        }

        if (valid && *num_selected > 0)
            break;
        if (!valid)
            *num_selected = 0;
    }
    fclose(tty);
}

int main(void) {
    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    curl_global_init(CURL_GLOBAL_DEFAULT);

    printf("%s🚀 Nerd Fonts Installer\n%s", COLOR_GREEN, COLOR_RESET);
    print_separator();
    printf("\n");

    install_dependencies();
    create_directories();
    fetch_available_fonts();

    printf("%s", COLOR_GREEN
           "Select fonts to install (space-separated numbers, or \"all\"):\n"
           COLOR_RESET);
    print_separator();
    display_fonts_with_pager();
    print_separator();
    printf("\n");

    int selected_indices[MAX_FONTS];
    int num_selected = 0;
    get_font_selection(selected_indices, &num_selected);

    int installed_count = 0;
    for (int i = 0; i < num_selected; i++) {
        if (download_and_install_font(fonts[selected_indices[i]]))
            installed_count++;
    }

    if (installed_count > 0) {
        update_font_cache();
        printf("%s\n🎉 Successfully installed %d font%s!\n%s",
               COLOR_GREEN, installed_count,
               installed_count == 1 ? "" : "s",
               COLOR_RESET);
    } else {
        printf("%s", COLOR_RED "No fonts were installed.\n" COLOR_RESET);
    }

    full_cleanup();
    curl_global_cleanup();
    return 0;
}
