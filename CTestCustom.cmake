# We don't want to valgrind the python interpreter
set (CTEST_CUSTOM_MEMCHECK_IGNORE ${CTEST_CUSTOM_MEMCHECK_IGNORE} unit_test_python_statistics)
# Ignore test cases the heavily use bash
set (CTEST_CUSTOM_MEMCHECK_IGNORE ${CTEST_CUSTOM_MEMCHECK_IGNORE}
     unit_test_analyze
     unit_test_benchmark
     unit_test_lua_prompt
     unit_test_pilot_opt_parsing
    )
