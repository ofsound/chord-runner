cmake_minimum_required(VERSION 3.25)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" "${CMAKE_CURRENT_SOURCE_DIR}/cmake_project")
include(PamplejuceVersion)
include(JUCEDefaults)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "Minimum iOS version")
    endif()
elseif(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build configuration" FORCE)
endif()
set(PROJECT_NAME ChordRunner)
set(PRODUCT_NAME "Chord Runner")
set(COMPANY_NAME ofsound)
set(BUNDLE_ID com.ofsound.chordrunner)
project(${PROJECT_NAME} VERSION ${CURRENT_VERSION})
include(PamplejuceLog)
set(_PAMPLEJUCE_USE_FUNCTIONS ON)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    include(Tests)
endif()
add_subdirectory(JUCE)
add_subdirectory(modules/melatonin_blur EXCLUDE_FROM_ALL)
set(CHORD_RUNNER_MOBILE_OPTIONS)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(FORMATS Standalone AUv3)
    option(CHORD_RUNNER_IOS_ICLOUD_ENABLED "Enable the shared iCloud project library (requires a paid Apple Developer team)" OFF)
    set(CHORD_RUNNER_ICLOUD_PLIST_ENTRIES "")
    if(CHORD_RUNNER_IOS_ICLOUD_ENABLED)
        set(CHORD_RUNNER_ICLOUD_PLIST_ENTRIES [=[
            <key>NSUbiquitousContainers</key><dict><key>iCloud.com.ofsound.chordrunner</key><dict>
            <key>NSUbiquitousContainerIsDocumentScopePublic</key><true/>
            <key>NSUbiquitousContainerName</key><string>Chord Runner</string>
            <key>NSUbiquitousContainerSupportedFolderLevels</key><string>Any</string>
            </dict></dict>]=])
    endif()
    configure_file(packaging/ios-projects.plist.in "${CMAKE_CURRENT_BINARY_DIR}/ios-projects.plist" @ONLY)
    file(READ "${CMAKE_CURRENT_BINARY_DIR}/ios-projects.plist" CHORD_RUNNER_PROJECT_PLIST)
    list(APPEND CHORD_RUNNER_MOBILE_OPTIONS PLIST_TO_MERGE "${CHORD_RUNNER_PROJECT_PLIST}"
        ICLOUD_PERMISSIONS_ENABLED ${CHORD_RUNNER_IOS_ICLOUD_ENABLED})
    list(APPEND CHORD_RUNNER_MOBILE_OPTIONS
        BACKGROUND_AUDIO_ENABLED TRUE
        TARGETED_DEVICE_FAMILY "1,2"
        IPHONE_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight
        IPAD_SCREEN_ORIENTATIONS UIInterfaceOrientationPortrait UIInterfaceOrientationPortraitUpsideDown UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight)
else()
    set(FORMATS Standalone VST3)
    if(APPLE)
        list(APPEND FORMATS AU)
        if(CMAKE_GENERATOR STREQUAL "Xcode")
            list(APPEND FORMATS AUv3)
        endif()
    endif()
endif()
option(CHORD_RUNNER_COPY_PLUGINS "Install plugins in user plugin folders after building" ON)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(CHORD_RUNNER_COPY_PLUGINS OFF)
endif()
juce_add_plugin(${PROJECT_NAME}
    ${CHORD_RUNNER_MOBILE_OPTIONS}
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png"
    COPY_PLUGIN_AFTER_BUILD ${CHORD_RUNNER_COPY_PLUGINS}
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
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    enable_language(OBJCXX)
    set(IOS_LIBRARY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/source/platform/IOSProjectLibrary.mm")
    set_source_files_properties("${IOS_LIBRARY_SOURCE}" PROPERTIES COMPILE_FLAGS "-fobjc-arc" COMPILE_OPTIONS -Werror)
    target_sources(SharedCode INTERFACE "${IOS_LIBRARY_SOURCE}")
    target_compile_definitions(SharedCode INTERFACE CHORD_RUNNER_IOS_ICLOUD_ENABLED=$<BOOL:${CHORD_RUNNER_IOS_ICLOUD_ENABLED}>)
    if(CHORD_RUNNER_IOS_ICLOUD_ENABLED)
        foreach(format Standalone AUv3)
            set_target_properties(${PROJECT_NAME}_${format} PROPERTIES
                XCODE_ATTRIBUTE_CODE_SIGN_ENTITLEMENTS "${CMAKE_CURRENT_SOURCE_DIR}/packaging/ios-projects.entitlements")
        endforeach()
    endif()
endif()
juce_add_binary_data(Assets SOURCES "assets/chords.json")
target_compile_definitions(SharedCode INTERFACE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
    CMAKE_BUILD_TYPE="$<CONFIG>" VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="${PRODUCT_NAME}")
target_link_libraries(SharedCode INTERFACE
    melatonin_blur
    Assets juce_audio_utils juce_audio_processors juce_gui_extra
    juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
target_link_libraries(${PROJECT_NAME} PRIVATE SharedCode)
target_compile_definitions(${PROJECT_NAME}_Standalone PRIVATE JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1)
target_sources(${PROJECT_NAME}_Standalone PRIVATE standalone/StandaloneApp.cpp)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
    pamplejuce_add_tests()
endif()
if(CMAKE_GENERATOR STREQUAL "Xcode")
    set_target_properties(${PROJECT_NAME}_Standalone PROPERTIES XCODE_ATTRIBUTE_COPY_PHASE_STRIP NO)
    if(TARGET ${PROJECT_NAME}_AUv3_Framework)
        set_target_properties(${PROJECT_NAME}_AUv3_Framework PROPERTIES
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${BUNDLE_ID}.internal")
    endif()
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/source" PREFIX "Source" FILES ${SourceFiles})
    foreach(format IN LISTS FORMATS)
        set_target_properties(${PROJECT_NAME}_${format} PROPERTIES XCODE_GENERATE_SCHEME ON FOLDER "Targets")
    endforeach()
endif()
file(GLOB_RECURSE OwnedFiles CONFIGURE_DEPENDS "tests/*.cpp" "standalone/*.cpp")
if(MSVC)
    set_source_files_properties(${SourceFiles} ${OwnedFiles} PROPERTIES COMPILE_OPTIONS /WX)
else()
    set_source_files_properties(${SourceFiles} ${OwnedFiles} PROPERTIES COMPILE_OPTIONS -Werror)
endif()
include(GitHubENV)
