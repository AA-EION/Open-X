juce_add_gui_app(OpenXUninstaller
    PRODUCT_NAME "Open-X Uninstaller"
    COMPANY_NAME "Open-X DSP"
    BUNDLE_IDENTIFIER "com.openxdsp.uninstaller"
    VERSION ${PROJECT_VERSION}
)

set(UNINSTALLER_SOURCES
    source/Main.cpp
    source/UninstallerComponent.cpp
    source/UninstallerComponent.h
    source/UninstallerHelper.h
)

if(APPLE)
    list(APPEND UNINSTALLER_SOURCES source/UninstallerHelper.mm)
else()
    list(APPEND UNINSTALLER_SOURCES source/UninstallerHelper.cpp)
endif()

target_sources(OpenXUninstaller PRIVATE ${UNINSTALLER_SOURCES})

target_link_libraries(OpenXUninstaller PRIVATE
    openx_ui
    juce::juce_gui_basics
    juce::juce_gui_extra
)

if(APPLE)
    target_link_libraries(OpenXUninstaller PRIVATE
        "-framework Foundation"
        "-framework AppKit"
    )
    set_source_files_properties(source/UninstallerHelper.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

target_compile_features(OpenXUninstaller PRIVATE cxx_std_20)
