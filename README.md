# LinSCP

**LinSCP** is a cross-platform SFTP/SSH file manager for Linux, built with C++20 and Qt 6.
It aims to bring the familiar WinSCP experience to Linux and other Unix-like systems.


## Features

- **Dual-pane interface** — local and remote file browsers side by side
- **SFTP file operations** — browse, upload, download, rename, delete, chmod, mkdir
- **SSH channel multiplexing** — SFTP and terminal share a single SSH connection
- **Authentication** — password, public key (RSA/Ed25519/ECDSA), SSH agent
- **Session manager** — save/load profiles with AES-256-GCM encrypted passwords
- **Path state persistence** — each session remembers the last open local and remote directory
- **WinSCP import** — import sessions directly from WinSCP.ini (host, port, user, key, paths, folder structure)
- **SSH key manager** — generate, import, export, and convert keys (PuTTY PPK ↔ OpenSSH)
- **Transfer queue** — parallel transfers with progress, speed, and ETA display
- **Directory synchronization** — compare local ↔ remote and sync in either direction
- **Known hosts management** — verify and trust remote host keys
- **SCP protocol support** — as a fallback when SFTP is unavailable

## Screenshots

> Coming soon — the project is in early development (v0.1-dev).

## Requirements

| Dependency | Minimum version |
|------------|----------------|
| C++ compiler | GCC 11 / Clang 14 (C++20) |
| CMake | 3.25 |
| Qt | 6.5 |
| libssh | 0.10 |
| OpenSSL | 3.0 |

Optional:
- **qtermwidget6** — embedded terminal emulator (`-DWITH_TERMINAL=ON`)
- **libsecret** — GNOME Keyring / KWallet integration (`-DWITH_KEYRING=ON`)

## Building

```bash
# Install dependencies (Arch Linux)
sudo pacman -S qt6-base cmake libssh openssl ninja

# Install dependencies (Ubuntu 24.04 / Debian 12)
sudo apt install qt6-base-dev cmake libssh-dev libssl-dev ninja-build

# Install dependencies (Fedora 39+)
sudo dnf install qt6-qtbase-devel cmake libssh-devel openssl-devel ninja-build

# Clone and build
git clone https://github.com/YOUR_USERNAME/linscp.git
cd linscp
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bin/linscp
```

### Optional features

```bash
# With embedded terminal
cmake -B build -G Ninja -DWITH_TERMINAL=ON

# With system keyring (GNOME/KDE)
cmake -B build -G Ninja -DWITH_KEYRING=ON
```

## Project structure

```
linscp/
├── src/
│   ├── core/
│   │   ├── app_settings.h/cpp        # Application-wide settings (QSettings wrapper)
│   │   ├── ssh/                      # SSH session, channel, auth, known hosts
│   │   ├── sftp/                     # SFTP client, file and directory types
│   │   ├── scp/                      # SCP protocol fallback
│   │   ├── transfer/                 # Transfer queue and manager (async, QThreadPool)
│   │   ├── sync/                     # Directory synchronization engine
│   │   ├── keys/                     # SSH key manager, generator, PPK converter
│   │   └── session/
│   │       ├── session_profile       # SessionProfile struct (all connection settings)
│   │       ├── session_store         # Encrypted JSON store (AES-256-GCM)
│   │       ├── session_manager       # Manages active SSH connections
│   │       ├── path_state_store      # Persists last visited paths per session
│   │       └── winscp_importer       # Import sessions from WinSCP.ini
│   ├── models/
│   │   ├── local_fs_model            # QAbstractItemModel for local filesystem
│   │   ├── remote_fs_model           # QAbstractItemModel for remote SFTP (async)
│   │   └── transfer_queue_model      # Model for transfer queue view
│   ├── ui/
│   │   ├── main_window               # Main window, tab management, menu/toolbar
│   │   ├── connection_tab            # One tab: LocalPanel + RemotePanel per SSH session
│   │   ├── panels/
│   │   │   ├── file_panel            # Base class (toolbar, breadcrumb, list, statusbar)
│   │   │   ├── local_panel           # Left panel — local filesystem
│   │   │   └── remote_panel          # Right panel — remote SFTP filesystem
│   │   ├── widgets/
│   │   │   ├── breadcrumb_bar        # Clickable path bar with inline edit
│   │   │   ├── file_list_view        # File list with drag & drop support
│   │   │   └── transfer_progress_widget  # Per-transfer progress row
│   │   ├── dialogs/
│   │   │   ├── login_dialog          # Session tree + connection form (main entry point)
│   │   │   ├── session_dialog        # Quick connect dialog
│   │   │   ├── advanced_session_dialog   # Tunnel, proxy, environment, SSH settings
│   │   │   ├── auth_dialog           # Authentication progress
│   │   │   ├── host_fingerprint_dialog   # Unknown/changed host key warning
│   │   │   ├── key_dialog            # SSH key manager UI
│   │   │   ├── sync_dialog           # Directory sync (3-page wizard)
│   │   │   ├── transfer_panel        # Transfer queue dock panel
│   │   │   ├── copy_dialog           # Copy/move options
│   │   │   ├── properties_dialog     # File/directory properties
│   │   │   ├── progress_dialog       # Transfer progress dialog
│   │   │   └── preferences_dialog    # Application preferences
│   │   └── terminal/
│   │       └── terminal_widget       # Embedded terminal (qtermwidget6, optional)
│   └── utils/
│       ├── checksum                  # Local and remote file checksums
│       ├── crypto_utils              # AES-256-GCM encrypt/decrypt helpers
│       └── file_utils                # Path and file utility functions
└── tests/
    └── unit/                         # Qt Test unit tests (one executable per suite)
```

