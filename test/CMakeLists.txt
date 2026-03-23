include(GoogleTest)

file(GLOB_RECURSE TEST_SOURCES *.cpp)

add_executable(job_scheduler_tests ${TEST_SOURCES})

target_link_libraries(job_scheduler_tests PRIVATE
    job_scheduler_lib
    GTest::gtest_main
    GTest::gmock
)

gtest_discover_tests(job_scheduler_tests)
