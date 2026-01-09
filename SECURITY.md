# Security Policy

## Supported Versions

Use this section to tell people about which versions of your project are
currently being supported with security updates.

| Version | Supported          |
| ------- | ------------------ |
| v2026.x.x.x | :white_check_mark: |
| v2025.x.x.x | :white_check_mark: |
| < v2025.x.x.x | :x:                |

## Security Measures

We employ a comprehensive set of automated security checks to prevent vulnerabilities:

*   **Static Application Security Testing (SAST)**:
    *   **CodeQL**: Runs on every push to detect semantic security flaws.
    *   **Flawfinder**: Scans C/C++ code for potential security risks.
    *   **CppCheck**: Checks for undefined behavior and dangerous coding constructs.
    *   **Clang Static Analyzer**: Performs deep analysis of code paths.

*   **Dynamic Analysis**:
    *   **AddressSanitizer (ASan)**: Detects memory corruption bugs (buffer overflows, use-after-free).
    *   **MemorySanitizer (MSan)**: Detects use of uninitialized memory.
    *   **ThreadSanitizer (TSan)**: Detects data races in concurrent code.

*   **Compiler Hardening**:
    *   Executables are built with PIE, Stack Canaries, and Full RELRO.

## Reporting a Vulnerability

We take the security of `nerd_fonts_installer` seriously. If you have found a security vulnerability, please do **not** open a public issue.

### How to Report

Please report security vulnerabilities by using the **"Report a vulnerability"** button in the **Security** tab of this repository, if available. This allows for a private disclosure and collaboration process.

If that option is not available, please contact the repository owner directly or open an issue asking for a private communication channel without disclosing the details of the vulnerability.

### What to Include

When reporting a vulnerability, please include as much information as possible to help us reproduce and fix the issue:

*   The version of the project you are using.
*   The operating system and environment.
*   Steps to reproduce the vulnerability.
*   A description of the impact.

### Response

We will acknowledge your report and work to investigate and resolve the issue as quickly as possible. We appreciate your responsible disclosure and your help in making this project more secure.