## Architecture

LinSCP is organized around these core components:

- **`linscp::core::ssh`** — `SshSession` wraps a `libssh` session with a state machine (Disconnected → Connecting → VerifyingHost → Authenticating → Connected). Multiple `SshChannel` objects (SFTP, Shell, Exec) share one `ssh_session_t`.
- **`linscp::core::sftp`** — `SftpClient` provides the full SFTP API. Directory listings are loaded asynchronously via `QThreadPool`.
- **`linscp::core::transfer`** — Thread-safe `TransferQueue` + `TransferManager` that runs transfers in a worker pool.
- **`linscp::core::sync`** — `SyncComparator` diffs local ↔ remote trees and feeds deltas to `SyncEngine`.
- **`linscp::core::session`** — `SessionProfile` serialized to JSON with AES-256-GCM encrypted passwords. `PathStateStore` saves the last visited local and remote path for each session in `~/.config/linscp/path_state.json`. `WinScpImporter` parses WinSCP.ini and converts sessions including folder hierarchy.
- **`linscp::models`** — `RemoteFsModel` / `LocalFsModel` implement `QAbstractItemModel` for the dual-pane view.

## WinSCP import

**Tools → Import from WinSCP…** in the Login dialog accepts a `WinSCP.ini` file.

Imported fields per session:
- Host, port, username
- Protocol (SCP / SFTP)
- Public key file path (Windows path converted to Linux)
- Initial remote and local directories
- Folder hierarchy (e.g. `Moy/A-media/Cobalt-pro`)

Passwords are **not** imported — they are stored encrypted with a WinSCP-specific key and cannot be decoded. You will be prompted for a password or key on first connect.

## Running tests

```bash
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

- [x] Project skeleton with full C++20/Qt6 source layout
- [x] SSH session state machine + channel multiplexing
- [x] SFTP client (ls, upload, download, rename, rm, mkdir, chmod)
- [x] Transfer queue and manager
- [x] Session profile store with AES-256-GCM encryption
- [x] SSH key manager and generator
- [x] Directory synchronization engine
- [x] Dual-pane Qt UI skeleton
- [x] Path state persistence — sessions reopen at the last visited directory
- [x] WinSCP session import (host, auth, paths, folder structure)
- [ ] Working end-to-end SFTP connection
- [ ] Embedded terminal (qtermwidget6)
- [ ] Drag & drop between panels
- [ ] Bookmarks and recent sessions
- [ ] CI pipeline (GitHub Actions)
- [ ] Packages: AppImage, Flatpak, AUR

## Contributing

Contributions are welcome! Please open an issue before starting work on a large feature.

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Follow the code style (`.clang-format` is provided)
4. Add tests for new functionality
5. Open a pull request

## License

LinSCP is licensed under the **GNU General Public License v2.0** — see [LICENSE](LICENSE) for details.

The project links against:
- [libssh](https://www.libssh.org/) — LGPL 2.1
- [Qt 6](https://www.qt.io/) — LGPL 3.0 (open source)
- [OpenSSL](https://www.openssl.org/) — Apache 2.0
