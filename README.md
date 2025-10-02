# 🚀 Nerd Fonts Installer

> An interactive installer for [Nerd Fonts](https://github.com/ryanoasis/nerd-fonts) with automatic distribution detection and dependency management.

[![GitHub stars](https://img.shields.io/github/stars/fam007e/nerd_fonts_installer?style=for-the-badge&logo=github)](https://github.com/fam007e/nerd_fonts_installer)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![Packaging status](https://repology.org/badge/vertical-allrepos/nerdfonts-installer.svg)](https://repology.org/project/nerdfonts-installer/versions)

---

## ✨ Features

- **🐧 Cross-platform Support** - Works on Arch, Debian, Ubuntu, Fedora, CentOS, RHEL, and Linux Mint
- **📦 Automatic Dependencies** - Installs `curl`, `unzip`, and `fontconfig` automatically
- **🔍 Live Font Discovery** - Fetches current font list from Nerd Fonts GitHub API
- **🎯 Interactive Selection** - Choose specific fonts or install all with one command
- **🏠 Smart Installation** - Installs to `~/.local/share/fonts` with automatic cache updates
- **🎨 Beautiful Interface** - Colorful terminal output for better user experience
- **🛡️ Robust Error Handling** - Comprehensive error checking and recovery
- **⚡ High Performance** - C version optimized for speed and memory efficiency

---

## 🚀 Quick Start

### One-Line Installation (Recommended)

```bash
curl -sSLo nerdfonts-installer https://github.com/fam007e/nerd_fonts_installer/releases/latest/download/nerdfonts-installer && chmod +x nerdfonts-installer && sudo mv nerdfonts-installer /usr/local/bin/
```

Then run:
```bash
nerdfonts-installer
```

---

## 📥 Installation Methods

### 🎯 Method 1: AUR (Arch Linux Users)

<details>
<summary>Click to expand AUR installation options</summary>

```bash
# Using yay (recommended)
yay -S nerdfonts-installer-bin

# Using paru
paru -S nerdfonts-installer-bin

# Manual installation
git clone https://aur.archlinux.org/nerdfonts-installer-bin.git
cd nerdfonts-installer-bin
makepkg -si
```
</details>

### 📦 Method 2: Pre-built Binary

```bash
# Download and install
curl -sSLo nerdfonts-installer https://github.com/fam007e/nerd_fonts_installer/releases/latest/download/nerdfonts-installer
chmod +x nerdfonts-installer
sudo mv nerdfonts-installer /usr/local/bin/
```

### 🔨 Method 3: Build from Source

<details>
<summary>Click to expand build instructions</summary>

**Install build dependencies:**

```bash
# Arch Linux / Manjaro
sudo pacman -S gcc make curl jansson

# Ubuntu / Debian / Linux Mint
sudo apt-get install build-essential libcurl4-openssl-dev libjansson-dev

# Fedora
sudo dnf install gcc make libcurl-devel jansson-devel

# CentOS / RHEL
sudo yum install gcc make libcurl-devel jansson-devel
```

**Build and install:**

```bash
git clone https://github.com/fam007e/nerd_fonts_installer.git
cd nerd_fonts_installer
make
sudo cp nerdfonts-installer /usr/local/bin/
```
</details>

---

## 🎮 Usage

### Interactive Mode

Simply run:
```bash
nerdfonts-installer
```

### Example Session

```bash
$ nerdfonts-installer
Detected OS: arch, Package Manager: sudo pacman
Creating directory: /home/fam007e/.local/share/fonts
Creating directory: /home/fam007e/tmp
Fetching available fonts from GitHub...
Found 70 available fonts
Select fonts to install (separate with spaces, or enter "all" to install all fonts):
---------------------------------------------
1. 0xProto                    21. DroidSansMono             41. JetBrainsMono             61. SourceCodePro
2. 3270                       22. EnvyCodeR                 42. Lekton                    62. SpaceMono
3. AdwaitaMono                23. FantasqueSansMono         43. LiberationMono            63. Terminus
4. Agave                      24. FiraCode                  44. Lilex                     64. Tinos
5. AnonymousPro               25. FiraMono                  45. MPlus                     65. Ubuntu
6. Arimo                      26. GeistMono                 46. MartianMono               66. UbuntuMono
7. AtkinsonHyperlegibleMono   27. Go-Mono                   47. Meslo                     67. UbuntuSans
8. AurulentSansMono           28. Gohu                      48. Monaspace                 68. VictorMono
9. BigBlueTerminal            29. Hack                      49. Monofur                   69. ZedMono
10. BitstreamVeraSansMono     30. Hasklig                   50. Monoid                    70. iA-Writer
11. CascadiaCode              31. HeavyData                 51. Mononoki
12. CascadiaMono              32. Hermit                    52. NerdFontsSymbolsOnly
13. CodeNewRoman              33. IBMPlexMono               53. Noto
14. ComicShannsMono           34. Inconsolata               54. OpenDyslexic
15. CommitMono                35. InconsolataGo             55. Overpass
16. Cousine                   36. InconsolataLGC            56. ProFont
17. D2Coding                  37. IntelOneMono              57. ProggyClean
18. DaddyTimeMono             38. Iosevka                   58. Recursive
19. DejaVuSansMono            39. IosevkaTerm               59. RobotoMono
20. DepartureMono             40. IosevkaTermSlab           60. ShareTechMono
---------------------------------------------
Enter the numbers of the fonts to install (e.g., "1 2 3") or type "all" to install all fonts:
```

---

## 🖥️ Alternative: Shell Script Version

For minimal dependencies or systems without build tools:

```bash
# Run directly (no installation required)
curl -sSL https://raw.githubusercontent.com/fam007e/nerd_fonts_installer/main/nerdfonts_installer.sh | bash

# Or download first
wget https://raw.githubusercontent.com/fam007e/nerd_fonts_installer/main/nerdfonts_installer.sh
chmod +x nerdfonts_installer.sh
./nerdfonts_installer.sh
```

---

## 🐧 Supported Distributions

| Distribution | Package Manager | Status | Notes |
|:-------------|:----------------|:------:|:------|
| **Arch Linux** | `pacman` | ✅ | AUR package available |
| **Manjaro** | `pacman` | ✅ | Full compatibility |
| **EndeavourOS** | `pacman` | ✅ | Full compatibility |
| **Ubuntu** | `apt-get` | ✅ | All LTS versions |
| **Debian** | `apt-get` | ✅ | Stable and testing |
| **Linux Mint** | `apt-get` | ✅ | All versions |
| **Fedora** | `dnf` | ✅ | Recent versions |
| **CentOS** | `yum` | ✅ | 7, 8, Stream |
| **RHEL** | `yum` | ✅ | 7, 8, 9 |

> **Note**: Other distributions may work but are not officially tested.

---

## 🔧 Technical Details

### 📋 Dependencies

<details>
<summary>Runtime Dependencies</summary>

- **`curl`** - Downloads fonts and makes API requests
- **`unzip`** - Extracts font archives
- **`fontconfig`** - Manages font cache and detection

*All dependencies are installed automatically if missing.*
</details>

<details>
<summary>Build Dependencies (C version)</summary>

- **`gcc`** - GNU Compiler Collection
- **`make`** - Build automation
- **`libcurl-dev`** - HTTP client library
- **`libjansson-dev`** - JSON parsing library
</details>

### 📁 Font Installation

Fonts are installed to `~/.local/share/fonts/` following XDG specifications:

- ✅ **No root required** - User-local installation
- ✅ **Automatic detection** - Scanned by fontconfig
- ✅ **Standard location** - Compatible with all applications
- ✅ **Easy management** - Simple to backup or remove

### ⚡ Performance Comparison

| Version | Dependencies | Speed | Memory | Error Handling | JSON Parsing | Recommended Use |
|:--------|:-------------|:------|:-------|:---------------|:-------------|:----------------|
| **C Binary** | libcurl, libjansson | 🔥 Fast | 💚 Low | 🛡️ Advanced | 🚀 Native | Production, Daily use |
| **Shell Script** | bash, curl, unzip | 🐌 Slower | 🟡 Higher | ⚠️ Basic | 🔧 awk-based | Testing, Quick installs |

---

## 🛠️ Development

### Building Locally

```bash
# Clone the repository
git clone https://github.com/fam007e/nerd_fonts_installer.git
cd nerd_fonts_installer

# Build the C version
make

# Test the build
./nerdfonts-installer

# Clean build artifacts
make clean
```

### 📁 Project Structure

```
📦 nerd_fonts_installer/
├── 📄 nerdfonts_installer.c    # Main C implementation
├── 📄 nerdfonts_installer.sh   # Shell script version
├── 📄 Makefile                 # Build configuration
├── 📄 LICENSE                 # MIT license
└── 📄 README.md               # Documentation
```

---

## 🤝 Contributing

We welcome contributions! Here's how you can help:

### 🐛 Report Issues
Found a bug? [Open an issue](https://github.com/fam007e/nerd_fonts_installer/issues) with:
- Steps to reproduce
- Expected vs actual behavior
- System information (OS, version)

### 💡 Feature Requests
Have an idea? We'd love to hear it! Include:
- Use case description
- Proposed solution
- Benefits to users

### 🔧 Code Contributions

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes (`git commit -m 'Add amazing feature'`)
4. **Push** to the branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### 📋 Development Guidelines

- **C code**: Follow C11 standard with comprehensive error handling
- **Shell scripts**: Maintain POSIX compliance where possible
- **Testing**: Verify functionality across multiple distributions
- **Documentation**: Update README for user-facing changes

---

## 📜 License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

---

## 🙏 Credits & Acknowledgments

- **🎨 Nerd Fonts**: Huge thanks to [@ryanoasis](https://github.com/ryanoasis) for the incredible [Nerd Fonts](https://github.com/ryanoasis/nerd-fonts) project
- **🔌 GitHub API**: For providing reliable font metadata
- **🐧 Linux Community**: For inspiration and feedback

---

## 📞 Support & Links

- 🐛 **Issues**: [GitHub Issues](https://github.com/fam007e/nerd_fonts_installer/issues)
- 📦 **AUR Package**: [nerdfonts-installer](https://aur.archlinux.org/packages/nerdfonts-installer)
- 📋 **Releases**: [Latest Release](https://github.com/fam007e/nerd_fonts_installer/releases/latest)
- 📖 **Documentation**: [Wiki](https://github.com/fam007e/nerd_fonts_installer/wiki)

---

<div align="center">

### 🌟 Star History

[![Star History Chart](https://api.star-history.com/svg?repos=fam007e/nerd_fonts_installer&type=Date)](https://star-history.com/#fam007e/nerd_fonts_installer&Date)

---

**💖 Made with love for the Linux community**

*If this project helped you, please consider giving it a ⭐!*

[⬆️ Back to top](#-nerd-fonts-installer)

</div>
