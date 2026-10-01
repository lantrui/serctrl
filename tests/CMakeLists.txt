# Test sources
set(TEST_SOURCES
    test_message.cpp
    test_encryptor.cpp
    test_auth.cpp
    test_config.cpp
    test_e2e.cpp
)

add_executable(serctrl_tests ${TEST_SOURCES})
target_link_libraries(serctrl_tests
    PRIVATE
        serctrl_lib
)

# Catch2 v3 amalgamated (vendored for offline/CI builds; BSL-1.0 license)
add_library(catch2_main STATIC ${CMAKE_CURRENT_SOURCE_DIR}/catch/catch_amalgamated.cpp)
target_include_directories(catch2_main PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/catch)
target_link_libraries(serctrl_tests PRIVATE catch2_main)

add_test(NAME serctrl_tests COMMAND serctrl_tests)
