#include <catch2/catch_test_macros.hpp>
#include "serctrl/config/config.hpp"
#include "serctrl/config/encrypted_store.hpp"

#include <filesystem>

using namespace serctrl::config;

TEST_CASE("Config parse target", "[config]") {
    auto target = Config::parseTarget("user@host:8080");
    REQUIRE(target.user == "user");
    REQUIRE(target.host == "host");
    REQUIRE(target.port == 8080);
}

TEST_CASE("Config parse target without port", "[config]") {
    auto target = Config::parseTarget("user@host");
    REQUIRE(target.user == "user");
    REQUIRE(target.host == "host");
    REQUIRE(target.port == 8080);
}

TEST_CASE("Config parse target without user", "[config]") {
    auto target = Config::parseTarget("host:9090");
    REQUIRE(target.user == "");
    REQUIRE(target.host == "host");
    REQUIRE(target.port == 9090);
}

TEST_CASE("Config save and load", "[config]") {
    std::filesystem::path tmpDir = std::filesystem::temp_directory_path() / "serctrl_test_config";
    std::filesystem::create_directories(tmpDir);

    {
        Config config(tmpDir);
        HostConfig host;
        host.user = "admin";
        host.host = "example.com";
        host.port = 9090;
        config.setHost("myhost", host);
    }

    {
        Config config(tmpDir);
        auto host = config.getHost("myhost");
        REQUIRE(host.has_value());
        REQUIRE(host->user == "admin");
        REQUIRE(host->host == "example.com");
        REQUIRE(host->port == 9090);
    }

    std::filesystem::remove_all(tmpDir);
}
