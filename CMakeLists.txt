cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_CURRENT_SOURCE_DIR}/cmake_project")
include(PamplejuceVersion)
include(CPM)
include(JUCEDefaults)
if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()
set(PROJECT_NAME ChordRunner)
set(PRODUCT_NAME "Chord Runner")
set(COMPANY_NAME ofsound)
set(BUNDLE_ID com.ofsound.chordrunner)
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})
include(PamplejuceLog)
include(PamplejuceFunctions)
add_subdirectory(JUCE)
set(FORMATS Standalone VST3)
if(APPLE)
    list(APPEND FORMATS AU)
endif()
option(CHORD_RUNNER_COPY_PLUGINS "Install plugins in user plugin folders after building" ON)
option(CHORD_RUNNER_UI_DEV_SERVER "Use Vite on port 5175 in Debug" OFF)
juce_add_plugin(${PROJECT_NAME}
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COPY_PLUGIN_AFTER_BUILD ${CHORD_RUNNER_COPY_PLUGINS}
    NEEDS_WEB_BROWSER TRUE
    NEEDS_WEBVIEW2 TRUE
    PLUGIN_MANUFACTURER_CODE Osnd
    PLUGIN_CODE Crn1
    FORMATS ${FORMATS}
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    VST3_CATEGORIES Fx Instrument
    PRODUCT_NAME "${PRODUCT_NAME}")
if(APPLE)
    include(PamplejuceIcon)
endif()
add_library(SharedCode INTERFACE)
# Standard floating point is needed for finite-number validation and precise timing.
target_compile_features(SharedCode INTERFACE cxx_std_23)
file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS "source/*.cpp" "source/*.h")
target_sources(SharedCode INTERFACE ${SourceFiles})
include(WebViewUI)
juce_add_binary_data(Assets SOURCES "${WEBVIEW_ZIP}")
add_dependencies(Assets WebViewUIBuild)
target_compile_definitions(SharedCode INTERFACE
    JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    CHORD_RUNNER_UI_DEV_SERVER=$<BOOL:${CHORD_RUNNER_UI_DEV_SERVER}>
    CMAKE_BUILD_TYPE="$<CONFIG>" VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="${PRODUCT_NAME}")
target_link_libraries(SharedCode INTERFACE
    Assets juce_audio_utils juce_audio_processors juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_link_libraries(${PROJECT_NAME} PRIVATE SharedCode)
target_compile_definitions(${PROJECT_NAME}_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_sources(${PROJECT_NAME}_Standalone PRIVATE standalone/StandaloneApp.cpp)
pamplejuce_add_tests()
file(GLOB_RECURSE OwnedFiles CONFIGURE_DEPENDS "tests/*.cpp" "standalone/*.cpp")
if(MSVC)
    set_source_files_properties(${SourceFiles} ${OwnedFiles} PROPERTIES COMPILE_OPTIONS /WX)
else()
    set_source_files_properties(${SourceFiles} ${OwnedFiles} PROPERTIES COMPILE_OPTIONS -Werror)
endif()
include(GitHubENV)
