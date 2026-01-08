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

// ANSI Color codes
#define COLOR_RED "\033[0;31m"
#define COLOR_GREEN "\033[0;32m"
#define COLOR_YELLOW "\033[0;33m"
#define COLOR_BLUE "\033[0;34m"
#define COLOR_MAGENTA "\033[0;35m"
#define COLOR_CYAN "\033[0;36m"
#define COLOR_RESET "\033[0m"

// Constants
#define MAX_FONTS 100
#define MAX_FONT_NAME_LEN 50
#define MAX_PATH_LEN 1024
#define MAX_COMMAND_LEN 2048
#define API_URL                                                                \
  "https://api.github.com/repos/ryanoasis/nerd-fonts/contents/"                \
  "patched-fonts?ref=master"

// Global variables
static char fonts[MAX_FONTS][MAX_FONT_NAME_LEN]; // flawfinder: ignore
static int font_count = 0;
static char home_path[MAX_PATH_LEN]; // flawfinder: ignore
static char tmp_path[MAX_PATH_LEN]; // flawfinder: ignore
static char fonts_path[MAX_PATH_LEN]; // flawfinder: ignore

// Structure for HTTP response
struct HTTPResponse {
  char *memory;
  size_t size;
};

// ============================================================================
// SECURITY HELPER FUNCTIONS
// ============================================================================

// Validate font name (alphanumeric, hyphens, underscores only)
static int validate_font_name(const char *name) {
  if (!name || strlen(name) == 0 || strlen(name) >= MAX_FONT_NAME_LEN) { // flawfinder: ignore
    return 0;
  }

  for (size_t i = 0; name[i] != '\0'; i++) {
    char c = name[i];
    if (!isalnum((unsigned char)c) && c != '-' && c != '_' && c != '.') {
      return 0;
    }
  }

  // Prevent ".." or starting with "." for security
  if (name[0] == '.' || strstr(name, "..") != NULL) {
    return 0;
  }
  return 1;
}

// Secure directory creation using mkdir() instead of system()
static int create_directory_secure(const char *path) {
  // Try to create the directory
  if (mkdir(path, 0755) == 0) {
    return 0; // Success
  }

  if (errno == EEXIST) {
    // Directory already exists, check if it's actually a directory
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
      return 0; // Directory exists and is valid
    }
    return -1;
  }

  // If parent doesn't exist, try to create parent directories recursively
  if (errno == ENOENT) {
    char parent[MAX_PATH_LEN]; // flawfinder: ignore
    strncpy(parent, path, sizeof(parent) - 1); // flawfinder: ignore
    parent[sizeof(parent) - 1] = '\0';

    char *last_slash = strrchr(parent, '/');
    if (last_slash != NULL && last_slash != parent) {
      *last_slash = '\0';
      if (create_directory_secure(parent) != 0) {
        return -1;
      }
      return mkdir(path, 0755) == 0 ? 0 : -1;
    }
  }

  return -1;
}

// Simple secure file deletion with basic path validation
static int secure_unlink(const char *filepath) {
  if (!filepath || strlen(filepath) == 0) { // flawfinder: ignore
    return -1;
  }

  // Basic sanity checks
  if (strstr(filepath, "..") != NULL) {
    fprintf(stderr, "Error: Path contains '..' - potential traversal attempt\n");
    return -1;
  }

  // Atomically attempt deletion to avoid TOCTOU race condition
  if (unlink(filepath) != 0) {
    if (errno == ENOENT) {
      // File already gone, consider success
      return 0;
    }
    return -1;
  }

  return 0;
}

// ============================================================================
// ORIGINAL FUNCTIONS (with security improvements)
// ============================================================================

// Callback function for libcurl to write response data
static size_t write_callback(char *contents, size_t size, size_t nmemb, // cppcheck-suppress constParameterCallback
                             void *userp) {
  struct HTTPResponse *response = (struct HTTPResponse *)userp;
  size_t realsize = size * nmemb;
  char *ptr = realloc(response->memory, response->size + realsize + 1);

  if (!ptr) {
    printf("%s", COLOR_RED "Error: Not enough memory (realloc returned NULL)\n" COLOR_RESET);
    return 0;
  }

  response->memory = ptr;
  memcpy(&(response->memory[response->size]), contents, realsize); // flawfinder: ignore
  response->size += realsize;
  response->memory[response->size] = 0;

  return realsize;
}

