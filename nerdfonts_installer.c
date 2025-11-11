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
#define MAX_PATH_LEN 512
#define MAX_COMMAND_LEN 1024
#define API_URL                                                                \
  "https://api.github.com/repos/ryanoasis/nerd-fonts/contents/"                \
  "patched-fonts?ref=master"

// Global variables
static char fonts[MAX_FONTS][MAX_FONT_NAME_LEN];
static int font_count = 0;
static char home_path[MAX_PATH_LEN];
static char tmp_path[MAX_PATH_LEN];
static char fonts_path[MAX_PATH_LEN];

// Structure for HTTP response
struct HTTPResponse {
  char *memory;
  size_t size;
};

// Callback function for libcurl to write response data
static size_t write_callback(void *contents, size_t size, size_t nmemb,
                             struct HTTPResponse *response) {
  size_t realsize = size * nmemb;
  char *ptr = realloc(response->memory, response->size + realsize + 1);

  if (!ptr) {
    printf(COLOR_RED
           "Error: Not enough memory (realloc returned NULL)\n" COLOR_RESET);
    return 0;
  }

  response->memory = ptr;
  memcpy(&(response->memory[response->size]), contents, realsize);
  response->size += realsize;
  response->memory[response->size] = 0;

  return realsize;
}

// Function to detect OS and return appropriate package manager command
const char *detect_os_and_get_package_manager() {
  FILE *fp = fopen("/etc/os-release", "r");
  if (!fp) {
    printf(COLOR_RED "OS detection failed. Please install curl, unzip, and "
                     "fontconfig manually.\n" COLOR_RESET);
    exit(1);
  }

  char line[256];
  char os_id[50] = {0};

  while (fgets(line, sizeof(line), fp)) {
    if (strncmp(line, "ID=", 3) == 0) {
      sscanf(line, "ID=%49s", os_id);
      // Remove quotes if present
      if (os_id[0] == '"') {
        memmove(os_id, os_id + 1, strlen(os_id));
        os_id[strlen(os_id) - 1] = '\0';
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
    printf(COLOR_RED "Unsupported OS: %s\n" COLOR_RESET, os_id);
    exit(1);
  }
}

// Function to check if a command exists
int command_exists(const char *command) {
  char cmd[256];
  snprintf(cmd, sizeof(cmd), "command -v %s >/dev/null 2>&1", command);
  return system(cmd) == 0;
}

// Function to install a package using the package manager
void install_package(const char *package_manager, const char *package) {
  char cmd[MAX_COMMAND_LEN];
  snprintf(cmd, sizeof(cmd), "%s %s", package_manager, package);

  printf(COLOR_YELLOW "%s not found. Installing %s...\n" COLOR_RESET, package,
         package);

  if (system(cmd) != 0) {
    printf(COLOR_RED "Failed to install %s\n" COLOR_RESET, package);
    exit(1);
  }
}

// Function to install dependencies
void install_dependencies() {
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

  printf(COLOR_GREEN "✓ All dependencies are installed\n" COLOR_RESET);
}

// Function to create necessary directories
void create_directories() {
  const char *home = getenv("HOME");
  if (!home) {
    printf(COLOR_RED "Error: Could not get HOME directory\n" COLOR_RESET);
    exit(1);
  }

  snprintf(home_path, sizeof(home_path), "%s", home);
  snprintf(fonts_path, sizeof(fonts_path), "%s/.local/share/fonts", home);
  snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", home);

  // Create directories with error checking
  char cmd[MAX_COMMAND_LEN];
  int result;

  snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", fonts_path);
  result = system(cmd);
  if (result != 0) {
    printf(COLOR_RED "Warning: Failed to create fonts directory\n" COLOR_RESET);
  }

  snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", tmp_path);
  result = system(cmd);
  if (result != 0) {
    printf(COLOR_RED "Warning: Failed to create tmp directory\n" COLOR_RESET);
  }
}

// Function to fetch available fonts from GitHub API
void fetch_available_fonts() {
  CURL *curl;
  CURLcode res;
  struct HTTPResponse response = {0};

  printf(COLOR_YELLOW "Fetching available fonts from GitHub...\n" COLOR_RESET);

  curl = curl_easy_init();
  if (!curl) {
    printf(COLOR_RED "Failed to initialize curl\n" COLOR_RESET);
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
    printf(COLOR_RED
           "Failed to fetch font list from GitHub API: %s\n" COLOR_RESET,
           curl_easy_strerror(res));
    if (response.memory)
      free(response.memory);
    exit(1);
  }

  if (!response.memory || response.size == 0) {
    printf(COLOR_RED "Empty response from GitHub API\n" COLOR_RESET);
    exit(1);
  }

  // Parse JSON response
  json_error_t error;
  json_t *root = json_loads(response.memory, 0, &error);
  free(response.memory);

  if (!root) {
    printf(COLOR_RED "JSON parsing error: %s\n" COLOR_RESET, error.text);
    exit(1);
  }

  if (!json_is_array(root)) {
    printf(COLOR_RED "Invalid JSON response format\n" COLOR_RESET);
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
      strncpy(fonts[font_count], name, MAX_FONT_NAME_LEN - 1);
      fonts[font_count][MAX_FONT_NAME_LEN - 1] = '\0';
      font_count++;
    }
  }

  json_decref(root);

  if (font_count == 0) {
    printf(COLOR_RED "No fonts found in the API response\n" COLOR_RESET);
    exit(1);
  }

  printf(COLOR_GREEN "Found %d available fonts\n" COLOR_RESET, font_count);
}

// Function to print fonts in columns, adapting to terminal width
void print_fonts_in_columns() {
  int term_width = 80; // Default terminal width
  struct winsize w;
  int fd = open("/dev/tty", O_RDWR);
  if (fd >= 0) {
    if (ioctl(fd, TIOCGWINSZ, &w) == 0) {
      term_width = w.ws_col;
    }
    close(fd);
  }

  // Find the widest font name
  int max_len = 0;
  for (int i = 0; i < font_count; i++) {
    int len = strlen(fonts[i]);
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
        char item[MAX_FONT_NAME_LEN + 20];
        snprintf(item, sizeof(item), "%d. %.*s", idx + 1, MAX_FONT_NAME_LEN, fonts[idx]);
        printf("%-*s", col_width, item);
      }
    }
    printf("\n");
  }
}

