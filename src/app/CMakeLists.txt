# ICON_BIG/ICON_SMALL: JUCE's own juceaide tool converts these PNGs into a
# real platform icon resource (.ico on Windows, .icns on macOS) embedded in
# the executable - without this the exe/app carries no icon at all, so
# Explorer/Finder, the taskbar, and any shortcut pointing at it (including
# the Start Menu shortcut CPack creates below) fall back to a generic icon.
juce_add_gui_app(Navalha2Juce
    PRODUCT_NAME "Navalha 2"
    COMPANY_NAME "Navalha 2"
    BUNDLE_ID "org.navalha.arcade"
    NEEDS_WEB_BROWSER ${NAVALHA_ENABLE_WEBVIEW}
    ICON_BIG "${NAVALHA_PD_PATH}/app/assets/brand/navalha2-app-icon-512.png"
    ICON_SMALL "${NAVALHA_PD_PATH}/app/assets/brand/navalha2-app-icon-32.png"
)

juce_add_binary_data(Navalha2ArcadeAssets
    SOURCES
        "${NAVALHA_PD_PATH}/app/assets/brand/navalha2-header-arcade.svg"
        "${NAVALHA_PD_PATH}/app/assets/brand/navalha2-app-icon-128.png"
)

target_sources(Navalha2Juce PRIVATE Main.cpp)
target_compile_features(Navalha2Juce PRIVATE cxx_std_20)

target_link_libraries(Navalha2Juce PRIVATE
    navalha_core
    Navalha2ArcadeAssets
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

if (UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(Navalha2Juce PRIVATE X11::X11)
endif()

if (NAVALHA_ENABLE_WEBVIEW)
    target_link_libraries(Navalha2Juce PRIVATE juce::juce_gui_extra)
endif()

target_compile_definitions(Navalha2Juce PRIVATE
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:Navalha2Juce,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_WEB_BROWSER=$<BOOL:${NAVALHA_ENABLE_WEBVIEW}>
    JUCE_USE_CURL=0
)

if (UNIX AND NOT APPLE)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/navalha2.desktop.in"
        "${CMAKE_CURRENT_BINARY_DIR}/navalha2.desktop"
        @ONLY)
    install(TARGETS Navalha2Juce RUNTIME DESTINATION bin)
    install(FILES
        "${NAVALHA_PD_PATH}/app/assets/brand/navalha2-app-icon-128.png"
        DESTINATION share/icons/hicolor/128x128/apps
        RENAME navalha2.png)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/navalha2.desktop"
        DESTINATION share/applications)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    install(TARGETS Navalha2Juce RUNTIME DESTINATION .)
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    install(TARGETS Navalha2Juce BUNDLE DESTINATION .)
endif()

if (NAVALHA_BUILD_TESTS AND UNIX AND NOT APPLE)
    add_test(
        NAME navalha_juce_headless_guard
        COMMAND "${CMAKE_COMMAND}" -E env DISPLAY=:987
                "$<TARGET_FILE:Navalha2Juce>")
    set_tests_properties(navalha_juce_headless_guard PROPERTIES WILL_FAIL TRUE)
endif()