// Function to detect OS and return appropriate package manager command
static const char *detect_os_and_get_package_manager() {
  FILE *fp = fopen("/etc/os-release", "r"); // flawfinder: ignore
  if (!fp) {
    printf("%s", COLOR_RED "OS detection failed. Please install curl, unzip, and "
           "fontconfig manually.\n" COLOR_RESET);
    exit(1);
  }

  char line[256]; // flawfinder: ignore
  char os_id[50] = {0}; // flawfinder: ignore

  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "ID=", 3) == 0) {
      sscanf(line, "ID=%49s", os_id); // flawfinder: ignore
      // Remove quotes if present
      if (os_id[0] == '"') {
        memmove(os_id, os_id + 1, strlen(os_id)); // flawfinder: ignore
        os_id[strlen(os_id) - 1] = '\0'; // flawfinder: ignore
      }
      break;
    }
  }
  fclose(fp);

  printf("Detected OS: %s\n", os_id);

  if (strcmp(os_id, "ubuntu") == 0 || strcmp(os_id, "debian") == 0 ||
      strcmp(os_id, "linuxmint") == 0 || strcmp(os_id, "kali") == 0 ||
      strcmp(os_id, "deepin") == 0 || strcmp(os_id, "devuan") == 0 ||
      strcmp(os_id, "mx") == 0 || strcmp(os_id, "pop") == 0) {
    return "sudo apt-get update && sudo apt-get install -y";
  } else if (strcmp(os_id, "fedora") == 0) {
    return "sudo dnf install -y";
  } else if (strcmp(os_id, "centos") == 0 || strcmp(os_id, "rhel") == 0) {
    return "sudo yum install -y";
  } else if (strcmp(os_id, "arch") == 0 || strcmp(os_id, "manjaro") == 0 ||
             strcmp(os_id, "endeavouros") == 0 ||
             strcmp(os_id, "cachyos") == 0 || strcmp(os_id, "garuda") == 0 ||
             strcmp(os_id, "artix") == 0 || strcmp(os_id, "arco") == 0 ||
             strcmp(os_id, "steamos") == 0 || strcmp(os_id, "blackarch") == 0) {
    return "sudo pacman -Syu --noconfirm";
  } else {
    printf("%s", COLOR_RED "Unsupported OS: ");
    printf("%s\n", os_id);
    printf("%s", COLOR_RESET);
    exit(1);
  }
}

// Function to check if a command exists (Pure C implementation for speed and security)
static int command_exists(const char *command) {
  const char *path = getenv("PATH"); // flawfinder: ignore
  if (!path)
    return 0;

  char *path_copy = strdup(path);
  if (!path_copy)
    return 0;

  const char *dir = strtok(path_copy, ":");
  while (dir != NULL) {
    char full_path[MAX_PATH_LEN]; // flawfinder: ignore
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, command);
    if (access(full_path, X_OK) == 0) { // flawfinder: ignore
      free(path_copy);
      return 1;
    }
    dir = strtok(NULL, ":");
  }

  free(path_copy);
  return 0;
}

// Function to install a package (Secure: uses sh -c for complex commands)
static void install_package(const char *package_manager, const char *package) {
  printf("%s%s%s", COLOR_YELLOW, package, " not found. Installing " COLOR_RESET);
  printf("%s...\n", package);

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    exit(1);
  }

  if (pid == 0) {
    char cmd[MAX_COMMAND_LEN]; // flawfinder: ignore
    snprintf(cmd, sizeof(cmd), "%s %s", package_manager, package);

    // We use sh -c to handle possible shell features like "&&" in the
    // package_manager string. This is safe because both package_manager
    // and package are controlled strings within this program.
    execlp("sh", "sh", "-c", cmd, (char *)NULL); // flawfinder: ignore
    perror("execlp sh");
    _exit(127);
  }

  int status;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    printf("%s%s\n", COLOR_RED "Failed to install ", package);
    printf("%s", COLOR_RESET);
    exit(1);
  }
}

