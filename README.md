# LinSCP

**LinSCP** is a cross-platform SFTP/SSH file manager for Linux, built with C++20 and Qt 6.
It aims to bring the familiar WinSCP experience to Linux and other Unix-like systems.

![LinSCP screenshot placeholder](docs/screenshot.png)

## Features

- **Dual-pane interface** — local and remote file browsers side by side
- **SFTP file operations** — browse, upload, download, rename, delete, chmod, mkdir
- **SSH channel multiplexing** — SFTP and terminal share a single SSH connection
- **Authentication** — password, public key (RSA/Ed25519/ECDSA), SSH agent
- **Session manager** — save/load profiles with AES-256-GCM encrypted passwords
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
./build/src/linscp
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
│   │   ├── ssh/          # SSH session, channel, auth, known hosts
│   │   ├── sftp/         # SFTP client, file/directory types
│   │   ├── scp/          # SCP protocol fallback
│   │   ├── transfer/     # Transfer queue and manager
│   │   ├── sync/         # Directory synchronization engine
│   │   ├── session/      # Session profiles and encrypted store
│   │   └── keys/         # SSH key manager and generator
│   ├── models/           # Qt item models (local FS, remote FS, transfer queue)
│   ├── ui/
│   │   ├── panels/       # Local and remote file panels
│   │   ├── widgets/      # Reusable widgets (breadcrumb, progress, file list)
│   │   └── dialogs/      # Session, key, sync, transfer dialogs
│   └── utils/            # Checksum, crypto utilities, file helpers
├── tests/
│   └── unit/             # Qt Test unit tests (one executable per suite)
└── task/
    ├── TODO.md           # Task board
    └── CHANGELOG.md      # Change history
```

## Architecture

LinSCP is organized around these core components:

- **`linscp::core::ssh`** — `SshSession` wraps a `libssh` session with a state machine (Disconnected → Connecting → VerifyingHost → Authenticating → Connected). Multiple `SshChannel` objects (SFTP, Shell, Exec) share one `ssh_session_t`.
- **`linscp::core::sftp`** — `SftpClient` provides the full SFTP API. Directory listings are loaded asynchronously via `QThreadPool`.
- **`linscp::core::transfer`** — Thread-safe `TransferQueue` + `TransferManager` that runs transfers in a worker pool.
- **`linscp::core::sync`** — `SyncComparator` diffs local ↔ remote trees and feeds deltas to `SyncEngine`.
- **`linscp::core::session`** — `SessionProfile` serialized to JSON, with passwords encrypted using AES-256-GCM (OpenSSL EVP).
- **`linscp::models`** — `RemoteFsModel` / `LocalFsModel` implement `QAbstractItemModel` for the dual-pane view.

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
