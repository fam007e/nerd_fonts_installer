#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h> // For EEXIST
#include <curl/curl.h>
#include <jansson.h> // For JSON parsing

// Function to execute a shell command and return its output
char* execute_command_with_output(const char* command) {
    FILE *fp;
    char *output = NULL;
    char path[1024];
    size_t current_size = 0;

    fp = popen(command, "r");
    if (fp == NULL) {
        fprintf(stderr, "Failed to run command: %s\n", command);
        return NULL;
    }

    while (fgets(path, sizeof(path), fp) != NULL) {
        size_t len = strlen(path);
        output = realloc(output, current_size + len + 1);
        if (output == NULL) {
            perror("realloc failed");
            pclose(fp);
            return NULL;
        }
        strcpy(output + current_size, path);
        current_size += len;
    }
    pclose(fp);
    return output;
}

// Function to execute a shell command and check its exit status
int execute_command(const char* command) {
    printf("\033[0;34mExecuting: %s\033[0m\n", command);
    int status = system(command);
    if (status == -1) {
        perror("system failed");
        return -1;
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1; // Command did not exit normally
}

// Function to detect the OS and set the package manager
void detect_os_and_set_package_manager(char** pkg_manager_cmd, char** pkg_install_flags) {
    FILE *fp = fopen("/etc/os-release", "r");
    if (fp == NULL) {
        fprintf(stderr, "\033[0;31mOS detection failed. Cannot open /etc/os-release. Please install curl, unzip, and fontconfig manually.\033[0m\n");
        exit(EXIT_FAILURE);
    }

    char line[256];
    char os_id[64] = {0};

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strncmp(line, "ID=", 3) == 0) {
            sscanf(line, "ID=%s", os_id);
            // Remove quotes if present
            if (os_id[0] == '"' || os_id[0] == '"') { // Corrected single quote to double quote for consistency
                memmove(os_id, os_id + 1, strlen(os_id));
                os_id[strlen(os_id) - 1] = '\0';
            }
            break;
        }
    }
    fclose(fp);

    if (strlen(os_id) == 0) {
        fprintf(stderr, "\033[0;31mOS detection failed. Could not find ID in /etc/os-release. Please install curl, unzip, and fontconfig manually.\033[0m\n");
        exit(EXIT_FAILURE);
    }

    if (strcmp(os_id, "ubuntu") == 0 || strcmp(os_id, "debian") == 0 || strcmp(os_id, "linuxmint") == 0) {
        *pkg_manager_cmd = strdup("sudo apt-get");
        *pkg_install_flags = strdup("update && sudo apt-get install -y");
    } else if (strcmp(os_id, "fedora") == 0) {
        *pkg_manager_cmd = strdup("sudo dnf");
        *pkg_install_flags = strdup("install -y");
    } else if (strcmp(os_id, "centos") == 0 || strcmp(os_id, "rhel") == 0) {
        *pkg_manager_cmd = strdup("sudo yum");
        *pkg_install_flags = strdup("install -y");
    } else if (strcmp(os_id, "arch") == 0 || strcmp(os_id, "manjaro") == 0 || strcmp(os_id, "endeavouros") == 0) {
        *pkg_manager_cmd = strdup("sudo pacman");
        *pkg_install_flags = strdup("-Syu --noconfirm");
    } else {
        fprintf(stderr, "\033[0;31mUnsupported OS: %s. Please install curl, unzip, and fontconfig manually.\033[0m\n", os_id);
        exit(EXIT_FAILURE);
    }
    printf("\033[0;32mDetected OS: %s, Package Manager: %s\033[0m\n", os_id, *pkg_manager_cmd);
}

// Function to check if a command exists
int command_exists(const char* cmd) {
    char command[256];
    snprintf(command, sizeof(command), "command -v %s >/dev/null 2>&1", cmd);
    return system(command) == 0;
}