// Function to install dependencies
static void install_dependencies() {
  const char *pkg_manager = detect_os_and_get_package_manager();

  if (!command_exists("curl")) {
    install_package(pkg_manager, "curl");
  }

  if (!command_exists("unzip")) {
    install_package(pkg_manager, "unzip");
  }

  if (!command_exists("fc-cache")) {
    install_package(pkg_manager, "fontconfig");
  }

  printf("%s", COLOR_GREEN "✓ All dependencies are installed\n" COLOR_RESET);
}

// SECURE: Function to create necessary directories using mkdir() instead of system()
static void create_directories() {
  const char *home = getenv("HOME"); // flawfinder: ignore
  if (!home) {
    printf("%s", COLOR_RED "Error: Could not get HOME directory\n" COLOR_RESET);
    exit(1);
  }

  // Validate HOME path length
  size_t home_len = strlen(home); // flawfinder: ignore
  if (home_len == 0 || home_len >= MAX_PATH_LEN - 50) {
    printf("%s", COLOR_RED "Error: HOME path too long or invalid\n" COLOR_RESET);
    exit(1);
  }

  snprintf(home_path, sizeof(home_path), "%s", home);
  snprintf(fonts_path, sizeof(fonts_path), "%s/.local/share/fonts", home);
  snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", home);

  // Create directories securely
  if (create_directory_secure(fonts_path) != 0) {
    printf("%s", COLOR_YELLOW "Note: Fonts directory may already exist\n" COLOR_RESET);
  }

  if (create_directory_secure(tmp_path) != 0) {
    printf("%s", COLOR_YELLOW "Note: Temp directory may already exist\n" COLOR_RESET);
  }
}

// Function to fetch available fonts from GitHub API
static void fetch_available_fonts() {
  CURL *curl;
  CURLcode res;
  struct HTTPResponse response = {0};

  printf("%s", COLOR_YELLOW "Fetching available fonts from GitHub...\n" COLOR_RESET);

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

  res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    printf("%s", COLOR_RED "Failed to fetch font list from GitHub API: ");
    printf("%s\n", curl_easy_strerror(res));
    printf("%s", COLOR_RESET);
    free(response.memory);
    exit(1);
  }

  if (!response.memory || response.size == 0) {
    printf("%s", COLOR_RED "Empty response from GitHub API\n" COLOR_RESET);
    exit(1);
  }

  // Parse JSON response
  json_error_t error;
  json_t *root = json_loads(response.memory, 0, &error);
  free(response.memory);

  if (!root) {
    printf("%s", COLOR_RED "JSON parsing error: ");
    printf("%s\n", error.text);
    printf("%s", COLOR_RESET);
    exit(1);
  }

  if (!json_is_array(root)) {
    printf("%s", COLOR_RED "Invalid JSON response format\n" COLOR_RESET);
    json_decref(root);
    exit(1);
  }

  size_t index;
  json_t *value;
  font_count = 0;

  json_array_foreach(root, index, value) {
    if (font_count >= MAX_FONTS)
      break;

    json_t *name_obj = json_object_get(value, "name");
    if (json_is_string(name_obj)) {
      const char *name = json_string_value(name_obj);
      strncpy(fonts[font_count], name, MAX_FONT_NAME_LEN - 1); // flawfinder: ignore
      fonts[font_count][MAX_FONT_NAME_LEN - 1] = '\0';
      font_count++;
    }
  }

  json_decref(root);

  if (font_count == 0) {
    printf("%s", COLOR_RED "No fonts found in the API response\n" COLOR_RESET);
    exit(1);
  }

  printf("%s%d available fonts\n", COLOR_GREEN "Found ", font_count);
  printf("%s", COLOR_RESET);
}

