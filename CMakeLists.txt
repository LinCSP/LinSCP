cmake_minimum_required(VERSION 3.25)
project(LinSCP VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets Network Concurrent Test)
find_package(Qt6 QUIET OPTIONAL_COMPONENTS LinguistTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBSSH REQUIRED libssh>=0.10)
pkg_check_modules(OPENSSL REQUIRED openssl>=3.0)

option(WITH_TERMINAL "Build with embedded terminal (requires qtermwidget)" ON)
option(WITH_KEYRING  "Build with libsecret/keyring integration" OFF)

if(WITH_TERMINAL)
    pkg_check_modules(QTERMWIDGET qtermwidget6)
    if(QTERMWIDGET_FOUND)
        add_compile_definitions(HAVE_QTERMWIDGET)
    else()
        message(WARNING "qtermwidget6 not found — terminal tab will be disabled")
        set(WITH_TERMINAL OFF)
    endif()
endif()

if(WITH_KEYRING)
    pkg_check_modules(LIBSECRET libsecret-1)
    if(LIBSECRET_FOUND)
        add_compile_definitions(HAVE_LIBSECRET)
    endif()
endif()

add_subdirectory(src)

enable_testing()
add_subdirectory(tests)