// Function to check and install dependencies
void install_dependencies(const char* pkg_manager_cmd, const char* pkg_install_flags) {
    char command[512];

    if (!command_exists("curl")) {
        printf("\033[0;33mcurl not found. Installing curl...\033[0m\n");
        snprintf(command, sizeof(command), "%s %s curl", pkg_manager_cmd, pkg_install_flags);
        if (execute_command(command) != 0) {
            fprintf(stderr, "\033[0;31mFailed to install curl.\033[0m\n");
            exit(EXIT_FAILURE);
        }
    }

    if (!command_exists("unzip")) {
        printf("\033[0;33munzip not found. Installing unzip...\033[0m\n");
        snprintf(command, sizeof(command), "%s %s unzip", pkg_manager_cmd, pkg_install_flags);
        if (execute_command(command) != 0) {
            fprintf(stderr, "\033[0;31mFailed to install unzip.\033[0m\n");
            exit(EXIT_FAILURE);
        }
    }

    if (!command_exists("fc-cache")) {
        printf("\033[0;33mfontconfig (fc-cache) not found. Installing fontconfig...\033[0m\n");
        snprintf(command, sizeof(command), "%s %s fontconfig", pkg_manager_cmd, pkg_install_flags);
        if (execute_command(command) != 0) {
            fprintf(stderr, "\033[0;31mFailed to install fontconfig.\033[0m\n");
            exit(EXIT_FAILURE);
        }
    }
}

// Callback for curl to write data into a string
struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {
        /* out of memory! */
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Function to fetch available fonts from GitHub API
json_t* fetch_available_fonts() {
    printf("\033[0;33mFetching available fonts from GitHub...\033[0m\n");

    CURL *curl_handle;
    CURLcode res;
    struct MemoryStruct chunk;

    chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */
    chunk.size = 0;    /* no data at this point */

    curl_global_init(CURL_GLOBAL_ALL);
    curl_handle = curl_easy_init();

    if(curl_handle) {
        curl_easy_setopt(curl_handle, CURLOPT_URL, "https://api.github.com/repos/ryanoasis/nerd-fonts/contents/patched-fonts?ref=master");
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 30L); // 30 seconds timeout
        curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 10L); // 10 seconds connect timeout

        res = curl_easy_perform(curl_handle);

        if(res != CURLE_OK) {
            fprintf(stderr, "\033[0;31mFailed to fetch font list from GitHub API: %s\033[0m\n", curl_easy_strerror(res));
            if (chunk.memory) free(chunk.memory);
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();
            exit(EXIT_FAILURE);
        } else {
            json_error_t error;
            json_t *root = json_loads(chunk.memory, 0, &error);

            if (chunk.memory) free(chunk.memory);
            curl_easy_cleanup(curl_handle);
            curl_global_cleanup();

            if (!root) {
                fprintf(stderr, "\033[0;31mFailed to parse JSON from GitHub API: %s (line %d, col %d)\033[0m\n", error.text, error.line, error.column);
                exit(EXIT_FAILURE);
            }
            return root;
        }
    }
    return NULL; // Should not reach here
}