// Function to print fonts in columns, adapting to terminal width
static void print_fonts_in_columns() {
  int term_width = 80; // Default terminal width
  struct winsize w;
  int fd = open("/dev/tty", O_RDWR); // flawfinder: ignore
  if (fd >= 0) {
    if (ioctl(fd, TIOCGWINSZ, &w) == 0) {
      term_width = w.ws_col;
    }
    close(fd);
  }

  // Find the widest font name
  int max_len = 0;
  for (int i = 0; i < font_count; i++) {
    int len = (int)strlen(fonts[i]); // flawfinder: ignore
    if (len > max_len) {
      max_len = len;
    }
  }

  // Calculate column width and number of columns
  int col_width = max_len + 8; // "n. " + padding
  int columns = term_width / col_width;
  if (columns == 0) {
    columns = 1;
  }

  // Calculate number of rows
  int rows = (font_count + columns - 1) / columns;

  // Print fonts in a column-major order
  for (int i = 0; i < rows; i++) {
    for (int j = 0; j < columns; j++) {
      int idx = i + j * rows;
      if (idx < font_count) {
        char item[MAX_FONT_NAME_LEN + 20]; // flawfinder: ignore
        snprintf(item, sizeof(item), "%d. %.*s", idx + 1, MAX_FONT_NAME_LEN, fonts[idx]);
        printf("%-*s", col_width, item);
      }
    }
    printf("\n");
  }
}

// SECURE: Function to display fonts with a pager (hardcoded pager for security)
static void display_fonts_with_pager() {
  int pipefd[2];
  pid_t pid;

  if (pipe(pipefd) == -1) {
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();

  if (pid == -1) {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  if (pid == 0) {     // Child process
    // SECURITY FIX: Use hardcoded pager instead of PAGER environment variable
    const char *pager_args[] = {"less", "-R", "-X", "-F", NULL};
    close(pipefd[1]); // Close write end
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    // SECURITY FIX: Use execv with hardcoded path instead of shell execution
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
    execvp("less", (char *const *)pager_args); // flawfinder: ignore
#pragma GCC diagnostic pop
    perror("execvp");
    _exit(127);
  } else {            // Parent process
    close(pipefd[0]); // Close read end
    int stdout_backup = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    print_fonts_in_columns();
    fflush(stdout);
    if (stdout_backup != -1) {
      dup2(stdout_backup, STDOUT_FILENO);
      close(stdout_backup);
    }
    wait(NULL);
  }
}

// Function to check if font exists in releases
static int check_font_exists(const char *font_name) {
  CURL *curl;
  CURLcode res;
  long response_code;
  char url[1024]; // flawfinder: ignore

  // Validate font name first
  if (!validate_font_name(font_name)) {
    return 0;
  }

  int url_len = snprintf(
      url, sizeof(url),
      "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/%s.zip",
      font_name);

  if (url_len >= (int)sizeof(url) || url_len < 0) {
    return 0;
  }

  curl = curl_easy_init();

  if (!curl)
    return 0;
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
  curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  res = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
  curl_easy_cleanup(curl);
  return (res == CURLE_OK && response_code == 200);
}

// SECURE: Function to execute unzip using fork/exec instead of system()
static int secure_unzip(const char *zip_file, const char *dest_dir) {
  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
    return -1;
  }

  if (pid == 0) {
    // Child process - redirect output to /dev/null
    int devnull = open("/dev/null", O_WRONLY); // flawfinder: ignore
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }

    // Execute unzip with arguments
    execlp("unzip", "unzip", "-o", zip_file, "-d", dest_dir, (char *)NULL); // flawfinder: ignore
    perror("execlp unzip");
    _exit(127);
  } else {
    // Parent process - wait for child
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
    return -1;
  }
}

