# LinSCP

**LinSCP** is a cross-platform SFTP/SSH file manager for Linux, built with C++20 and Qt 6.
It aims to bring the familiar WinSCP experience to Linux and other Unix-like systems.


## Features

- **Dual-pane interface** — local and remote file browsers side by side, with resizable QSplitter
- **Multiple connection tabs** — each tab has its own independent SSH session (Ctrl+T / Ctrl+W)
- **SFTP file operations** — browse, upload, download, rename, delete, chmod, mkdir; recursive copy of directories
- **Drag & drop between panels** — drag local files to remote and vice versa; a CopyDialog confirms the target path
- **WinSCP-style context menus** — Upload/Download (F5), Move (F6), Rename (F2), Delete, New Folder (F7), Open in Terminal, Properties
- **Background transfers** — "Download to Queue" enqueues files without showing a dialog
- **File conflict dialog** — Overwrite / Overwrite All / Skip / Skip All / Rename when a target file already exists
- **Transfer progress** — per-file and total progress bar, speed, ETA; Background button hides the dialog without cancelling
- **SSH channel multiplexing** — SFTP and terminal share a single SSH connection
- **Authentication** — password, public key (RSA/Ed25519/ECDSA), SSH agent; progress dialog shows the auth log
- **Host fingerprint verification** — Accept / Accept Once / Reject for unknown or changed host keys
- **Session manager (Login dialog)** — save/load profiles with AES-256-GCM encrypted passwords; folder hierarchy; WinSCP-style tree
- **Path state persistence** — each session reopens at the last visited local and remote directory
- **WinSCP session import** — import from WinSCP.ini (host, port, user, key, paths, folder structure)
- **SSH key manager** — generate RSA/Ed25519/ECDSA keys, import, and convert PuTTY PPK → OpenSSH
- **Transfer queue** — parallel transfers with progress, speed, and ETA display; thread-safe (deadlock-free)
- **Directory synchronization** — 3-page wizard: configure → preview diff → progress; directions: Local→Remote / Remote→Local / Bidirectional; compare by mtime+size or SHA-256 checksum; conflict detection; saved sync profiles
- **Remote free space** — statusbar shows available disk space (via `df -P`, async)
- **Show hidden files** — toggle with Ctrl+Alt+H; persisted in settings
- **External terminal** — opens the current session in PuTTY, kitty, xterm, Konsole, or any custom emulator (auto-detected; configured in Preferences)
- **Preferences dialog** — UI language, terminal emulator selection, max concurrent transfers
- **Russian localization** — full Russian translation of all dialogs and menus
- **Known hosts management** — verify and trust remote host keys (`~/.config/linscp/known_hosts`)

## Screenshots

> Coming soon — the project is in early development (v0.1-dev).

## Download & Install

