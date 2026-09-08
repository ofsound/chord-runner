# JUCE generates AppIcon.icns once at configure time; rebuilds do not notice packaging/icon.png changes.
# Regenerate the .icns when the source PNG changes so Standalone picks up new artwork.
if(NOT APPLE OR NOT TARGET juce::juceaide)
    return()
endif()

get_target_property(_pamplejuce_icon_gen_dir ${PROJECT_NAME} JUCE_GENERATED_SOURCES_DIRECTORY)
if(NOT _pamplejuce_icon_gen_dir)
    return()
endif()

set(_pamplejuce_icon_png "${CMAKE_CURRENT_SOURCE_DIR}/packaging/icon.png")
set(_pamplejuce_icon_icns "${_pamplejuce_icon_gen_dir}/AppIcon.icns")

add_custom_command(
    OUTPUT "${_pamplejuce_icon_icns}"
    COMMAND ${CMAKE_COMMAND} -E echo "Regenerating AppIcon.icns from packaging/icon.png"
    COMMAND $<TARGET_FILE:juce::juceaide> macicon "${_pamplejuce_icon_icns}" AppIcon "${_pamplejuce_icon_png}"
    DEPENDS "${_pamplejuce_icon_png}"
    COMMENT "Building macOS app icon from packaging/icon.png"
    VERBATIM)

add_custom_target(${PROJECT_NAME}_AppIcon DEPENDS "${_pamplejuce_icon_icns}")

foreach(_pamplejuce_icon_target ${PROJECT_NAME}_Standalone ${PROJECT_NAME}_AU ${PROJECT_NAME}_VST3)
    if(TARGET ${_pamplejuce_icon_target})
        add_dependencies(${_pamplejuce_icon_target} ${PROJECT_NAME}_AppIcon)
    endif()
endforeach()
