cmake_minimum_required(VERSION 3.25)
project(LinSCP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Concurrent Svg Test)
find_package(Qt6 QUIET OPTIONAL_COMPONENTS LinguistTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH REQUIRED libssh>=0.10)
pkg_check_modules(OPENSSL REQUIRED openssl>=3.0)

option(WITH_KEYRING  "Build with libsecret/keyring integration" OFF)

if(WITH_KEYRING)
    pkg_check_modules(LIBSECRET libsecret-1)
    if(LIBSECRET_FOUND)
        add_compile_definitions(HAVE_LIBSECRET)
    endif()
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)

# ── Install rules (used by CPack and cmake --install) ─────────────────────────
include(GNUInstallDirs)
install(TARGETS linscp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)
install(FILES packaging/appimage/linscp.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES packaging/appimage/linscp.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps/linscp.svg)

# ── CPack ─────────────────────────────────────────────────────────────────────
set(CPACK_PACKAGE_NAME             "linscp")
set(CPACK_PACKAGE_VENDOR           "LinSCP")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "SSH/SFTP file manager — Linux analog of WinSCP")
set(CPACK_PACKAGE_HOMEPAGE_URL     "https://github.com/LinCSP/LinSCP")
set(CPACK_RESOURCE_FILE_LICENSE    "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_STRIP_FILES              TRUE)

# RPM (Fedora / RHEL / openSUSE)
set(CPACK_RPM_FILE_NAME            RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE      "GPL-2.0-only")
set(CPACK_RPM_PACKAGE_GROUP        "Applications/Internet")
set(CPACK_RPM_PACKAGE_AUTOREQ      ON)   # rpmbuild auto-detects deps from binary

include(CPack)