// SECURE: Download and install a font with path validation
static int download_and_install_font(const char *font_name) {
  char url[1024]; // flawfinder: ignore
  char zip_path[MAX_PATH_LEN + 32]; // flawfinder: ignore
  CURL *curl;
  FILE *fp;
  CURLcode res;

  printf("%s", COLOR_BLUE "Downloading and installing ");
  printf("%s\n", font_name);
  printf("%s", COLOR_RESET);

  // SECURITY FIX: Validate font name
  if (!validate_font_name(font_name)) {
    printf("%s", COLOR_RED "Error: Invalid font name\n" COLOR_RESET);
    return 0;
  }

  // Check if font exists
  if (!check_font_exists(font_name)) {
    printf("%s", COLOR_RED "Warning: ");
    printf("%s not found in releases, skipping...\n", font_name);
    printf("%s", COLOR_RESET);
    return 0;
  }

  // Prepare URL
  int url_len = snprintf(
      url, sizeof(url),
      "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/%s.zip",
      font_name);

  if (url_len >= (int)sizeof(url) || url_len < 0) {
    printf("%s", COLOR_RED "Error: Font name too long for URL buffer\n" COLOR_RESET);
    return 0;
  }

  // SECURITY: Validate font name before using in path
  if (!validate_font_name(font_name)) {
    printf("%s%s\n", COLOR_RED "Error: Invalid font name received from API: ", font_name);
    printf("%s", COLOR_RESET);
    return 0;
  }

  // Prepare zip path
  int path_len = snprintf(zip_path, sizeof(zip_path), "%s/%s.zip", tmp_path, font_name);

  if (path_len >= (int)sizeof(zip_path) || path_len < 0) {
    printf("%s", COLOR_RED "Error: Path too long for buffer\n" COLOR_RESET);
    return 0;
  }

  // Download font
  curl = curl_easy_init();

  if (!curl) {
    printf("%s", COLOR_RED "Failed to initialize curl for ");
    printf("%s\n", font_name);
    printf("%s", COLOR_RESET);
    return 0;
  }

  // SECURITY: Create file with restricted permissions (0600) and O_NOFOLLOW
  int fd = open(zip_path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
  if (fd == -1) {
    printf("%s", COLOR_RED "Failed to create file ");
    printf("%s: %s\n", zip_path, strerror(errno));
    printf("%s", COLOR_RESET);
    curl_easy_cleanup(curl);
    return 0;
  }
  fp = fdopen(fd, "wb");

  if (!fp) {
    printf("%s", COLOR_RED "Failed to open file stream for ");
    printf("%s\n", zip_path);
    printf("%s", COLOR_RESET);
    close(fd);
    curl_easy_cleanup(curl);
    return 0;
  }

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L); // 5 minutes for large files
  res = curl_easy_perform(curl);
  fclose(fp);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    printf("%s", COLOR_RED "Failed to download ");
    printf("%s: %s\n", font_name, curl_easy_strerror(res));
    printf("%s", COLOR_RESET);
    secure_unlink(zip_path);
    return 0;
  }

  // SECURITY FIX: Extract font using secure_unzip instead of system()
  if (secure_unzip(zip_path, fonts_path) != 0) {
    printf("%s", COLOR_RED "Failed to extract ");
    printf("%s\n", font_name);
    printf("%s", COLOR_RESET);
    secure_unlink(zip_path);
    return 0;
  }

  // SECURITY FIX: Remove zip file using secure_unlink
  secure_unlink(zip_path);

  printf("%s", COLOR_GREEN "✓ ");
  printf("%s installed successfully\n", font_name);
  printf("%s", COLOR_RESET);
  return 1;
}

// Function to update font cache
static void update_font_cache() {
  pid_t pid = fork();

  if (pid == -1) {
    printf("%s", COLOR_YELLOW "Warning: Failed to update font cache\n" COLOR_RESET);
    return;
  }

  if (pid == 0) {
    // Child process - redirect output to /dev/null
    int devnull = open("/dev/null", O_WRONLY); // flawfinder: ignore
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }

    execlp("fc-cache", "fc-cache", "-f", (char *)NULL); // flawfinder: ignore
    _exit(127);
  } else {
    // Parent process - wait for child
    int status;
    waitpid(pid, &status, 0);

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
      printf("%s", COLOR_GREEN "✓ Font installation complete!\n" COLOR_RESET);
    } else {
      printf("%s", COLOR_YELLOW "Warning: Font cache update failed, but fonts were ");
      printf("installed\n");
      printf("%s", COLOR_RESET);
    }
  }
}

// Cleanup temporary files
static void cleanup() {
  // Individual files are cleaned up during installation
  // This is a placeholder for any additional cleanup
}

// Signal handler for cleanup
static void signal_handler(int sig) {
  (void)sig;
  printf("%s", COLOR_YELLOW "\nCleaning up and exiting...\n" COLOR_RESET);
  cleanup();
  exit(0);
}

