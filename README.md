# 🚀 Nerd Fonts Installer

> An interactive installer for [Nerd Fonts](https://github.com/ryanoasis/nerd-fonts) with automatic distribution detection and dependency management.

[![GitHub stars](https://img.shields.io/github/stars/fam007e/nerd_fonts_installer?style=for-the-badge&logo=github)](https://github.com/fam007e/nerd_fonts_installer)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![Release Binary](https://github.com/fam007e/nerd_fonts_installer/actions/workflows/release.yml/badge.svg)](https://github.com/fam007e/nerd_fonts_installer/actions/workflows/release.yml)
[![Packaging status](https://repology.org/badge/vertical-allrepos/nerdfonts-installer.svg)](https://repology.org/project/nerdfonts-installer/versions)

---

## ✨ Features

- **🐧 Cross-platform Support** - Works on Arch, Debian, Ubuntu, Fedora, CentOS, RHEL, and Linux Mint
- **📦 Automatic Dependencies** - Installs `curl`, `unzip`, and `fontconfig` automatically
- **🔍 Live Font Discovery** - Fetches current font list from Nerd Fonts **Releases API** (more reliable, no more 404 mismatches)
- **🎯 Interactive Selection** - Choose specific fonts or install all with one command
- **🏠 Smart Installation** - Installs to `~/.local/share/fonts` with automatic cache updates
- **🎨 Beautiful Interface** - Colorful terminal output for better user experience
- **🛡️ Robust Error Handling** - Comprehensive error checking and recovery
- **🔒 Secure by Design** - Input validation, memory safety (C version), and hardened binary compilation
- **⚡ High Performance** - C version optimized for speed and memory efficiency

---

## 🚀 Quick Start
> [!IMPORTANT]
> **Security Notice**: Please ensure you are using version `v2026.06` (or newer). Older versions using the old Contents API are deprecated.

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
❯ ./nerdfonts-installer
🚀 Nerd Fonts Installer
-------------------------------------------------------------------------------------------------------------------------------------------------
Detected OS: arch
✓ All dependencies are installed
Fetching available fonts from GitHub...
Found 70 available fonts
Select fonts to install (space-separated numbers, or "all"):
-------------------------------------------------------------------------------------------------------------------------------------------------
1. 0xProto                      15. CommitMono                  29. Hack                        43. Lekton                      57. ProFont
2. 3270                         16. Cousine                     30. Hasklig                     44. LiberationMono              58. ProggyClean
3. AdwaitaMono                  17. D2Coding                    31. HeavyData                   45. Lilex                       59. Recursive
4. Agave                        18. DaddyTimeMono               32. Hermit                      46. MartianMono                 60. RobotoMono
5. AnonymousPro                 19. DejaVuSansMono              33. iA-Writer                   47. Meslo                       61. ShareTechMono
6. Arimo                        20. DepartureMono               34. IBMPlexMono                 48. Monaspace                   62. SourceCodePro
7. AtkinsonHyperlegibleMono     21. DroidSansMono               35. Inconsolata                 49. Monofur                     63. SpaceMono
8. AurulentSansMono             22. EnvyCodeR                   36. InconsolataGo               50. Monoid                      64. Terminus
9. BigBlueTerminal              23. FantasqueSansMono           37. InconsolataLGC              51. Mononoki                    65. Tinos
10. BitstreamVeraSansMono       24. FiraCode                    38. IntelOneMono                52. MPlus                       66. Ubuntu
11. CascadiaCode                25. FiraMono                    39. Iosevka                     53. NerdFontsSymbolsOnly        67. UbuntuMono
12. CascadiaMono                26. GeistMono                   40. IosevkaTerm                 54. Noto                        68. UbuntuSans
13. CodeNewRoman                27. Go-Mono                     41. IosevkaTermSlab             55. OpenDyslexic                69. VictorMono
14. ComicShannsMono             28. Gohu                        42. JetBrainsMono               56. Overpass                    70. ZedMono
-------------------------------------------------------------------------------------------------------------------------------------------------
Enter font numbers (e.g. "1 2 3") or "all":

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

| Distribution | Package Manager | Shell Script | C Binary | Notes |
|:-------------|:----------------|:------------:|:--------:|:------|
| **Arch Linux** | `pacman` | ✅ | ✅ | AUR package available |
| **Manjaro** | `pacman` | ✅ | ✅ | Full compatibility |
| **EndeavourOS** | `pacman` | ✅ | ✅ | Full compatibility |
| **Ubuntu 22.04+** | `apt-get` | ✅ | ✅ | GLIBC 2.35+ required for binary |
| **Ubuntu < 22.04** | `apt-get` | ✅ | ⚠️ | Use Shell Script (Binary may fail) |
| **Debian 12+** | `apt-get` | ✅ | ✅ | Stable and testing |
| **Debian < 12** | `apt-get` | ✅ | ⚠️ | Use Shell Script (Binary may fail) |
| **Linux Mint 21+** | `apt-get` | ✅ | ✅ | All versions |
| **Fedora 34+** | `dnf` | ✅ | ✅ | Recent versions |
| **CentOS 7/8** | `yum` | ✅ | ❌ | **Use Shell Script** (Binary requires new GLIBC) |
| **RHEL 7/8/9** | `yum` | ✅ | ⚠️ | RHEL 9 OK, older usage Shell Script |

> **Note**: The C Binary is built on Ubuntu Latest and requires a recent GLIBC. For older distributions (CentOS 7, Ubuntu 18.04, etc.), please use the **Shell Script** version.

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

### ⚡ Performance & Security
| Version | Dependencies | Speed | Memory | Security | verification |
|:--------|:-------------|:------|:-------|:----------|:-------------|
| **C Binary** | libcurl, libjansson | 🔥 Fast | 💚 Low | 🛡️ **Hardened** (PIE, Full RELRO, Canary, FORTIFY_SOURCE=2) | ✅ **Verified** (ASan, MSan, CodeQL, Flawfinder) |
| **Shell Script** | bash, curl, unzip | 🐌 Slower | 🟡 Higher | ⚠️ Basic | ❌ Manual Check Only |

### 🔒 Security Measures

The C version is built with a defense-in-depth approach:
- **Sanitizers**: Continuous testing with AddressSanitizer (ASan), MemorySanitizer (MSan), and ThreadSanitizer (TSan).
- **Static Analysis**: Scanned weekly with CodeQL, CppCheck, and Flawfinder.
- **Hardening**: Compiled with `-fstack-protector-strong`, `-fPIE`, `-Wl,-z,relro,-z,now` (Full RELRO), `-Wl,-z,noexecstack`, and `-D_FORTIFY_SOURCE=2`.

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
├── 📄 LICENSE                  # MIT license
├── 📄 README.md                # Documentation
├── 📄 CONTRIBUTING.md          # Contribution guidelines and CI requirements
├── 📄 SECURITY.md              # Security policy and vulnerability reporting
└── 📄 CODE_OF_CONDUCT.md       # Community standards
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

Thank you to everyone who has contributed to the development of `nerd_fonts_installer`. Your efforts are greatly appreciated, and you're helping make this tool better for everyone!

[![Contributors](https://contrib.rocks/image?repo=fam007e/nerd_fonts_installer)](https://github.com/fam007e/nerd_fonts_installer/graphs/contributors)


---

## 📞 Support & Links

- 🐛 **Issues**: [GitHub Issues](https://github.com/fam007e/nerd_fonts_installer/issues)
- 📦 **AUR Package**: [nerdfonts-installer](https://aur.archlinux.org/packages/nerdfonts-installer-bin)
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
