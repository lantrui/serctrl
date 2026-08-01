/**
 * @file serctrld/main.cpp
 * @brief serctrld 守护进程 CLI 实现
 *
 * 服务器端守护进程命令：
 * - serctrld start              启动守护进程
 * - serctrld start -P <port>    指定端口启动
 * - serctrld start -r <dir>     指定沙箱根目录
 * - serctrld user add <n> -p <pass>  添加用户
 * - serctrld user list          列出用户
 * - serctrld user remove <n>    移除用户
 */

#include "serctrl/auth/authenticator.hpp"
#include "serctrl/transport/server.hpp"
#include "serctrl/util/logger.hpp"
#include "serctrl/util/signal.hpp"
#include "serctrl/util/error.hpp"
#include "serctrl/shell/session.hpp"
#include "serctrl/protocol/codec.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <array>
#include <cstdio>

static void printBanner() {
    std::cout << "SerCTRL v0.1.1" << std::endl;
    std::cout << "Encrypted Remote Shell Daemon" << std::endl << std::endl;
}

static void printUsage() {
    std::cout << R"(
Usage: serctrld start                   Start daemon
       serctrld start -P <port>         Start on custom port
       serctrld start -r <dir>          Start with sandbox root
       serctrld user add <n> -p <pass>  Add user
       serctrld user list               List users
       serctrld user remove <n>         Remove user
       serctrld help                    Show this message
)" << std::endl;
}

static void cmdUser(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printUsage();
        return;
    }

    serctrl::auth::Authenticator auth(
        std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / ".serctrl" / "auth.json");

    const std::string& sub = args[1];

    if (sub == "add") {
        if (args.size() < 5) {
            std::cerr << "Usage: serctrld user add <name> -p <password>" << std::endl;
            return;
        }
        std::string name = args[2];
        std::string password;
        for (size_t i = 3; i < args.size(); ++i) {
            if (args[i] == "-p" && i + 1 < args.size()) {
                password = args[i + 1];
                break;
            }
        }
        if (password.empty()) {
            std::cerr << "Password required with -p flag" << std::endl;
            return;
        }
        if (auth.addUser(name, password)) {
            std::cout << "User '" << name << "' added." << std::endl;
            auto token = serctrl::auth::Authenticator::generateToken();
            std::cout << "Token: " << serctrl::auth::Authenticator::tokenToHex(token) << std::endl;
        } else {
            std::cerr << "User '" << name << "' already exists." << std::endl;
        }
    } else if (sub == "list") {
        auto users = auth.listUsers();
        if (users.empty()) {
            std::cout << "No users configured." << std::endl;
            return;
        }
        std::cout << "Users:" << std::endl;
        for (const auto& user : users) {
            std::cout << "  " << user << std::endl;
        }
    } else if (sub == "remove") {
        if (args.size() < 3) {
            std::cerr << "Usage: serctrld user remove <name>" << std::endl;
            return;
        }
        if (auth.removeUser(args[2])) {
            std::cout << "User '" << args[2] << "' removed." << std::endl;
        } else {
            std::cerr << "User '" << args[2] << "' not found." << std::endl;
        }
    } else {
        std::cerr << "Unknown user command: " << sub << std::endl;
    }
}

/**
 * @brief 执行 shell 命令并返回结果
 * @param command 要执行的命令
 * @return {exitCode, stdout, stderr}
 */
static std::tuple<int, std::string, std::string> runCommand(const std::string& command) {
    std::array<char, 4096> buffer;
    std::string stdoutStr, stderrStr;

    std::string fullCmd = command + " 2>/tmp/serctrl_stderr.XXXXXX";

    auto* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return {-1, "", "Failed to execute command"};
    }

    // 读取 stdout
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        stdoutStr += buffer.data();
    }

    int exitCode = pclose(pipe);

    // 读取 stderr（如果有的话用另一个方式）
    return {exitCode, stdoutStr, stderrStr};
}