// Function to display fonts with a pager
void display_fonts_with_pager() {
  int pipefd[2];
  pid_t pid;
  const char *pager_cmd = getenv("PAGER");
  if (pager_cmd == NULL) {
    pager_cmd = "less -R -X -F";
  }

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
    close(pipefd[1]); // Close write end
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    execlp("/bin/sh", "sh", "-c", pager_cmd, NULL);
    perror("execlp");
    _exit(127);
  } else {            // Parent process
    close(pipefd[0]); // Close read end
    int stdout_backup = dup(STDOUT_FILENO);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    print_fonts_in_columns();
    fflush(stdout);
    dup2(stdout_backup, STDOUT_FILENO);
    close(stdout_backup);
    wait(NULL);
  }
}

// Function to check if font exists in releases
int check_font_exists(const char *font_name) {
  CURL *curl;
  CURLcode res;
  long response_code;
  char url[512];
  snprintf(
      url, sizeof(url),
      "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/%s.zip",
      font_name);
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

// Function to download and install a font
int download_and_install_font(const char *font_name) {
  char url[512], zip_path[MAX_PATH_LEN], cmd[MAX_COMMAND_LEN];
  CURL *curl;
  FILE *fp;
  CURLcode res;
  printf(COLOR_BLUE "Downloading and installing %s\n" COLOR_RESET, font_name);

  // Check if font exists
  if (!check_font_exists(font_name)) {
    printf(COLOR_RED
           "Warning: %s not found in releases, skipping...\n" COLOR_RESET,
           font_name);
    return 0;
  }

  // Prepare paths and URLs - use safer snprintf with size checking
  int url_len = snprintf(
      url, sizeof(url),
      "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/%s.zip",
      font_name);

  if (url_len >= (int)sizeof(url)) {
    printf(COLOR_RED "Error: Font name too long for URL buffer\n" COLOR_RESET);
    return 0;
  }

  int path_len =
      snprintf(zip_path, sizeof(zip_path), "%s/%s.zip", tmp_path, font_name);

  if (path_len >= (int)sizeof(zip_path)) {
    printf(COLOR_RED "Error: Path too long for buffer\n" COLOR_RESET);
    return 0;
  }

  // Download font
  curl = curl_easy_init();

  if (!curl) {
    printf(COLOR_RED "Failed to initialize curl for %s\n" COLOR_RESET,
           font_name);
    return 0;
  }

  fp = fopen(zip_path, "wb");

  if (!fp) {
    printf(COLOR_RED "Failed to create file %s\n" COLOR_RESET, zip_path);
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
    printf(COLOR_RED "Failed to download %s: %s\n" COLOR_RESET, font_name,
           curl_easy_strerror(res));
    unlink(zip_path); // Remove partial file
    return 0;
  }

  // Extract font - use safer snprintf with size checking
  int cmd_len =
      snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\" >/dev/null 2>&1",
               zip_path, fonts_path);

  if (cmd_len >= (int)sizeof(cmd)) {
    printf(COLOR_RED "Error: Command too long for buffer\n" COLOR_RESET);
    unlink(zip_path);
    return 0;
  }

  if (system(cmd) != 0) {
    printf(COLOR_RED "Failed to extract %s\n" COLOR_RESET, font_name);
    unlink(zip_path);
    return 0;
  }

  // Remove zip file
  unlink(zip_path);

  printf(COLOR_GREEN "✓ %s installed successfully\n" COLOR_RESET, font_name);
  return 1;
}

