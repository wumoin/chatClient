#pragma once

#include <QJsonObject>
#include <QString>

#include <QMetaType>

namespace chatclient::dto::conversation {

/**
 * @brief 创建或复用一对一私聊请求 DTO。
 */
struct CreatePrivateConversationRequestDto
{
    QString peerUserId;
};

/**
 * @brief 会话对端用户 DTO。
 */
struct ConversationPeerUserDto
{
    QString userId;
    QString account;
    QString nickname;
    QString avatarUrl;
};

/**
 * @brief 创建或复用一对一私聊成功后返回的最小会话 DTO。
 */
struct ConversationSummaryDto
{
    QString conversationId;
    QString conversationType;
    ConversationPeerUserDto peerUser;
};

/**
 * @brief 创建或复用一对一私聊成功响应 DTO。
 */
struct CreatePrivateConversationResponseDto
{
    QString requestId;
    ConversationSummaryDto conversation;
};

/**
 * @brief 会话域接口失败 DTO。
 */
struct ApiErrorDto
{
    int httpStatus = 0;
    int errorCode = 0;
    QString message;
    QString requestId;
};

/**
 * @brief 将创建私聊请求 DTO 转成 JSON 对象。
 * @param request 当前待提交的创建私聊请求。
 * @return 可直接序列化发送给服务端的 JSON 对象。
 */
QJsonObject toJsonObject(const CreatePrivateConversationRequestDto &request);

/**
 * @brief 解析创建私聊成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseCreatePrivateConversationSuccessResponse(
    const QJsonObject &root,
    CreatePrivateConversationResponseDto *out,
    QString *errorMessage);

/**
 * @brief 解析会话域接口失败响应。
 * @param root 服务端响应根 JSON 对象。
 * @param httpStatus 当前 HTTP 状态码。
 * @param fallbackMessage 当响应体不完整时使用的兜底消息。
 * @return 统一的失败 DTO。
 */
ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  int httpStatus,
                                  const QString &fallbackMessage);

}  // namespace chatclient::dto::conversation

Q_DECLARE_METATYPE(chatclient::dto::conversation::CreatePrivateConversationResponseDto)
