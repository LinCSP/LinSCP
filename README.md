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

## Building

```bash
# Install dependencies (Arch Linux)
sudo pacman -S qt6-base cmake libssh openssl ninja

# Install dependencies (Ubuntu 24.04 / Debian 12)
sudo apt install qt6-base-dev cmake libssh-dev libssl-dev ninja-build

# Install dependencies (Fedora 39+)
sudo dnf install qt6-qtbase-devel cmake libssh-devel openssl-devel ninja-build

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

## Project structure

```
linscp/
├── src/
│   ├── core/
│   │   ├── app_settings.h/cpp        # Application-wide settings (QSettings wrapper)
│   │   ├── ssh/                      # SSH session, channel, auth, known hosts
│   │   ├── sftp/                     # SFTP client, file and directory types
│   │   ├── scp/                      # SCP protocol fallback (stub)
│   │   ├── transfer/                 # Transfer queue and manager (async, QThreadPool)
│   │   ├── sync/
│   │   │   ├── sync_profile          # Sync settings struct
│   │   │   ├── sync_profile_store    # JSON persistence for sync profiles
│   │   │   ├── sync_comparator       # Diff local ↔ remote (mtime+size or SHA-256)
│   │   │   └── sync_engine           # Orchestrates preview + apply via TransferQueue
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
│   │   │   └── remote_panel          # Right panel — remote SFTP + free space display
│   │   ├── widgets/
│   │   │   ├── breadcrumb_bar        # Clickable path bar with inline edit
│   │   │   ├── file_list_view        # File list with drag & drop support
│   │   │   └── transfer_progress_widget  # Per-transfer progress row
│   │   ├── dialogs/
│   │   │   ├── login_dialog          # Session tree + connection form (main entry point)
│   │   │   ├── session_dialog        # Quick connect dialog
│   │   │   ├── advanced_session_dialog   # Tunnel, proxy, environment, SSH settings
│   │   │   ├── auth_dialog           # Authentication progress log
│   │   │   ├── host_fingerprint_dialog   # Unknown/changed host key warning
│   │   │   ├── key_dialog            # SSH key manager UI
│   │   │   ├── sync_dialog           # Directory sync (3-page wizard + saved profiles)
│   │   │   ├── transfer_panel        # Transfer queue dock panel
│   │   │   ├── copy_dialog           # Copy/move options (Upload/Download/Move + queue)
│   │   │   ├── overwrite_dialog      # File conflict: Overwrite/Skip/Rename/Cancel
│   │   │   ├── properties_dialog     # File/directory properties + chmod
│   │   │   ├── progress_dialog       # Transfer progress (WinSCP TProgressForm style)
│   │   │   └── preferences_dialog    # Application preferences
│   │   └── terminal/
│   │       └── terminal_widget       # External terminal launcher (kitty/xterm/konsole/…)
│   └── utils/
│       ├── checksum                  # Local and remote file checksums (SHA-256/MD5)
│       ├── crypto_utils              # AES-256-GCM encrypt/decrypt helpers
│       └── file_utils                # Path and file utility functions
├── tests/
│   └── unit/                         # Qt Test unit tests (one executable per suite)
└── task/
    ├── TODO.md                       # Task board
    └── CHANGELOG.md                  # Change log
```

## Architecture

LinSCP is organized around these core components:

- **`linscp::core::ssh`** — `SshSession` wraps a `libssh` session with a state machine (Disconnected → Connecting → VerifyingHost → Authenticating → Connected). Multiple `SshChannel` objects (SFTP, Shell, Exec) share one `ssh_session_t`. Connect is asynchronous (`QFuture` + `std::atomic` abort flag).
- **`linscp::core::sftp`** — `SftpClient` provides the full SFTP API. Directory listings are loaded asynchronously via `QThreadPool`. A generation counter in `RemoteFsModel` prevents use-after-free on rapid navigation.
- **`linscp::core::transfer`** — Thread-safe `TransferQueue` (all signals emitted outside the mutex lock) + `TransferManager` that runs transfers in a worker pool.
- **`linscp::core::sync`** — `SyncComparator` diffs local ↔ remote trees (mtime+size or SHA-256 via `ssh exec`) and feeds deltas to `SyncEngine`, which queues uploads/downloads in `TransferQueue`. `SyncProfileStore` persists sync settings to `~/.config/linscp/sync_profiles.json`.
- **`linscp::core::session`** — `SessionProfile` serialized to JSON with AES-256-GCM encrypted passwords. `PathStateStore` saves the last visited local and remote path for each session. `WinScpImporter` parses WinSCP.ini including folder hierarchy.
- **`linscp::models`** — `RemoteFsModel` / `LocalFsModel` implement `QAbstractItemModel` for the dual-pane view.
- **`linscp::core::AppSettings`** — singleton wrapper around `QSettings`; typed getters/setters for language, terminal emulator, and transfer concurrency.

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

## License

LinSCP is licensed under the **GNU General Public License v2.0** — see [LICENSE](LICENSE) for details.

The project links against:
- [libssh](https://www.libssh.org/) — LGPL 2.1
- [Qt 6](https://www.qt.io/) — LGPL 3.0 (open source)
- [OpenSSL](https://www.openssl.org/) — Apache 2.0
