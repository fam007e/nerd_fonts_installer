#!/bin/bash -e

# Function to detect the OS and set the package manager
detect_os_and_set_package_manager() {
    if [ -f /etc/os-release ]; then
        . /etc/os-release
        case "$ID" in
            ubuntu|debian|linuxmint|kali|deepin|devuan|mx|pop)
                PKG_MANAGER="sudo apt-get update && sudo apt-get install -y"
                ;;
            fedora)
                PKG_MANAGER="sudo dnf install -y"
                ;;
            centos|rhel)
                PKG_MANAGER="sudo yum install -y"
                ;;
            arch|manjaro|endeavouros|cachyos|garuda|artix|arco|steamos|blackarch)
                PKG_MANAGER="sudo pacman -Syu --noconfirm"
                ;;
            *)
                printf "%b\n" '\033[0;31mUnsupported OS: '"$ID"'\033[0m'
                exit 1
                ;;
        esac
    else
        printf "%b\n" '\033[0;31mOS detection failed. Please install curl, unzip, and fontconfig manually.\033[0m'
        exit 1
    fi
}

# Function to check and install dependencies
install_dependencies() {
    if ! command -v curl >/dev/null 2>&1; then
        printf "%b\n" '\033[0;33mcurl not found. Installing curl...\033[0m'
        $PKG_MANAGER curl
    fi

    if ! command -v unzip >/dev/null 2>&1; then
        printf "%b\n" '\033[0;33munzip not found. Installing unzip...\033[0m'
        $PKG_MANAGER unzip
    fi

    if ! command -v fc-cache >/dev/null 2>&1; then
        printf "%b\n" '\033[0;33mfontconfig (fc-cache) not found. Installing fontconfig...\033[0m'
        $PKG_MANAGER fontconfig
    fi
}

# Function to fetch available fonts from GitHub API
fetch_available_fonts() {
    printf "%b\n" '\033[0;33mFetching available fonts from GitHub...\033[0m'

    local api_response
    api_response=$(curl -s --connect-timeout 10 --max-time 30 \
        "https://api.github.com/repos/ryanoasis/nerd-fonts/contents/patched-fonts?ref=master")

    if [ $? -ne 0 ] || [ -z "$api_response" ]; then
        printf "%b\n" '\033[0;31mFailed to fetch font list from GitHub API. Please check your internet connection.\033[0m'
        exit 1
    fi

    # Parse JSON response to get font names
    fonts=($(echo "$api_response" | awk -F'"' '/name/ {print $4}' | sort))

    if [ ${#fonts[@]} -eq 0 ]; then
        printf "%b\n" '\033[0;31mNo fonts found in the API response. Please try again later.\033[0m'
        exit 1
    fi

    printf "%b\n" '\033[0;32mFound '"${#fonts[@]}"' available fonts\033[0m'
}

# Display menu of available fonts in multiple columns
print_fonts_in_columns() {
    local cols=$(tput cols)
    local items_per_col=20
    local total_fonts=${#fonts[@]}
    local columns=$((total_fonts / items_per_col))
    if ((total_fonts % items_per_col != 0)); then
        columns=$((columns + 1))
    fi

    for ((i=0; i<items_per_col; i++)); do
        for ((j=0; j<columns; j++)); do
            idx=$((i + j * items_per_col))
            if ((idx < total_fonts)); then
                printf "%-30s" "$((idx + 1)). ${fonts[idx]}"
            fi
        done
        echo
    done
}

# Cleanup function for temporary files
cleanup() {
    printf "%b\n" '\033[0;33mCleaning up temporary files...\033[0m'
    rm -rf "$HOME/tmp"/*.zip 2>/dev/null
}

# Trap interrupts to ensure cleanup
trap cleanup SIGINT SIGTERM

# Detect OS and set package manager, then install dependencies
detect_os_and_set_package_manager
install_dependencies

# Create directories
mkdir -p "$HOME/.local/share/fonts"
mkdir -p "$HOME/tmp"

# Fetch available fonts
fetch_available_fonts

# Display font list
printf "%b\n" '\033[0;32mSelect fonts to install (separate with spaces, or enter "all" to install all fonts):\033[0m'
printf "%b\n" "---------------------------------------------"
print_fonts_in_columns
printf "%b\n" "---------------------------------------------"

# Prompt user to select fonts and validate input
while true; do
    printf "%b\n" '\033[0;36mEnter the numbers of the fonts to install (e.g., "1 2 3") or type "all" to install all fonts: \033[0m'
    read -r font_selection < /dev/tty

    # Check if user selected "all"
    if [ "$font_selection" = "all" ]; then
        selected_fonts=("${fonts[@]}")
        break
    fi

    # Validate input
    if [ -z "$font_selection" ]; then
        printf "%b\n" '\033[0;31mError: Please select at least one font or type "all".\033[0m'
        continue
    fi

    # Check for valid numeric input
    valid_selection=true
    selected_fonts=()
    for selection in $font_selection; do
        # Check if selection is a number
        if ! [[ "$selection" =~ ^[0-9]+$ ]]; then
            printf "%b\n" '\033[0;31mError: Invalid input '"$selection"'. Please enter numbers or "all".\033[0m'
            valid_selection=false
            break
        fi

        font_index=$((selection - 1))
        if ((font_index >= 0 && font_index < ${#fonts[@]})); then
            selected_fonts+=("${fonts[font_index]}")
        else
            printf "%b\n" '\033[0;31mError: Selection '"$selection"' is out of range.\033[0m'
            valid_selection=false
            break
        fi
    done

    # Break loop if valid selection
    if [ "$valid_selection" = true ] && [ ${#selected_fonts[@]} -gt 0 ]; then
        break
    fi
done

# Download and install selected fonts
for font in "${selected_fonts[@]}"; do
    printf "%b\n" '\033[0;34mDownloading and installing '"$font"'\033[0m'
    font_name=$(printf "%b" "$font" | awk '{print $1}')

    # Check if font exists before downloading
    if curl -s --head --fail "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/$font_name.zip" >/dev/null 2>&1; then
        curl -sSLo "$HOME/tmp/$font_name.zip" "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/$font_name.zip"
        unzip -o "$HOME/tmp/$font_name.zip" -d "$HOME/.local/share/fonts" >/dev/null
        rm "$HOME/tmp/$font_name.zip"
        printf "%b\n" '\033[0;32m'"$font"' installed successfully\033[0m'
    else
        printf "%b\n" '\033[0;31mWarning: '"$font"' not found in releases, skipping...\033[0m'
    fi
done

# Update font cache only if at least one font was installed
if [ ${#selected_fonts[@]} -gt 0 ]; then
    printf "%b\n" '\033[0;34mUpdating font cache...\033[0m'
    fc-cache -f >/dev/null
    printf "%b\n" '\033[0;32mFont installation complete!\033[0m'
else
    printf "%b\n" '\033[0;31mNo fonts were installed.\033[0m'
fi

# Clean up temporary files
cleanup
