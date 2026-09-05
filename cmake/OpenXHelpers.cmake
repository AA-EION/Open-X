# Helper macro for standard Open-X plugin declaration
macro(openx_declare_plugin NAME CODE)
    juce_add_plugin(${NAME}
        COMPANY_NAME "Open-X DSP"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE "OpnX"
        PLUGIN_CODE ${CODE}
        FORMATS ${OPENX_PLUGIN_FORMATS}
        PRODUCT_NAME ${NAME}
    )

    target_sources(${NAME} PRIVATE
        source/PluginProcessor.cpp
        source/PluginProcessor.h
        source/PluginEditor.cpp
        source/PluginEditor.h
    )

    target_link_libraries(${NAME} PRIVATE
        openx_dsp
        openx_ui
        juce::juce_audio_utils
    )

    target_compile_features(${NAME} PRIVATE cxx_std_20)
endmacro()