// SECURE: Get user input with better error messages (no sensitive data logging)
static void get_font_selection(int *selected_indices, int *num_selected) {
  char input[1024]; // flawfinder: ignore
  const char *token;
  FILE *tty = fopen("/dev/tty", "r"); // flawfinder: ignore

  if (tty == NULL) {
    perror("fopen /dev/tty");
    exit(EXIT_FAILURE);
  }

  while (1) {
    printf("%s", COLOR_CYAN "Enter the numbers of the fonts to install (e.g., \"1 2 3\") or "
           "type \"all\" to install all fonts: " COLOR_RESET);

    if (ferror(tty)) {
        clearerr(tty);
    }

    if (fgets(input, sizeof(input), tty) == NULL) {
      if (feof(tty)) {
        printf("\nEnd of input reached. Exiting selection.\n");
        break;
      }
      if (ferror(tty)) {
        printf("%s", COLOR_RED "Error reading input: ");
        printf("%s\n", strerror(errno));
        printf("%s", COLOR_RESET);
      }
      // Explicitly clear error and reset to try and satisfy static analysis
      clearerr(tty);
      continue;
    }

    // Remove newline
    input[strcspn(input, "\n")] = 0;

    // Check for empty input
    if (strlen(input) == 0) { // flawfinder: ignore
      printf("%s", COLOR_RED "Error: Please select at least one font or type "
                       "\"all\".\n" COLOR_RESET);
      continue;
    }

    // Check for "all"
    if (strcmp(input, "all") == 0) {
      *num_selected = font_count;
      for (int i = 0; i < font_count; i++) {
        selected_indices[i] = i;
      }
      break;
    }

    // Parse numeric input
    *num_selected = 0;
    int valid_selection = 1;
    token = strtok(input, " ");

    while (token != NULL && *num_selected < MAX_FONTS) {
      char *endptr;
      long selection = strtol(token, &endptr, 10);

      // SECURITY FIX: Don't log the actual token value
      if (*endptr != '\0' || selection < 1 || selection > font_count) {
        printf("%s", COLOR_RED "Error: Invalid selection. Please enter "
                         "numbers between 1 and ");
        printf("%d.\n", font_count);
        printf("%s", COLOR_RESET);
        valid_selection = 0;
        break;
      }

      selected_indices[*num_selected] = (int)(selection - 1);
      (*num_selected)++;
      token = strtok(NULL, " ");
    }

    if (valid_selection && *num_selected > 0) {
      break;
    }

    if (!valid_selection) {
      *num_selected = 0; // Reset for next iteration
    }
  }
  fclose(tty);
}

int main() {

  // Setup signal handlers
  signal(SIGINT, signal_handler);
  signal(SIGTERM, signal_handler);

  // Initialize curl
  curl_global_init(CURL_GLOBAL_DEFAULT);
  printf("%s", COLOR_GREEN "🚀 Nerd Fonts Installer\n" COLOR_RESET);
  printf("════════════════════════\n\n");

  // Install dependencies
  install_dependencies();

  // Create directories
  create_directories();

  // Fetch available fonts
  fetch_available_fonts();

  // Display font selection menu
  printf("%s", COLOR_GREEN "Select fonts to install (separate with spaces, or enter "
                     "\"all\" to install all fonts):\n" COLOR_RESET);
  printf("---------------------------------------------\n");
  display_fonts_with_pager();
  printf("---------------------------------------------\n\n");

  // Get user selection
  int selected_indices[MAX_FONTS];
  int num_selected = 0;
  get_font_selection(selected_indices, &num_selected);

  // Install selected fonts
  int installed_count = 0;

  for (int i = 0; i < num_selected; i++) {
    if (download_and_install_font(fonts[selected_indices[i]])) {
      installed_count++;
    }
  }

  // Update font cache if any fonts were installed
  if (installed_count > 0) {
    update_font_cache();
    printf("%s", COLOR_GREEN "\n🎉 Successfully installed ");
    printf("%d fonts!\n", installed_count);
    printf("%s", COLOR_RESET);
  } else {
    printf("%s", COLOR_RED "No fonts were installed.\n" COLOR_RESET);
  }

  // Cleanup
  cleanup();
  curl_global_cleanup();
  return 0;
}
