#pragma once

/**
 * @file codec.hpp
 * @brief 协议编解码器
 *
 * 负责 Message 与二进制帧之间的转换。
 *
 * 两种帧格式：
 * 1. 应用层消息：[type][id][ts][JSON payload] → 用于未加密连接
 * 2. 加密帧：[frameType][nonce][ciphertext] → 用于加密连接
 *
 * 编解码流程：
 *   encode(msg) → [type|id|ts|JSON] → packEncrypted() → [0x01|nonce|密文]
 *   decode([0x01|nonce|密文]) → unpackFrame() → [type|id|ts|JSON] → decode()
 */

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "serctrl/protocol/message.hpp"

namespace serctrl::protocol {

/** @brief 明文帧标识 */
constexpr uint8_t FRAME_PLAIN = 0x00;
/** @brief 加密帧标识 */
constexpr uint8_t FRAME_ENCRYPTED = 0x01;

/**
 * @brief 编码消息为 JSON 字节流
 * @param msg 要编码的消息
 * @return JSON 字节序列
 */
std::vector<uint8_t> encode(const Message& msg);

/**
 * @brief 从字节流解码消息
 * @param data 输入数据（JSON 格式）
 * @return 解码后的消息，失败返回 std::nullopt
 */
std::optional<Message> decode(std::span<const uint8_t> data);

/**
 * @brief 编码加密帧
 * @param frameType 帧类型（FRAME_PLAIN 或 FRAME_ENCRYPTED）
 * @param nonce 随机数（加密时使用）
 * @param ciphertext 密文数据
 * @return 完整帧：[frameType][nonce][ciphertext]
 */
std::vector<uint8_t> encodeFrame(uint8_t frameType, std::span<const uint8_t> nonce,
                                  std::span<const uint8_t> ciphertext);

/**
 * @brief 解码帧
 * @param data 完整帧数据
 * @return {帧类型, 载荷数据}，失败返回 std::nullopt
 */
std::optional<std::pair<uint8_t, std::vector<uint8_t>>> decodeFrame(std::span<const uint8_t> data);

}  // namespace serctrl::protocol