Pre-built packages are available on the [Releases](https://github.com/LinCSP/LinSCP/releases) page.

| Platform | Package | Notes |
|----------|---------|-------|
| Linux (any) | `.AppImage` | Make executable, run directly |
| Ubuntu / Debian | `.deb` | `sudo dpkg -i linscp_*.deb` |
| Fedora / RHEL | `.rpm` | `sudo rpm -i linscp-*.rpm` |
| Windows 10/11 | `.exe` | Standard installer, SmartScreen may warn — click "More info → Run anyway" |
| macOS 13+ (Apple Silicon) | `.dmg` | See note below |

### macOS

LinSCP is not notarized (Apple charges $99/year for a Developer account).
macOS will block the app with *"LinSCP is damaged and can't be opened"*.

Run this once in Terminal after downloading the `.dmg`, then open it normally:

```bash
xattr -rd com.apple.quarantine LinSCP-*.dmg
```

## Requirements

| Dependency | Minimum version |
|------------|----------------|
| C++ compiler | GCC 11 / Clang 14 (C++20) |
| CMake | 3.25 |
| Qt | 6.5 |
| libssh | 0.10 |
| OpenSSL | 3.0 |

Optional:
- **libsecret** — GNOME Keyring / KWallet integration (`-DWITH_KEYRING=ON`)
- **sshpass** — required for password-based authentication in the external terminal (key/agent auth works without it)

## Building

```bash
# Install dependencies (Arch Linux)
sudo pacman -S qt6-base cmake libssh openssl ninja sshpass

# Install dependencies (Ubuntu 24.04 / Debian 12)
sudo apt install qt6-base-dev cmake libssh-dev libssl-dev ninja-build sshpass

# Install dependencies (Fedora 39+)
sudo dnf install qt6-qtbase-devel cmake libssh-devel openssl-devel ninja-build sshpass

# Clone and build
git clone https://github.com/LinCSP/LinSCP.git
cd linscp
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/linscp
```

### Optional features

```bash
# With system keyring (GNOME/KDE)
cmake -B build -G Ninja -DWITH_KEYRING=ON
```


## WinSCP import

**Tools → Import from WinSCP…** in the Login dialog accepts a `WinSCP.ini` file.

Imported fields per session:
- Host, port, username
- Protocol (SCP / SFTP)
- Public key file path (Windows path converted to Linux)
- Initial remote and local directories
- Folder hierarchy (e.g. `Production/Web`)

Passwords are **not** imported — they are stored encrypted with a WinSCP-specific key and cannot be decoded. You will be prompted for a password or key on first connect.

## Terminal

LinSCP does not embed a terminal emulator. Instead, it launches your preferred terminal with the SSH connection parameters pre-filled:

| Terminal | Auto-detected |
|----------|--------------|
| PuTTY    | yes          |
| kitty    | yes          |
| xterm    | yes          |
| Konsole  | yes          |
| GNOME Terminal | yes    |
| xfce4-terminal | yes    |
| Alacritty | yes         |
| Custom   | via Preferences |

Press **F9** or use **Session → Open Terminal** to open the terminal for the current session.

### Password authentication

When connecting with a password (not a key or SSH agent), LinSCP uses **`sshpass`** to pass the
password to the SSH process non-interactively. Without `sshpass`, the terminal will open but SSH
will prompt for the password manually.

Install `sshpass` if it is not already present:

```bash
# Arch Linux
sudo pacman -S sshpass

# Ubuntu / Debian
sudo apt install sshpass

# Fedora / RHEL
sudo dnf install sshpass
```

Key-based and SSH agent authentication do not require `sshpass`.

## Running tests

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

- [x] Project skeleton with full C++20/Qt6 source layout
- [x] SSH session state machine + channel multiplexing
- [x] SFTP client (ls, upload, download, rename, rm, mkdir, chmod, recursive copy)
- [x] Transfer queue and manager (parallel, thread-safe, deadlock-free)
- [x] Session profile store with AES-256-GCM encryption
- [x] SSH key manager and generator (RSA, Ed25519, ECDSA; PPK → OpenSSH)
- [x] Directory synchronization (mtime+size and SHA-256 checksum; conflict detection; saved profiles)
- [x] Dual-pane Qt UI with multiple connection tabs
- [x] Path state persistence — sessions reopen at the last visited directory
- [x] WinSCP session import (host, auth, paths, folder structure)
- [x] Drag & drop between panels (local ↔ remote, custom MIME type)
- [x] WinSCP-style context menus (F5 Copy, F6 Move, F2 Rename, F7 Mkdir, Del Delete)
- [x] Background transfer queue (Download to Queue)
- [x] File conflict dialog (Overwrite / Skip / Rename)
- [x] Authentication progress dialog (WinSCP TAuthenticateForm style)
- [x] Transfer progress dialog (WinSCP TProgressForm style)
- [x] Host fingerprint verification dialog
- [x] External terminal launcher (auto-detects kitty, xterm, Konsole, GNOME Terminal, …)
- [x] Preferences dialog (language, terminal, transfer concurrency)
- [x] Remote free space display in statusbar (async df -P)
- [x] Russian localization (full translation of all dialogs and menus)
- [ ] Working end-to-end SFTP connection (integration test)
- [ ] File icon provider (QFileIconProvider for remote panel)
- [ ] Configurable columns (size, date, permissions, owner)
- [ ] Inline remote file editor
- [ ] Theme support (Light / Dark / system)
- [ ] CI pipeline (GitHub Actions)
- [ ] Packages: AppImage, Flatpak, AUR

## Contributing

Contributions are welcome! Please open an issue before starting work on a large feature.

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Follow the code style (`.clang-format` is provided)
4. Add tests for new functionality
5. Open a pull request

## Support the Project

LinSCP is a free, open-source tool developed in spare time with no corporate backing.
If it saves you time or makes your workflow a little better — consider buying me a coffee.
Every contribution, no matter how small, helps keep the project alive and motivates further development.

| Currency | Network | Address |
|----------|---------|---------|
| **USDT** | TRON (TRC20) | `TRttXvwBiJfLcZoBLxjD6US4optkw5y8op` |
| **ETH** | Ethereum (ERC20) | `0xfb4a697fc94f938777a29b3777ba4cb432228886` |
| **BTC** | Bitcoin | `1Gp9xpx8fBJuSdQMtsV66aveDDeEz9AGVR` |

Thank you — it really does matter.

## License

LinSCP is licensed under the **GNU General Public License v2.0** — see [LICENSE](LICENSE) for details.

The project links against:
- [libssh](https://www.libssh.org/) — LGPL 2.1
- [Qt 6](https://www.qt.io/) — LGPL 3.0 (open source)
- [OpenSSL](https://www.openssl.org/) — Apache 2.0
