set(LINSCP_SOURCES
    main.cpp

    # core/ssh  (ssh_auth.h — header-only, нет .cpp)
    core/ssh/ssh_session.cpp
    core/ssh/ssh_channel.cpp
    core/ssh/known_hosts.cpp

    # core/sftp
    core/sftp/sftp_client.cpp
    core/sftp/sftp_file.cpp
    core/sftp/sftp_directory.cpp

    # core/scp  (заглушка — файл создаётся ниже)

    # core/transfer
    core/transfer/transfer_item.cpp
    core/transfer/transfer_queue.cpp
    core/transfer/transfer_manager.cpp

    # core/sync
    core/sync/sync_profile.cpp
    core/sync/sync_comparator.cpp
    core/sync/sync_engine.cpp

    # core/keys
    core/keys/key_manager.cpp
    core/keys/key_generator.cpp
    core/keys/ppk_converter.cpp

    # core/session
    core/session/session_profile.cpp
    core/session/session_store.cpp
    core/session/session_manager.cpp

    # models
    models/remote_fs_model.cpp
    models/local_fs_model.cpp
    models/transfer_queue_model.cpp

    # ui
    ui/main_window.cpp

    # ui/panels
    ui/panels/file_panel.cpp
    ui/panels/local_panel.cpp
    ui/panels/remote_panel.cpp

    # ui/dialogs
    ui/dialogs/session_dialog.cpp
    ui/dialogs/key_dialog.cpp
    ui/dialogs/sync_dialog.cpp
    ui/dialogs/transfer_panel.cpp

    # ui/widgets
    ui/widgets/breadcrumb_bar.cpp
    ui/widgets/file_list_view.cpp
    ui/widgets/transfer_progress_widget.cpp

    # utils
    utils/checksum.cpp
    utils/file_utils.cpp
    utils/crypto_utils.cpp
)

if(WITH_TERMINAL)
    list(APPEND LINSCP_SOURCES ui/terminal/terminal_widget.cpp)
endif()

add_executable(linscp ${LINSCP_SOURCES})

target_include_directories(linscp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBSSH_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

target_link_libraries(linscp PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Concurrent
    ${LIBSSH_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

if(WITH_TERMINAL AND QTERMWIDGET_FOUND)
    target_include_directories(linscp PRIVATE ${QTERMWIDGET_INCLUDE_DIRS})
    target_link_libraries(linscp PRIVATE ${QTERMWIDGET_LIBRARIES})
endif()

if(WITH_KEYRING AND LIBSECRET_FOUND)
    target_include_directories(linscp PRIVATE ${LIBSECRET_INCLUDE_DIRS})
    target_link_libraries(linscp PRIVATE ${LIBSECRET_LIBRARIES})
endif()

target_compile_options(linscp PRIVATE
    -Wall -Wextra -Wpedantic
    ${LIBSSH_CFLAGS_OTHER}
)
