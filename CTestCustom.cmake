# We don't want to valgrind the python interpreter
set (CTEST_CUSTOM_MEMCHECK_IGNORE ${CTEST_CUSTOM_MEMCHECK_IGNORE} unit_test_python_statistics)