// Function to update font cache
void update_font_cache() {
  printf(COLOR_BLUE "Updating font cache...\n" COLOR_RESET);
  if (system("fc-cache -f >/dev/null 2>&1") == 0) {
    printf(COLOR_GREEN "✓ Font installation complete!\n" COLOR_RESET);
  }

  else {
    printf(COLOR_YELLOW "Warning: Font cache update failed, but fonts were "
                        "installed\n" COLOR_RESET);
  }
}

// Function to cleanup temporary files
void cleanup() {
  char cmd[MAX_COMMAND_LEN];
  snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"/*.zip 2>/dev/null", tmp_path);
  int result = system(cmd);
  (void)result; // Suppress unused result warning - cleanup is best effort
}

// Signal handler for cleanup
void signal_handler(int sig) {
  (void)sig; // Suppress unused parameter warning
  printf(COLOR_YELLOW "\nCleaning up and exiting...\n" COLOR_RESET);
  cleanup();
  exit(0);
}

// Function to get user input for font selection
void get_font_selection(int *selected_indices, int *num_selected) {
  char input[1024];
  char *token;
  FILE *tty = fopen("/dev/tty", "r");

  if (tty == NULL) {
    perror("fopen /dev/tty");
    exit(EXIT_FAILURE);
  }

  while (1) {
    printf(COLOR_CYAN
           "Enter the numbers of the fonts to install (e.g., \"1 2 3\") or "
           "type \"all\" to install all fonts: " COLOR_RESET);

    if (!fgets(input, sizeof(input), tty)) {
      printf(COLOR_RED "Error reading input\n" COLOR_RESET);
      continue;
    }

    // Remove newline
    input[strcspn(input, "\n")] = 0;

    // Check for empty input
    if (strlen(input) == 0) {
      printf(COLOR_RED "Error: Please select at least one font or type "
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

      // Check if token is a valid number
      if (*endptr != '\0' || selection < 1 || selection > font_count) {
        printf(COLOR_RED "Error: Invalid selection \"%s\". Please enter "
                         "numbers between 1 and %d.\n" COLOR_RESET,
               token, font_count);
        valid_selection = 0;
        break;
      }

      selected_indices[*num_selected] = selection - 1;
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
  printf(COLOR_GREEN "🚀 Nerd Fonts Installer\n" COLOR_RESET);
  printf("━━━━━━━━━━━━━━━━━━━━━━━━\n\n");

  // Install dependencies
  install_dependencies();

  // Create directories
  create_directories();

  // Fetch available fonts
  fetch_available_fonts();

  // Display font selection menu
  printf(COLOR_GREEN "Select fonts to install (separate with spaces, or enter "
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
    printf(COLOR_GREEN "\n🎉 Successfully installed %d fonts!\n" COLOR_RESET,
           installed_count);
  } else {
    printf(COLOR_RED "No fonts were installed.\n" COLOR_RESET);
  }

  // Cleanup
  cleanup();
  curl_global_cleanup();
  return 0;
}
