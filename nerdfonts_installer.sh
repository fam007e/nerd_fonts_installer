#!/bin/bash -e

PAGER="${PAGER:-less -R -X -F}"

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

# Fetch available fonts from the GitHub Releases API.
#
# FIX 1: Use releases/latest instead of the Contents API so the font list
#         exactly matches downloadable release assets — no more name mismatches.
#
# FIX 2: Capture the HTTP status code separately from curl's exit code.
#         The original only checked $? (curl network failure), so a rate-limit
#         403 — which curl returns as success — produced an error JSON that the
#         sed parser silently yielded zero results for.
fetch_available_fonts() {
    printf "%b\n" '\033[0;33mFetching available fonts from GitHub...\033[0m'

    local tmp_api
    tmp_api=$(mktemp "${TMPDIR:-/tmp}/nerdfonts_api.XXXXXX")

    local http_code
    http_code=$(curl -s -A "nerdfonts-installer/1.0" \
        --connect-timeout 10 --max-time 30 \
        -w "%{http_code}" \
        -o "$tmp_api" \
        "https://api.github.com/repos/ryanoasis/nerd-fonts/releases/latest")

    local curl_exit=$?
    if [ $curl_exit -ne 0 ]; then
        rm -f "$tmp_api"
        printf "%b\n" '\033[0;31mFailed to reach GitHub API. Check your internet connection.\033[0m'
        exit 1
    fi

    if [ "$http_code" != "200" ]; then
        rm -f "$tmp_api"
        printf "%b\n" '\033[0;31mGitHub API returned HTTP '"$http_code"'.\033[0m'
        if [ "$http_code" = "403" ] || [ "$http_code" = "429" ]; then
            printf "%b\n" '\033[0;33mRate-limited. Set GITHUB_TOKEN in your environment to raise the limit:\033[0m'
            printf "%b\n" '\033[0;33m  export GITHUB_TOKEN=ghp_...\033[0m'
        fi
        exit 1
    fi

    local api_response
    api_response=$(cat "$tmp_api")
    rm -f "$tmp_api"

    if [ -z "$api_response" ]; then
        printf "%b\n" '\033[0;31mEmpty response from GitHub API.\033[0m'
        exit 1
    fi

    # Extract font names from the release assets array.
    # Filter to .zip files; exclude FontPatcher.zip; strip .zip suffix for display.
    # jq is preferred for correctness; sed is the fallback for minimal systems.
    if command -v jq >/dev/null 2>&1; then
        mapfile -t fonts < <(printf '%s' "$api_response" \
            | jq -r '.assets[].name
                      | select(endswith(".zip"))
                      | select(. != "FontPatcher.zip")
                      | rtrimstr(".zip")' \
            | sort) || true
    else
        # Fallback: split on '{' so each asset object is on its own line,
        # then pull "name":"*.zip" values, drop FontPatcher, strip suffix.
        mapfile -t fonts < <(printf '%s' "$api_response" \
            | tr '{' '\n' \
            | sed -n 's/.*"name":"\([^"]*\.zip\)".*/\1/p' \
            | grep -v 'FontPatcher\.zip' \
            | sed 's/\.zip$//' \
            | sort) || true
    fi

    if [ ${#fonts[@]} -eq 0 ]; then
        printf "%b\n" '\033[0;31mNo fonts found in the release assets. Please try again later.\033[0m'
        exit 1
    fi

    printf "%b\n" '\033[0;32mFound '"${#fonts[@]}"' available fonts\033[0m'
}

# Display menu of available fonts in multiple columns
print_fonts_in_columns() {
    local term_width
    term_width=$(tput cols)
    local max_len=0
    for font in "${fonts[@]}"; do
        if (( ${#font} > max_len )); then
            max_len=${#font}
        fi
    done

    local col_width=$((max_len + 6))
    local columns=$((term_width / col_width))
    if (( columns == 0 )); then
        columns=1
    fi

    local total_fonts=${#fonts[@]}
    local rows=$(( (total_fonts + columns - 1) / columns ))

    for ((i=0; i<rows; i++)); do
        for ((j=0; j<columns; j++)); do
            local idx=$(( i + j * rows ))
            if (( idx < total_fonts )); then
                # FIX 3: was "%-$(echo $col_width)s" — pointless subshell.
                printf "%-${col_width}s" "$((idx + 1)). ${fonts[idx]}"
            fi
        done
        echo
    done
}

# Create temporary working directory
TMPWORKDIR=$(mktemp -d "${TMPDIR:-/tmp}/nerdfonts.XXXXXX")
if [ ! -d "$TMPWORKDIR" ]; then
    printf "%b\n" '\033[0;31mError: Failed to create temporary directory. Aborting.\033[0m'
    exit 1
fi

# FIX 4: Cleanup must not call exit 0.
# The original trap called cleanup which called exit 0, overriding any non-zero
# exit code set by an earlier "exit 1" (bash: exit inside an EXIT trap sets the
# final exit status). Separating the traps preserves the correct exit code on
# normal exits and sets meaningful codes for signals.
cleanup() {
    if [ -n "$TMPWORKDIR" ] && [ -d "$TMPWORKDIR" ]; then
        rm -rf "$TMPWORKDIR"
    fi
    TMPWORKDIR=""
}

trap cleanup EXIT
trap 'cleanup; exit 130' SIGINT
trap 'cleanup; exit 143' SIGTERM

# Detect OS and set package manager, then install dependencies
detect_os_and_set_package_manager
install_dependencies

# Create fonts directory
mkdir -p "$HOME/.local/share/fonts"

# Fetch available fonts from releases API
fetch_available_fonts

# Display font list
printf "%b\n" '\033[0;32mSelect fonts to install (separate with spaces, or enter "all" to install all fonts):\033[0m'
printf "%b\n" "---------------------------------------------"
print_fonts_in_columns | $PAGER
printf "%b\n" "---------------------------------------------"

# Prompt user to select fonts and validate input
while true; do
    printf "%b\n" '\033[0;36mEnter the numbers of the fonts to install (e.g., "1 2 3"), type "all" to install all fonts, or "list" to see the list again: \033[0m'
    read -r font_selection < /dev/tty

    # Check if user selected "all"
    if [ "$font_selection" = "all" ]; then
        selected_fonts=("${fonts[@]}")
        break
    fi

    # Check if user wants to see the list again
    if [ "$font_selection" = "list" ]; then
        print_fonts_in_columns | $PAGER
        continue
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

    if [ "$valid_selection" = true ] && [ ${#selected_fonts[@]} -gt 0 ]; then
        break
    fi
done

# Download and install selected fonts
installed_count=0
for font in "${selected_fonts[@]}"; do
    printf "%b\n" '\033[0;34mDownloading and installing '"$font"'\033[0m'

    # Font names from the releases API are already clean asset names;
    # no awk/printf extraction needed.
    font_name="$font"

    # HEAD check: asset existence is guaranteed by the releases listing, but
    # this guards against transient CDN errors before we create the zip file.
    if curl -s -A "nerdfonts-installer/1.0" --head --fail \
        "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/$font_name.zip" \
        >/dev/null 2>&1; then
        if curl -sSL -A "nerdfonts-installer/1.0" \
            -o "$TMPWORKDIR/$font_name.zip" \
            "https://github.com/ryanoasis/nerd-fonts/releases/latest/download/$font_name.zip"; then
            if unzip -o "$TMPWORKDIR/$font_name.zip" -d "$HOME/.local/share/fonts" >/dev/null; then
                rm -f "$TMPWORKDIR/$font_name.zip"
                printf "%b\n" '\033[0;32m'"$font"' installed successfully\033[0m'
                installed_count=$((installed_count + 1))
            else
                printf "%b\n" '\033[0;31mWarning: Failed to extract '"$font"', skipping...\033[0m'
                rm -f "$TMPWORKDIR/$font_name.zip"
            fi
        else
            printf "%b\n" '\033[0;31mWarning: Failed to download '"$font"', skipping...\033[0m'
        fi
    else
        printf "%b\n" '\033[0;31mWarning: '"$font"' not reachable in releases, skipping...\033[0m'
    fi
done

# Update font cache only if at least one font was installed
if [ "$installed_count" -gt 0 ]; then
    printf "%b\n" '\033[0;34mUpdating font cache...\033[0m'
    fc-cache -f >/dev/null
    printf "%b\n" '\033[0;32mFont installation complete! '"$installed_count"' font(s) installed.\033[0m'
else
    printf "%b\n" '\033[0;31mNo fonts were installed.\033[0m'
fi

# EXIT trap calls cleanup automatically; no explicit call needed here.
