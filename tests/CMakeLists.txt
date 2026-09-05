add_executable(dsp_tests
    dsp/crossover_test.cpp
)

target_link_libraries(dsp_tests PRIVATE
    openx_dsp
)

target_compile_features(dsp_tests PRIVATE cxx_std_20)

add_test(NAME dsp_crossover_tests COMMAND dsp_tests)
