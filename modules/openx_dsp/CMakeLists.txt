add_library(openx_dsp INTERFACE)

target_include_directories(openx_dsp INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(openx_dsp INTERFACE
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_dsp
)

target_compile_features(openx_dsp INTERFACE cxx_std_20)

if(MSVC)
    # /O2 with /fp:fast makes MSVC's codegen hang on this concept/`requires`-heavy
    # DSP (the "Generating Code..." phase never completes). Use /O1 and precise fp.
    target_compile_options(openx_dsp INTERFACE /O1 /fp:precise /W4)
else()
    target_compile_options(openx_dsp INTERFACE -O3 -ffast-math -Wall -Wextra -Wpedantic)
endif()
