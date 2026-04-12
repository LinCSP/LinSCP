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

    # core/scp
    core/scp/scp_client.cpp

    # core/transfer
    core/transfer/transfer_item.cpp
    core/transfer/transfer_queue.cpp
    core/transfer/transfer_manager.cpp

    # core/sync
    core/sync/sync_profile.cpp
    core/sync/sync_comparator.cpp
    core/sync/sync_engine.cpp
    core/sync/sync_profile_store.cpp

    # core/keys
    core/keys/key_manager.cpp
    core/keys/key_generator.cpp
    core/keys/ppk_converter.cpp

    # core/session
    core/session/session_profile.cpp
    core/session/session_store.cpp
    core/session/session_manager.cpp
    core/session/path_state_store.cpp
    core/session/winscp_importer.cpp

    # models
    models/remote_fs_model.cpp
    models/local_fs_model.cpp
    models/transfer_queue_model.cpp

    # ui
    ui/main_window.cpp
    ui/connection_tab.cpp

    # ui/panels
    ui/panels/file_panel.cpp
    ui/panels/local_panel.cpp
    ui/panels/remote_panel.cpp

    # ui/dialogs
    ui/dialogs/session_dialog.cpp
    ui/dialogs/advanced_session_dialog.cpp
    ui/dialogs/login_dialog.cpp
    ui/dialogs/key_dialog.cpp
    ui/dialogs/sync_dialog.cpp
    ui/dialogs/transfer_panel.cpp
    ui/dialogs/host_fingerprint_dialog.cpp
    ui/dialogs/copy_dialog.cpp
    ui/dialogs/overwrite_dialog.cpp
    ui/dialogs/properties_dialog.cpp
    ui/dialogs/auth_dialog.cpp
    ui/dialogs/progress_dialog.cpp
    ui/dialogs/preferences_dialog.cpp
    ui/dialogs/remote_editor_dialog.cpp

    # core
    core/app_settings.cpp

    # ui/widgets
    ui/widgets/breadcrumb_bar.cpp
    ui/widgets/file_list_view.cpp
    ui/widgets/transfer_progress_widget.cpp

    # utils
    utils/checksum.cpp
    utils/file_utils.cpp
    utils/crypto_utils.cpp

    # ui/utils
    ui/utils/svg_icon.cpp
    ui/utils/file_icon_provider.cpp
)

list(APPEND LINSCP_SOURCES ui/terminal/terminal_widget.cpp)

qt_add_resources(LINSCP_SOURCES resources/icons.qrc)

add_executable(linscp ${LINSCP_SOURCES})

set_target_properties(linscp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
if(APPLE)
    set_target_properties(linscp PROPERTIES MACOSX_BUNDLE TRUE)
endif()

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
    Qt6::Svg
    ${LIBSSH_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)

target_link_directories(linscp PRIVATE
    ${LIBSSH_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
)

if(WITH_KEYRING AND LIBSECRET_FOUND)
    target_include_directories(linscp PRIVATE ${LIBSECRET_INCLUDE_DIRS})
    target_link_libraries(linscp PRIVATE ${LIBSECRET_LIBRARIES})
endif()

target_compile_options(linscp PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/wd4100;/wd4127>
    ${LIBSSH_CFLAGS_OTHER}
)

# ── i18n / переводы ───────────────────────────────────────────────────────────
if(Qt6LinguistTools_FOUND)
    set(TS_FILES i18n/linscp_ru.ts)
    qt_add_translations(linscp
        TS_FILES ${TS_FILES}
        LUPDATE_OPTIONS -no-obsolete
    )
    # Копируем .qm рядом с бинарником — fallback для загрузки из файловой системы
    add_custom_command(TARGET linscp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_BINARY_DIR}/linscp_ru.qm"
            "${CMAKE_BINARY_DIR}/bin/linscp_ru.qm"
        COMMENT "Copying linscp_ru.qm → bin/"
    )
    message(STATUS "Qt Linguist found — translations enabled (${TS_FILES})")
else()
    message(STATUS "Qt6::LinguistTools not found — translations disabled")
endif()
