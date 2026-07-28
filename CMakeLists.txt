cmake_minimum_required(VERSION 3.20)
project(serctrl
    VERSION 0.1.0
    DESCRIPTION "Encrypted remote shell over WebSocket"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Homebrew prefix for Apple Silicon
if(APPLE)
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# ── Options ──────────────────────────────────────────────
option(SERCTRL_BUILD_TESTS "Build tests" ON)
option(SERCTRL_INSTALL "Generate install targets" ON)

option(SERCTRL_VENDOR_DEPS "Use vendored third-party dependencies" ON)

# ── Dependencies ─────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

find_package(Sodium REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(nlohmann_json REQUIRED)

# uSockets（vendor）
set(USOCKETS_DIR "${CMAKE_SOURCE_DIR}/third_party/usockets")
set(USOCKETS_SRC
    ${USOCKETS_DIR}/bsd.c
    ${USOCKETS_DIR}/loop.c
    ${USOCKETS_DIR}/socket.c
    ${USOCKETS_DIR}/context.c
    ${USOCKETS_DIR}/udp.c
    ${USOCKETS_DIR}/quic.c
    ${USOCKETS_DIR}/crypto/openssl.c
    ${USOCKETS_DIR}/crypto/sni_tree.cpp
    ${USOCKETS_DIR}/eventing/epoll_kqueue.c
)
add_library(usockets STATIC ${USOCKETS_SRC})
target_include_directories(usockets PUBLIC "${USOCKETS_DIR}")
target_link_libraries(usockets PUBLIC OpenSSL::SSL OpenSSL::Crypto ZLIB::ZLIB)
target_compile_definitions(usockets PRIVATE LIBUS_USE_OPENSSL)
if(APPLE)
    target_link_libraries(usockets PUBLIC "-framework CoreFoundation" "-framework CoreServices")
    set_target_properties(usockets PROPERTIES COMPILE_FLAGS "-Wno-everything")
endif()

# uWebSockets（vendor，header-only）
set(UWS_DIR "${CMAKE_SOURCE_DIR}/third_party/uwebsockets")

# IXWebSocket（vendor）
set(IXWS_DIR "${CMAKE_SOURCE_DIR}/third_party/ixwebsocket")
set(IXWS_SRC
    ${IXWS_DIR}/IXCancellationRequest.cpp
    ${IXWS_DIR}/IXConnectionState.cpp
    ${IXWS_DIR}/IXDNSLookup.cpp
    ${IXWS_DIR}/IXExponentialBackoff.cpp
    ${IXWS_DIR}/IXHttp.cpp
    ${IXWS_DIR}/IXNetSystem.cpp
    ${IXWS_DIR}/IXSelectInterrupt.cpp
    ${IXWS_DIR}/IXSelectInterruptFactory.cpp
    ${IXWS_DIR}/IXSelectInterruptPipe.cpp
    ${IXWS_DIR}/IXSocket.cpp
    ${IXWS_DIR}/IXSocketConnect.cpp
    ${IXWS_DIR}/IXSocketFactory.cpp
    ${IXWS_DIR}/IXSocketTLSOptions.cpp
    ${IXWS_DIR}/IXUrlParser.cpp
    ${IXWS_DIR}/IXUserAgent.cpp
    ${IXWS_DIR}/IXWebSocket.cpp
    ${IXWS_DIR}/IXWebSocketCloseConstants.cpp
    ${IXWS_DIR}/IXWebSocketHandshake.cpp
    ${IXWS_DIR}/IXWebSocketHttpHeaders.cpp
    ${IXWS_DIR}/IXWebSocketPerMessageDeflate.cpp
    ${IXWS_DIR}/IXWebSocketPerMessageDeflateCodec.cpp
    ${IXWS_DIR}/IXWebSocketPerMessageDeflateOptions.cpp
    ${IXWS_DIR}/IXWebSocketTransport.cpp
)
if(APPLE)
    list(APPEND IXWS_SRC ${IXWS_DIR}/IXSocketAppleSSL.cpp ${IXWS_DIR}/apple/IXSetThreadName_apple.cpp)
else()
    list(APPEND IXWS_SRC ${IXWS_DIR}/IXSocketOpenSSL.cpp ${IXWS_DIR}/linux/IXSetThreadName_linux.cpp)
endif()
add_library(ixwebsocket STATIC ${IXWS_SRC})
target_include_directories(ixwebsocket PUBLIC "${CMAKE_SOURCE_DIR}/third_party")
target_link_libraries(ixwebsocket PUBLIC OpenSSL::SSL OpenSSL::Crypto)
if(APPLE)
    target_link_libraries(ixwebsocket PUBLIC "-framework CoreFoundation" "-framework CoreServices" "-framework Security")
endif()

# ── Library (shared sources) ────────────────────────────
set(SERCTRL_SOURCES
    src/shared/message.cpp
    src/shared/encryptor.cpp
    src/shared/kdf.cpp
    src/shared/argon2.cpp
    src/shared/authenticator.cpp
    src/shared/config.cpp
    src/shared/encrypted_store.cpp
    src/shared/pty.cpp
    src/shared/session.cpp
    src/shared/file_transfer.cpp
    src/shared/batch.cpp
    src/shared/connection.cpp
    src/shared/server.cpp
    src/shared/client.cpp
    src/shared/updater.cpp
    src/shared/error.cpp
    src/shared/logger.cpp
    src/shared/signal.cpp
)

add_library(serctrl_lib STATIC ${SERCTRL_SOURCES})

target_include_directories(serctrl_lib
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${UWS_DIR}
        ${USOCKETS_DIR}
        ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(serctrl_lib
    PUBLIC
        Sodium::Sodium
        nlohmann_json::nlohmann_json
        usockets
        ixwebsocket
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
)

if(APPLE)
    target_link_libraries(serctrl_lib PUBLIC "-framework CoreFoundation" "-framework CoreServices" "-framework Security")
endif()

# ── Executables ──────────────────────────────────────────
add_executable(serctrl src/serctrl/main.cpp)
target_link_libraries(serctrl PRIVATE serctrl_lib)

add_executable(serctrld src/serctrld/main.cpp)
target_link_libraries(serctrld PRIVATE serctrl_lib)

# ── Tests ────────────────────────────────────────────────
if(SERCTRL_BUILD_TESTS)
    enable_testing()
    find_package(Catch2 REQUIRED)
    add_subdirectory(tests)
endif()

# ── Install ──────────────────────────────────────────────
if(SERCTRL_INSTALL)
    include(GNUInstallDirs)

    install(TARGETS serctrl serctrld
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY include/serctrl
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
    message(STATUS "Binaries will be installed to: ${CMAKE_INSTALL_FULL_BINDIR}")
endif()