// Function to display fonts in columns
void print_fonts_in_columns(json_t* fonts_json_array) {
    if (!fonts_json_array || !json_is_array(fonts_json_array)) {
        fprintf(stderr, "\033[0;31mInvalid fonts array provided.\033[0m\n");
        return;
    }

    size_t total_fonts = json_array_size(fonts_json_array);
    if (total_fonts == 0) {
        printf("\033[0;31mNo fonts found in the API response. Please try again later.\033[0m\n");
        return;
    }

    printf("\033[0;32mFound %zu available fonts\033[0m\n", total_fonts);

    // Get terminal width (simplified, actual tput cols is more complex in C)
    // For now, assume a fixed width or try to get it from environment
    int cols = 80; // Default
    char* term_cols_str = getenv("COLUMNS");
    if (term_cols_str) {
        cols = atoi(term_cols_str);
        if (cols < 40) cols = 80; // Minimum reasonable width
    }

    int items_per_col = 20;
    int columns = (total_fonts + items_per_col - 1) / items_per_col; // Ceiling division

    printf("\033[0;32mSelect fonts to install (separate with spaces, or enter \"all\" to install all fonts):\033[0m\n");
    printf("---------------------------------------------\n");

    char display_buffer[total_fonts][60]; // Max font name length + index + padding
    for (size_t i = 0; i < total_fonts; ++i) {
        json_t *font_obj = json_array_get(fonts_json_array, i);
        json_t *name_obj = json_object_get(font_obj, "name");
        if (json_is_string(name_obj)) {
            snprintf(display_buffer[i], sizeof(display_buffer[i]), "%zu. %s", i + 1, json_string_value(name_obj));
        } else {
            snprintf(display_buffer[i], sizeof(display_buffer[i]), "%zu. (Unknown)", i + 1);
        }
    }

    for (int i = 0; i < items_per_col; ++i) {
        for (int j = 0; j < columns; ++j) {
            size_t idx = i + j * items_per_col;
            if (idx < total_fonts) {
                printf("%-30s", display_buffer[idx]);
            }
        }
        printf("\n");
    }
    printf("---------------------------------------------\n");
}

// Function to download a file
int download_file(const char* url, const char* output_path) {
    CURL *curl_handle;
    CURLcode res;
    FILE *fp;

    curl_handle = curl_easy_init();
    if (!curl_handle) {
        fprintf(stderr, "\033[0;31mFailed to initialize curl for download.\033[0m\n");
        return 1;
    }

    fp = fopen(output_path, "wb");
    if (!fp) {
        fprintf(stderr, "\033[0;31mFailed to open file for writing: %s\033[0m\n", output_path);
        curl_easy_cleanup(curl_handle);
        return 1;
    }

    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, NULL); // Use default write function
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl_handle, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirects
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");
    curl_easy_setopt(curl_handle, CURLOPT_TIMEOUT, 60L); // 60 seconds timeout
    curl_easy_setopt(curl_handle, CURLOPT_CONNECTTIMEOUT, 15L); // 15 seconds connect timeout

    res = curl_easy_perform(curl_handle);

    fclose(fp);
    curl_easy_cleanup(curl_handle);

    if (res != CURLE_OK) {
        fprintf(stderr, "\033[0;31mFailed to download %s: %s\033[0m\n", url, curl_easy_strerror(res));
        remove(output_path); // Clean up partial download
        return 1;
    }
    return 0;
}