static void cmdStart(const std::vector<std::string>& args) {
    serctrl::transport::ServerConfig config;
    config.port = 8080;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-P" && i + 1 < args.size()) {
            config.port = std::stoi(args[i + 1]);
            i++;
        } else if (args[i] == "-r" && i + 1 < args.size()) {
            config.rootDir = args[i + 1];
            i++;
        } else if (args[i] == "-a" && i + 1 < args.size()) {
            config.authFile = args[i + 1];
            i++;
        }
    }

    if (config.authFile.empty()) {
        config.authFile = (std::filesystem::path(getenv("HOME") ? getenv("HOME") : ".") / ".serctrl" / "auth.json").string();
    }

    serctrl::transport::Server server(config);

    // 会话管理（ShellOpen/ShellData/ShellClose 共享）
    struct ShellSession {
        std::unique_ptr<serctrl::shell::Session> session;
        std::unique_ptr<std::thread> readerThread;
        std::string clientId;
    };
    std::unordered_map<std::string, std::shared_ptr<ShellSession>> sessions;
    std::mutex sessionsLock;

    // 注册客户端消息处理器
    server.setClientHandler([&server, &sessions, &sessionsLock](const std::string& clientId, const serctrl::protocol::Message& msg) {
        switch (msg.type) {
            case serctrl::protocol::MsgType::Exec: {
                // 执行远程命令
                auto* execPayload = std::get_if<serctrl::protocol::ExecPayload>(&msg.payload);
                if (!execPayload) break;

                auto [exitCode, stdoutStr, stderrStr] = runCommand(execPayload->command);

                serctrl::protocol::Message resp;
                resp.type = serctrl::protocol::MsgType::ExecResult;
                resp.payload = serctrl::protocol::ExecResultPayload{
                    execPayload->requestId, exitCode, stdoutStr, stderrStr
                };
                server.sendToClient(clientId, resp);
                break;
            }

            case serctrl::protocol::MsgType::ShellOpen: {
                auto* openPayload = std::get_if<serctrl::protocol::ShellOpenPayload>(&msg.payload);
                if (!openPayload) break;

                std::string sid = std::to_string(
                    std::chrono::steady_clock::now().time_since_epoch().count());

                auto ss = std::make_shared<ShellSession>();
                ss->session = std::make_unique<serctrl::shell::Session>(sid,
                    serctrl::shell::PtyOptions{openPayload->cols, openPayload->rows});
                ss->clientId = clientId;

                if (!ss->session->isRunning()) {
                    serctrl::protocol::Message resp;
                    resp.type = serctrl::protocol::MsgType::ShellOpenResult;
                    resp.payload = serctrl::protocol::ShellOpenResultPayload{openPayload->requestId, "", false};
                    server.sendToClient(clientId, resp);
                    break;
                }

                // PTY 读取线程
                ss->readerThread = std::make_unique<std::thread>([ss, sid, &server]() {
                    char buf[4096];
                    while (ss->session->isRunning()) {
                        auto n = ss->session->readPTY(buf, sizeof(buf));
                        if (n > 0) {
                            serctrl::protocol::Message resp;
                            resp.type = serctrl::protocol::MsgType::ShellData;
                            resp.payload = serctrl::protocol::ShellDataPayload{sid, std::string(buf, (size_t)n)};
                            server.sendToClient(ss->clientId, resp);
                        } else if (n < 0) {
                            break;
                        }
                        usleep(10000);
                    }
                });

                {
                    std::lock_guard<std::mutex> lock(sessionsLock);
                    sessions[sid] = ss;
                }

                serctrl::protocol::Message resp;
                resp.type = serctrl::protocol::MsgType::ShellOpenResult;
                resp.payload = serctrl::protocol::ShellOpenResultPayload{openPayload->requestId, sid, true};
                server.sendToClient(clientId, resp);
                LOG_INFO("Shell opened: %s", sid.c_str());
                break;
            }

            case serctrl::protocol::MsgType::ShellData: {
                auto* dp = std::get_if<serctrl::protocol::ShellDataPayload>(&msg.payload);
                if (!dp) break;
                std::shared_ptr<ShellSession> ss;
                {
                    std::lock_guard<std::mutex> lock(sessionsLock);
                    auto it = sessions.find(dp->sessionId);
                    if (it == sessions.end()) break;
                    ss = it->second;
                }
                ss->session->write(dp->data);
                break;
            }

            case serctrl::protocol::MsgType::ShellResize: {
                auto* rp = std::get_if<serctrl::protocol::ShellResizePayload>(&msg.payload);
                if (!rp) break;
                std::lock_guard<std::mutex> lock(sessionsLock);
                auto it = sessions.find(rp->sessionId);
                if (it != sessions.end()) {
                    it->second->session->resize(rp->cols, rp->rows);
                }
                break;
            }

            case serctrl::protocol::MsgType::ShellClose: {
                auto* cp = std::get_if<serctrl::protocol::ShellClosePayload>(&msg.payload);
                if (!cp) break;
                std::lock_guard<std::mutex> lock(sessionsLock);
                auto it = sessions.find(cp->sessionId);
                if (it != sessions.end()) {
                    it->second->session->close();
                    if (it->second->readerThread && it->second->readerThread->joinable()) {
                        it->second->readerThread->join();
                    }
                    sessions.erase(it);
                    LOG_INFO("Shell closed: %s", cp->sessionId.c_str());
                }
                break;
            }

            case serctrl::protocol::MsgType::Ping: {
                // 心跳响应
                serctrl::protocol::Message resp;
                resp.type = serctrl::protocol::MsgType::Pong;
                resp.payload = serctrl::protocol::PongPayload{};
                server.sendToClient(clientId, resp);
                break;
            }

            default: {
                LOG_DEBUG("Unhandled message type from %s: %d",
                          clientId.c_str(), static_cast<int>(msg.type));
                break;
            }
        }
    });

    // 优雅退出
    serctrl::SignalHandler::install([&server](int sig) {
        LOG_INFO("Received signal %d, shutting down...", sig);
        server.stop();
    });

    server.start();
    LOG_INFO("Server started on port %d", config.port);

    // 主循环等待信号
    while (true) {
        sleep(1);
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printBanner();
        printUsage();
        return 1;
    }

    std::vector<std::string> args(argv + 1, argv + argc);
    const std::string& cmd = args[0];

    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        printBanner();
        printUsage();
    } else if (cmd == "user") {
        cmdUser(args);
    } else if (cmd == "start") {
        cmdStart(args);
    } else {
        std::cerr << "Unknown command: " << cmd << std::endl;
        printUsage();
        return 1;
    }

    return 0;
}
