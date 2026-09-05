juce_add_plugin(EQ-X
    COMPANY_NAME "Open-X DSP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE "OpnX"
    PLUGIN_CODE "Eq01"
    FORMATS ${OPENX_PLUGIN_FORMATS}
    PRODUCT_NAME "EQ-X"
)

target_sources(EQ-X PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.cpp
    source/PluginEditor.h
)

target_link_libraries(EQ-X PRIVATE
    openx_dsp
    openx_ui
    juce::juce_audio_utils
)

target_compile_features(EQ-X PRIVATE cxx_std_20)
