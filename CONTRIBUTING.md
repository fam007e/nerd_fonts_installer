# Contributing to Nerd Fonts Installer

First off, thanks for taking the time to contribute! 🎉

The following is a set of guidelines for contributing to Nerd Fonts Installer. These are mostly guidelines, not rules. Use your best judgment, and feel free to propose changes to this document in a pull request.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [How Can I Contribute?](#how-can-i-contribute)
  - [Reporting Bugs](#reporting-bugs)
  - [Suggesting Enhancements](#suggesting-enhancements)
  - [Your First Code Contribution](#your-first-code-contribution)
  - [Pull Requests](#pull-requests)
- [Styleguides](#styleguides)
  - [C Code](#c-code)
  - [Shell Scripts](#shell-scripts)
  - [Commit Messages](#commit-messages)

## Code of Conduct

This project and everyone participating in it is governed by the [Nerd Fonts Installer Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code. Please report unacceptable behavior to the project maintainers.

## How Can I Contribute?

### Reporting Bugs

This section guides you through submitting a bug report. Following these guidelines helps maintainers and the community understand your report, reproduce the behavior, and find related reports.

- **Use a clear and descriptive title** for the issue to identify the problem.
- **Describe the exact steps which reproduce the problem** in as much detail as possible.
- **Provide specific examples** to demonstrate the steps.
- **Describe the behavior you observed after following the steps** and point out what exactly is the problem with that behavior.
- **Explain which behavior you expected to see instead and why.**
- **Include details about your configuration and environment**:
  - Which OS and version are you using? (e.g., Ubuntu 22.04, Arch Linux)
  - Which version of the installer are you using?
  - Any relevant logs or terminal output.

### Suggesting Enhancements

This section guides you through submitting an enhancement suggestion, including completely new features and minor improvements to existing functionality.

- **Use a clear and descriptive title** for the issue to identify the suggestion.
- **Provide a step-by-step description of the suggested enhancement** in as much detail as possible.
- **Explain why this enhancement would be useful** to most users.

### Your First Code Contribution

Unsure where to begin contributing? You can start by looking through these `good first issue` and `help wanted` labels:

- [Good First Issue](https://github.com/fam007e/nerd_fonts_installer/labels/good%20first%20issue) - issues which should only require a few lines of code, and a test or two.
- [Help Wanted](https://github.com/fam007e/nerd_fonts_installer/labels/help%20wanted) - issues which may be a bit more involved than a simple bug fix.

### Pull Requests

1. Fork the repo and create your branch from `main`.
2. If you've added code that should be tested, add tests.
3. Ensure the test suite passes.
4. Make sure your code lints.
5. Update the documentation (like the `README.md`) if appropriate.
6. Open a Pull Request pointing to the `main` branch.

## Continuous Integration & Quality Standards

This repository enforces strict quality and security standards. All Pull Requests must pass the following checks to be eligible for merging.

### Branch Protection Rules

- **Linear History**: Merge commits are blocked. Please rebase your branch on `main` before submitting.
- **Strict Status Checks**: All CI jobs listed below must pass.
- **No Force Pushes**: Force pushing to `main` is disabled.

### Mandatory CI Checks

We recommend running these checks locally to identify issues early.

#### 1. Security Sanitizers
The codebase is tested against multiple sanitizers to detect memory and threading errors.

*   **AddressSanitizer (ASan)**: Detects buffer overflows and use-after-free.
    ```bash
    gcc -fsanitize=address -g -O1 -o nerdfonts_installer_asan nerdfonts_installer.c $(pkg-config --libs libcurl jansson)
    ```

*   **MemorySanitizer (MSan)**: Detects uninitialized memory reads (requires Clang).
    ```bash
    clang -fsanitize=memory -fno-omit-frame-pointer -g -O1 -o nerdfonts_installer_msan nerdfonts_installer.c $(pkg-config --libs libcurl jansson)
    ```

*   **ThreadSanitizer (TSan)**: Detects data races.
    ```bash
    gcc -fsanitize=thread -g -O1 -o nerdfonts_installer_tsan nerdfonts_installer.c $(pkg-config --libs libcurl jansson)
    ```

#### 2. Static Analysis Tools
We use a suite of static analysis tools to maintain code quality.

*   **CppCheck**:
    ```bash
    cppcheck --enable=all --inconclusive --force --suppress=missingIncludeSystem .
    ```

*   **Flawfinder**:
    ```bash
    # Install: pip install flawfinder
    flawfinder --minlevel=1 --context ./
    ```

*   **Clang Static Analyzer**:
    ```bash
    # Requires clang-tools
    scan-build gcc -Wall -Wextra -o nerdfonts_installer nerdfonts_installer.c $(pkg-config --cflags --libs libcurl jansson)
    ```

*   **CodeQL**: Runs automatically on GitHub. Ensure your code does not introduce taint tracking paths (e.g., user input reaching file system APIs without sanitization).

## Styleguides

### C Code

- **Standard**: Follow the C11 standard.
- **Security**:
  - Avoid `system()` calls; use `fork`/`exec` family functions for subprocesses.
  - Validate all external inputs (filenames, user selection, API responses).
  - Use safe string functions (e.g., `snprintf` instead of `sprintf`).
- **Error Handling**: Implement comprehensive error checking for all system calls and library functions.
- **Memory Management**: Ensure all allocated memory is properly freed. Use tools like Valgrind to check for leaks.
- **Formatting**: Consistent indentation (recommend 4 spaces) and bracketing style.

### Shell Scripts

- **Compatibility**: Maintain POSIX compliance where possible.
- **Shebang**: Use `#!/bin/bash` or `#!/bin/sh` as appropriate.
- **Error Handling**: Use logical operators (`&&`, `||`) or `set -e` where strict error handling is needed.
- **Comments**: Comment complex logic to ensure maintainability.

### Commit Messages

- Use the present tense ("Add feature" not "Added feature").
- Use the imperative mood ("Move cursor to..." not "Moves cursor to...").
- Limit the first line to 72 characters or less.
- Reference issues and pull requests liberally after the first line.

## Development Setup

To set up your local development environment:

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/fam007e/nerd_fonts_installer.git
    cd nerd_fonts_installer
    ```

2.  **Check and install dependencies:**
    ```bash
    make check-deps
    # See README.md for distribution-specific installation commands if deps are missing
    ```

3.  **Build the project:**
    ```bash
    make
    # Optional: Verify security features
    make verify-security
    ```

4.  **Run the installer:**
    ```bash
    ./nerdfonts-installer
    ```

Thank you for contributing!
