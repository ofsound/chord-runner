set(WEBVIEW_UI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ui")
set(WEBVIEW_ZIP "${CMAKE_CURRENT_SOURCE_DIR}/assets/webview/ui.zip")
find_program(NPM_EXECUTABLE npm REQUIRED)
file(GLOB_RECURSE WEBVIEW_UI_SOURCES CONFIGURE_DEPENDS "${WEBVIEW_UI_DIR}/src/*" "${WEBVIEW_UI_DIR}/scripts/*" "${WEBVIEW_UI_DIR}/public/*")
set(WEBVIEW_DEPENDENCIES_STAMP "${WEBVIEW_UI_DIR}/node_modules/.chord-runner-dependencies")
add_custom_command(OUTPUT "${WEBVIEW_DEPENDENCIES_STAMP}"
    COMMAND "${NPM_EXECUTABLE}" ci --no-audit --no-fund
    COMMAND "${CMAKE_COMMAND}" -E touch "${WEBVIEW_DEPENDENCIES_STAMP}"
    DEPENDS "${WEBVIEW_UI_DIR}/package.json" "${WEBVIEW_UI_DIR}/package-lock.json"
    WORKING_DIRECTORY "${WEBVIEW_UI_DIR}" VERBATIM)
add_custom_command(OUTPUT "${WEBVIEW_ZIP}"
    COMMAND "${NPM_EXECUTABLE}" run check
    COMMAND "${NPM_EXECUTABLE}" run build
    DEPENDS ${WEBVIEW_UI_SOURCES} "${WEBVIEW_DEPENDENCIES_STAMP}"
        "${WEBVIEW_UI_DIR}/index.html" "${WEBVIEW_UI_DIR}/vite.config.js"
        "${WEBVIEW_UI_DIR}/svelte.config.js" "${WEBVIEW_UI_DIR}/jsconfig.json"
    WORKING_DIRECTORY "${WEBVIEW_UI_DIR}" VERBATIM)
add_custom_target(WebViewUIBuild DEPENDS "${WEBVIEW_ZIP}")
set_source_files_properties("${WEBVIEW_ZIP}" PROPERTIES GENERATED TRUE)
