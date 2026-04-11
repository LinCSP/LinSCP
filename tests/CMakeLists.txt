# Qt6::Test найден в корневом CMakeLists.txt

# ── Вспомогательная функция ───────────────────────────────────────────────────
function(add_unit_test name)
    # Остальные аргументы — дополнительные source-файлы из src/
    add_executable(${name}
        unit/${name}.cpp
        ${ARGN}
    )
    target_include_directories(${name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${LIBSSH_INCLUDE_DIRS}
        ${OPENSSL_INCLUDE_DIRS}
    )
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Test
        ${LIBSSH_LIBRARIES}
        ${OPENSSL_LIBRARIES}
    )
    target_link_directories(${name} PRIVATE
        ${LIBSSH_LIBRARY_DIRS}
        ${OPENSSL_LIBRARY_DIRS}
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ── Unit tests (без сети) ─────────────────────────────────────────────────────

add_unit_test(test_file_utils
    ${CMAKE_SOURCE_DIR}/src/utils/file_utils.cpp
)

add_unit_test(test_crypto_utils
    ${CMAKE_SOURCE_DIR}/src/utils/crypto_utils.cpp
)

add_unit_test(test_sftp_file
    ${CMAKE_SOURCE_DIR}/src/core/sftp/sftp_file.cpp
    ${CMAKE_SOURCE_DIR}/src/core/sftp/sftp_directory.cpp
)

add_unit_test(test_transfer_queue
    ${CMAKE_SOURCE_DIR}/src/core/transfer/transfer_item.cpp
    ${CMAKE_SOURCE_DIR}/src/core/transfer/transfer_queue.cpp
)

add_unit_test(test_sync_comparator
    ${CMAKE_SOURCE_DIR}/src/core/sync/sync_profile.cpp
)

# ── Integration tests (с SSH-сервером в Docker) ──────────────────────────────
# Запускаются только при наличии переменной LINSCP_TEST_HOST
if(DEFINED ENV{LINSCP_TEST_HOST})
    add_executable(linscp_integration_tests
        integration/test_ssh_session.cpp
        integration/test_sftp_client.cpp
        integration/test_transfer_manager.cpp
    )
    target_include_directories(linscp_integration_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${LIBSSH_INCLUDE_DIRS}
    )
    target_link_libraries(linscp_integration_tests PRIVATE
        Qt6::Core Qt6::Test
        ${LIBSSH_LIBRARIES}
    )
    add_test(NAME integration_tests COMMAND linscp_integration_tests)
endif()
