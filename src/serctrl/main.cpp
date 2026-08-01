/**
 * @file serctrl/main.cpp
 * @brief serctrl 客户端 CLI 实现
 *
 * SSH 风格的命令行界面：
 * - serctrl user@host              交互式 shell
 * - serctrl user@host exec <cmd>   执行单条命令
 * - serctrl user@host put <l> <r>  上传文件
 * - serctrl user@host get <r> <l>  下载文件
 * - serctrl config add <n> <u@h>   添加主机
 * - serctrl config remove <n>      移除主机
 * - serctrl config list            列出主机
 * - serctrl keygen                 生成令牌
 * - serctrl update [--version <v>] 更新二进制
 * - serctrl rollback               回滚更新
 */

#include "serctrl/config/config.hpp"
#include "serctrl/config/encrypted_store.hpp"
#include "serctrl/auth/authenticator.hpp"
#include "serctrl/transport/client.hpp"
#include "serctrl/update/updater.hpp"
#include "serctrl/util/logger.hpp"
#include "serctrl/util/error.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <signal.h>

static void printBanner() {
    std::cout << "SerCTRL v0.1.1" << std::endl;
    std::cout << "Encrypted Remote Shell" << std::endl << std::endl;
}

static void printUsage() {
    std::cout << R"(
Usage: serctrl <user@host>                Interactive shell
       serctrl <user@host> exec <cmd>    Run one command
       serctrl <user@host> put <l> <r>   Upload file
       serctrl <user@host> get <r> <l>   Download file
       serctrl config add <n> <u@h>      Add host
       serctrl config remove <n>         Remove host
       serctrl config list               List hosts
       serctrl keygen                    Generate token
       serctrl update [--version <v>]    Update binary
       serctrl rollback                  Rollback update
       serctrl help                      Show this message
)" << std::endl;
}

static void cmdConfig(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        printUsage();
        return;
    }

    serctrl::config::Config config;
    const std::string& sub = args[1];

    if (sub == "add") {
        if (args.size() < 4) {
            std::cerr << "Usage: serctrl config add <name> <user@host>" << std::endl;
            return;
        }
        auto target = serctrl::config::Config::parseTarget(args[3]);
        serctrl::config::HostConfig host;
        host.user = target.user;
        host.host = target.host;
        host.port = target.port;
        config.setHost(args[2], host);
        std::cout << "Host '" << args[2] << "' added." << std::endl;

        // 提示生成 token
        std::cout << "Run 'serctrl keygen' to generate a token, then add user on server:" << std::endl;
        std::cout << "  serctrld user add " << target.user << " -p <token>" << std::endl;
    } else if (sub == "remove") {
        if (args.size() < 3) {
            std::cerr << "Usage: serctrl config remove <name>" << std::endl;
            return;
        }
        if (config.removeHost(args[2])) {
            std::cout << "Host '" << args[2] << "' removed." << std::endl;
        } else {
            std::cerr << "Host '" << args[2] << "' not found." << std::endl;
        }
    } else if (sub == "list") {
        auto hosts = config.listHosts();
        if (hosts.empty()) {
            std::cout << "No hosts configured." << std::endl;
            return;
        }
        std::cout << "Configured hosts:" << std::endl;
        for (const auto& name : hosts) {
            auto host = config.getHost(name);
            std::cout << "  " << name << " -> " << host->user << "@" << host->host;
            if (host->port != 8080) {
                std::cout << ":" << host->port;
            }
            std::cout << std::endl;
        }
    } else {
        std::cerr << "Unknown config command: " << sub << std::endl;
    }
}

static void cmdKeygen() {
    auto token = serctrl::auth::Authenticator::generateToken();
    std::string hex = serctrl::auth::Authenticator::tokenToHex(token);
    std::cout << "Token: " << hex << std::endl;
    std::cout << std::endl;
    std::cout << "On the server, add this user:" << std::endl;
    std::cout << "  serctrld user add <username> -p " << hex << std::endl;
}

static void cmdUpdate(const std::vector<std::string>& args) {
    serctrl::update::Updater updater("lantrui", "serctrl");

    std::string version;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--version" && i + 1 < args.size()) {
            version = args[i + 1];
            i++;
        }
    }

    updater.setProgressCallback([](int percent, const std::string& stage) {
        std::cout << "\r[" << percent << "%] " << stage << std::flush;
    });

    if (updater.updateTo(version)) {
        std::cout << "\nUpdate successful!" << std::endl;
    } else {
        std::cerr << "\nUpdate failed." << std::endl;
    }
}

static void cmdRollback() {
    serctrl::update::Updater updater("lantrui", "serctrl");
    if (updater.rollback()) {
        std::cout << "Rollback successful!" << std::endl;
    } else {
        std::cerr << "Rollback failed." << std::endl;
    }
}

/**
 * @brief 连接到服务器
 * @param target 连接目标（user@host 或配置名）
 * @param subcommand exec/put/get
 * @param subArgs 子命令参数
 */
