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
        Catch2::Catch2WithMain
)

include(Catch)
catch_discover_tests(serctrl_tests)
