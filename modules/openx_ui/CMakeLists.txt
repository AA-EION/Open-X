add_library(openx_ui INTERFACE)

target_include_directories(openx_ui INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(openx_ui INTERFACE
    openx_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_dsp
)

target_compile_features(openx_ui INTERFACE cxx_std_20)

if(MSVC)
    target_compile_options(openx_ui INTERFACE /O2 /W4)
else()
    target_compile_options(openx_ui INTERFACE -O3 -Wall -Wextra -Wpedantic)
endif()