static int cmdConnect(const std::string& target,
                      const std::string& subcommand,
                      const std::vector<std::string>& subArgs) {
    serctrl::config::Config config;
    serctrl::transport::ClientConfig clientConfig;

    // 如果 target 包含 @，直接解析
    if (target.find('@') != std::string::npos) {
        auto parsed = serctrl::config::Config::parseTarget(target);
        clientConfig.host = parsed.host;
        clientConfig.port = parsed.port;
        clientConfig.user = parsed.user;
    } else {
        // 尝试从配置中查找
        auto host = config.getHost(target);
        if (!host) {
            std::cerr << "Host '" << target << "' not found in config." << std::endl;
            std::cerr << "Use 'serctrl config add " << target << " <user@host>' first." << std::endl;
            return 1;
        }
        clientConfig.host = host->host;
        clientConfig.port = host->port;
        clientConfig.user = host->user;
        clientConfig.token = host->token;
    }

    // 如果没有 token，提示
    if (clientConfig.token.empty()) {
        std::cout << "No token found for this host." << std::endl;
        std::cout << "Run 'serctrl keygen' to generate one, then:" << std::endl;
        std::cout << "  serctrld user add " << clientConfig.user << " -p <token>" << std::endl;
        std::cout << "  serctrl config add " << target << " " << clientConfig.user << "@" 
                  << clientConfig.host << ":" << clientConfig.port << std::endl;
        return 1;
    }

    // 创建客户端
    serctrl::transport::Client client;

    bool authOk = false;
    std::string authError;

    // 处理认证响应
    client.setMessageHandler([&](const serctrl::protocol::Message& msg) {
        if (msg.type == serctrl::protocol::MsgType::AuthResult) {
            auto* result = std::get_if<serctrl::protocol::AuthResultPayload>(&msg.payload);
            if (result) {
                authOk = result->ok;
                authError = result->error;
                // 认证成功后启用加密
                if (result->ok && !clientConfig.token.empty()) {
                    client.enableEncryption(clientConfig.token);
                }
            }
        } else if (msg.type == serctrl::protocol::MsgType::ExecResult) {
            auto* result = std::get_if<serctrl::protocol::ExecResultPayload>(&msg.payload);
            if (result) {
                if (result->exitCode != 0) {
                    std::cout << "[exit: " << result->exitCode << "]" << std::endl;
                }
                if (!result->stdout.empty()) {
                    std::cout << result->stdout;
                }
                if (!result->stderr.empty()) {
                    std::cerr << result->stderr;
                }
            }
        } else if (msg.type == serctrl::protocol::MsgType::Error) {
            auto* err = std::get_if<serctrl::protocol::ErrorPayload>(&msg.payload);
            if (err) {
                std::cerr << "Server error: " << err->message << std::endl;
            }
        }
    });

    client.setErrorHandler([&](const std::string& error) {
        std::cerr << "Connection error: " << error << std::endl;
    });

    // 连接
    std::cout << "Connecting to " << clientConfig.user << "@" << clientConfig.host 
              << ":" << clientConfig.port << "..." << std::endl;

    if (!client.connect(clientConfig)) {
        std::cerr << "Failed to initiate connection." << std::endl;
        return 1;
    }

    // 等待认证结果
    int waitCount = 0;
    while (!authOk && authError.empty() && waitCount < 50) {
        usleep(100000);  // 100ms
        waitCount++;
    }

    if (!authOk) {
        if (authError.empty()) {
            std::cerr << "Authentication timed out." << std::endl;
        } else {
            std::cerr << "Authentication failed: " << authError << std::endl;
        }
        client.disconnect();
        return 1;
    }

    std::cout << "Authenticated as " << clientConfig.user << std::endl;

    // 处理子命令
    if (subcommand == "exec") {
        if (subArgs.empty()) {
            std::cerr << "Usage: serctrl <user@host> exec <command>" << std::endl;
            client.disconnect();
            return 1;
        }
        // 拼接命令
        std::string cmd;
        for (size_t i = 0; i < subArgs.size(); ++i) {
            if (i > 0) cmd += " ";
            cmd += subArgs[i];
        }

        serctrl::protocol::Message msg;
        msg.type = serctrl::protocol::MsgType::Exec;
        msg.payload = serctrl::protocol::ExecPayload{"1", cmd, 30000};
        client.send(msg);

        // 等待结果
        sleep(5);
        client.disconnect();
        return 0;
    }

    // 交互式 shell 模式
    std::cout << "Connected. Type 'exit' to disconnect." << std::endl;
    std::cout << std::endl;

    // 简单交互：读取 stdin 发送到服务器
    // TODO: 完整的 PTY 交互（raw mode, resize 等）
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == "exit" || line == "quit") {
            break;
        }

        serctrl::protocol::Message msg;
        msg.type = serctrl::protocol::MsgType::Exec;
        msg.payload = serctrl::protocol::ExecPayload{"0", line, 10000};
        client.send(msg);

        // 等待响应
        sleep(1);
    }

    client.disconnect();
    std::cout << "Disconnected." << std::endl;

    return 0;
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
    } else if (cmd == "config") {
        cmdConfig(args);
    } else if (cmd == "keygen") {
        cmdKeygen();
    } else if (cmd == "update") {
        cmdUpdate(args);
    } else if (cmd == "rollback") {
        cmdRollback();
    } else if (cmd.find('@') != std::string::npos) {
        // 连接到服务器（SSH 风格）
        std::string subcommand;
        std::vector<std::string> subArgs;

        if (args.size() > 1) {
            subcommand = args[1];
            for (size_t i = 2; i < args.size(); ++i) {
                subArgs.push_back(args[i]);
            }
        }

        return cmdConnect(cmd, subcommand, subArgs);
    } else {
        // 尝试作为配置名连接
        std::string subcommand;
        std::vector<std::string> subArgs;

        if (args.size() > 1) {
            subcommand = args[1];
            for (size_t i = 2; i < args.size(); ++i) {
                subArgs.push_back(args[i]);
            }
        }

        return cmdConnect(cmd, subcommand, subArgs);
    }

    return 0;
}
