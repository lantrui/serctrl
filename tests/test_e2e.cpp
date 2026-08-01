/**
 * @file test_e2e.cpp
 * @brief 端到端测试：serctrld + serctrl 全链路连通性
 *
 * 测试流程：
 * 1. 启动服务器（固定测试端口）
 * 2. 添加测试用户
 * 3. 客户端连接 + 认证
 * 4. 执行远程命令
 * 5. 校验结果
 */

#include "serctrl/auth/authenticator.hpp"
#include "serctrl/transport/server.hpp"
#include "serctrl/transport/client.hpp"
#include "serctrl/protocol/codec.hpp"

#include <catch2/catch_test_macros.hpp>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>

using namespace std::chrono_literals;
namespace fs = std::filesystem;

static std::string prepareAuthFile() {
    auto dir = fs::temp_directory_path() / "serctrl_e2e";
    fs::create_directories(dir);

    serctrl::auth::Authenticator auth(dir / "auth.json");
    // token: 32字节 hex 字符串 "aaaa....bb"
    auth.addUser("test", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb");

    return (dir / "auth.json").string();
}

TEST_CASE("E2E: client connects, authenticates, executes command", "[e2e]") {
    const uint16_t kTestPort = 18080;
    const std::string kToken = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaabbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

    std::string authFile = prepareAuthFile();

    // ── 启动服务器 ──
    serctrl::transport::ServerConfig sc;
    sc.port = kTestPort;
    sc.authFile = authFile;

    serctrl::transport::Server server(sc);

    server.setClientHandler([&server](const std::string& cid, const serctrl::protocol::Message& msg) {
        auto* ep = std::get_if<serctrl::protocol::ExecPayload>(&msg.payload);
        if (ep) {
            serctrl::protocol::Message resp;
            resp.type = serctrl::protocol::MsgType::ExecResult;
            resp.payload = serctrl::protocol::ExecResultPayload{ep->requestId, 0, "hello from e2e", ""};
            server.sendToClient(cid, resp);
        }
    });

    bool serverReady = false;
    std::thread st([&]() { server.start(); serverReady = true; });
    std::this_thread::sleep_for(200ms);

    // ── 启动客户端 ──
    serctrl::transport::Client client;

    bool authOk = false;
    bool execOk = false;
    std::string execOut;

    client.setMessageHandler([&](const serctrl::protocol::Message& msg) {
        auto* ar = std::get_if<serctrl::protocol::AuthResultPayload>(&msg.payload);
        if (ar && ar->ok) {
            authOk = true;
            client.enableEncryption(kToken);
        }
        auto* er = std::get_if<serctrl::protocol::ExecResultPayload>(&msg.payload);
        if (er) {
            execOk = true;
            execOut = er->stdout;
        }
    });

    serctrl::transport::ClientConfig cc;
    cc.host = "127.0.0.1";
    cc.port = kTestPort;
    cc.user = "test";
    cc.token = kToken;

    client.connect(cc);

    // 等待认证
    for (int i = 0; i < 50 && !authOk; ++i) {
        std::this_thread::sleep_for(100ms);
    }
    REQUIRE(authOk);

    // 发送 exec
    serctrl::protocol::Message cmd;
    cmd.type = serctrl::protocol::MsgType::Exec;
    cmd.payload = serctrl::protocol::ExecPayload{"1", "id", 5000};
    client.send(cmd);

    // 等待结果
    for (int i = 0; i < 30 && !execOk; ++i) {
        std::this_thread::sleep_for(100ms);
    }
    REQUIRE(execOk);
    REQUIRE(execOut == "hello from e2e");

    // ── 清理 ──
    client.disconnect();
    server.stop();
    if (st.joinable()) st.join();
    fs::remove_all(fs::temp_directory_path() / "serctrl_e2e");
}