int main() {
    char* pkg_manager_cmd = NULL;
    char* pkg_install_flags = NULL;
    char home_dir[256];
    snprintf(home_dir, sizeof(home_dir), "%s", getenv("HOME"));

    // 1. Detect OS and set package manager
    detect_os_and_set_package_manager(&pkg_manager_cmd, &pkg_install_flags);

    // 2. Check and install dependencies
    install_dependencies(pkg_manager_cmd, pkg_install_flags);

    // 3. Create directories
    char fonts_dir[512];
    char tmp_dir[512];
    snprintf(fonts_dir, sizeof(fonts_dir), "%s/.local/share/fonts", home_dir);
    snprintf(tmp_dir, sizeof(tmp_dir), "%s/tmp", home_dir);

    printf("\033[0;34mCreating directory: %s\033[0m\n", fonts_dir);
    if (mkdir(fonts_dir, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create fonts directory");
        exit(EXIT_FAILURE);
    }
    printf("\033[0;34mCreating directory: %s\033[0m\n", tmp_dir);
    if (mkdir(tmp_dir, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create tmp directory");
        exit(EXIT_FAILURE);
    }

    // 4. Fetch available fonts
    json_t *fonts_json_array = fetch_available_fonts();
    if (!fonts_json_array) {
        fprintf(stderr, "\033[0;31mFailed to fetch or parse fonts.\033[0m\n");
        exit(EXIT_FAILURE);
    }

    // 5. Display fonts and get user selection
    print_fonts_in_columns(fonts_json_array);

    printf("\033[0;36mEnter the numbers of the fonts to install (e.g., \"1 2 3\") or type \"all\" to install all fonts: \033[0m");
    char selection_input[1024];
    if (fgets(selection_input, sizeof(selection_input), stdin) == NULL) {
        fprintf(stderr, "\033[0;31mError reading input.\033[0m\n");
        json_decref(fonts_json_array);
        exit(EXIT_FAILURE);
    }
    selection_input[strcspn(selection_input, "\n")] = 0; // Remove newline

    json_t *selected_fonts_array = json_array();
    if (strcmp(selection_input, "all") == 0) {
        for (size_t i = 0; i < json_array_size(fonts_json_array); ++i) {
            json_t *font_obj = json_array_get(fonts_json_array, i);
            json_t *name_obj = json_object_get(font_obj, "name");
            if (json_is_string(name_obj)) {
                json_array_append_new(selected_fonts_array, json_string(json_string_value(name_obj)));
            }
        }
    } else {
        char *token = strtok(selection_input, " ");
        while (token != NULL) {
            int index = atoi(token);
            if (index > 0 && (size_t)index <= json_array_size(fonts_json_array)) {
                json_t *font_obj = json_array_get(fonts_json_array, index - 1);
                json_t *name_obj = json_object_get(font_obj, "name");
                if (json_is_string(name_obj)) {
                    json_array_append_new(selected_fonts_array, json_string(json_string_value(name_obj)));
                }
            } else {
                fprintf(stderr, "\033[0;31mInvalid selection: %s\033[0m\n", token);
            }
            token = strtok(NULL, " ");
        }
    }

    if (json_array_size(selected_fonts_array) == 0) {
        fprintf(stderr, "\033[0;31mNo valid fonts selected. Exiting.\033[0m\n");
        json_decref(fonts_json_array);
        json_decref(selected_fonts_array);
        exit(EXIT_FAILURE);
    }

    // 6. Download and install selected fonts
    size_t i;
    json_t *value;
    json_array_foreach(selected_fonts_array, i, value) {
        const char *font_name = json_string_value(value);
        printf("\033[0;34mDownloading and installing %s\033[0m\n", font_name);

        char download_url[1024];
        char zip_path[1024]; // Increased size
        snprintf(download_url, sizeof(download_url), "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/%s.zip", font_name);
        snprintf(zip_path, sizeof(zip_path), "%s/%s.zip", tmp_dir, font_name);

        // Check if font exists before downloading (simplified: just try to download)
        if (download_file(download_url, zip_path) == 0) {
            char unzip_command[2048]; // Increased size
            snprintf(unzip_command, sizeof(unzip_command), "unzip -o %s -d %s", zip_path, fonts_dir);
            if (execute_command(unzip_command) == 0) {
                printf("\033[0;32m%s installed successfully\033[0m\n", font_name);
            } else {
                fprintf(stderr, "\033[0;31mFailed to unzip %s.\033[0m\n", font_name);
            }
            remove(zip_path); // Clean up downloaded zip
        } else {
            fprintf(stderr, "\033[0;31mWarning: %s not found in releases or failed to download, skipping...\033[0m\n", font_name);
        }
    }

    // 7. Update font cache
    printf("\033[0;34mUpdating font cache...\033[0m\n");
    if (execute_command("fc-cache -vf") == 0) {
        printf("\033[0;32mFont installation complete!\033[0m\n");
    } else {
        fprintf(stderr, "\033[0;31mFailed to update font cache.\033[0m\n");
    }

    json_decref(fonts_json_array);
    json_decref(selected_fonts_array);
    free(pkg_manager_cmd);
    free(pkg_install_flags);

    return 0;
}
