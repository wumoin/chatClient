#pragma once

#include <QJsonObject>
#include <QString>
#include <QVector>

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
 * @brief 查询历史消息请求 DTO。
 */
struct ListConversationMessagesRequestDto
{
    int limit = 50;
    bool hasBeforeSeq = false;
    qint64 beforeSeq = 0;
    bool hasAfterSeq = false;
    qint64 afterSeq = 0;
};

/**
 * @brief 发送文本消息请求 DTO。
 */
struct SendTextMessageRequestDto
{
    QString text;
    QString clientMessageId;
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
 * @brief 当前登录用户在会话中的成员状态 DTO。
 */
struct ConversationMemberDto
{
    QString userId;
    QString memberRole;
    qint64 joinedAtMs = 0;
    qint64 lastReadSeq = 0;
    bool hasLastReadAtMs = false;
    qint64 lastReadAtMs = 0;
};

/**
 * @brief 会话摘要 DTO。
 */
struct ConversationSummaryDto
{
    QString conversationId;
    QString conversationType;
    ConversationPeerUserDto peerUser;
    qint64 lastMessageSeq = 0;
    qint64 lastReadSeq = 0;
    qint64 unreadCount = 0;
    QString lastMessagePreview;
    bool hasLastMessageAtMs = false;
    qint64 lastMessageAtMs = 0;
    qint64 createdAtMs = 0;
};

/**
 * @brief 单条会话详情 DTO。
 */
struct ConversationDetailDto
{
    QString conversationId;
    QString conversationType;
    ConversationPeerUserDto peerUser;
    ConversationMemberDto myMember;
    qint64 lastMessageSeq = 0;
    qint64 unreadCount = 0;
    QString lastMessagePreview;
    bool hasLastMessageAtMs = false;
    qint64 lastMessageAtMs = 0;
    qint64 createdAtMs = 0;
};

/**
 * @brief 会话消息 DTO。
 */
struct ConversationMessageDto
{
    QString messageId;
    QString conversationId;
    qint64 seq = 0;
    QString senderId;
    bool hasClientMessageId = false;
    QString clientMessageId;
    QString messageType;
    QJsonObject content;
    qint64 createdAtMs = 0;
};

/**
 * @brief 创建或复用私聊会话成功响应 DTO。
 */
struct CreatePrivateConversationResponseDto
{
    QString requestId;
    ConversationSummaryDto conversation;
};

/**
 * @brief 会话列表成功响应 DTO。
 */
struct ConversationListResponseDto
{
    QString requestId;
    QVector<ConversationSummaryDto> conversations;
};

/**
 * @brief 单条会话详情成功响应 DTO。
 */
struct ConversationDetailResponseDto
{
    QString requestId;
    ConversationDetailDto conversation;
};

/**
 * @brief 历史消息列表成功响应 DTO。
 */
struct ConversationMessageListResponseDto
{
    QString requestId;
    QVector<ConversationMessageDto> items;
    bool hasMore = false;
    bool hasNextBeforeSeq = false;
    qint64 nextBeforeSeq = 0;
    bool hasNextAfterSeq = false;
    qint64 nextAfterSeq = 0;
};

/**
 * @brief 发送文本消息成功响应 DTO。
 */
struct SendTextMessageResponseDto
{
    QString requestId;
    ConversationMessageDto message;
};

/**
 * @brief 解析单条会话摘要对象。
 * @param object 当前会话摘要对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示结构不符合当前协议。
 */
bool parseConversationSummary(const QJsonObject &object,
                              ConversationSummaryDto *out,
                              QString *errorMessage);

/**
 * @brief 解析单条会话消息对象。
 * @param object 当前消息对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示结构不符合当前协议。
 */
bool parseConversationMessageObject(const QJsonObject &object,
                                    ConversationMessageDto *out,
                                    QString *errorMessage);

/**
 * @brief 将创建私聊请求 DTO 转成 JSON 对象。
 * @param request 当前待提交的创建私聊请求。
 * @return 可直接序列化发送给服务端的 JSON 对象。
 */
QJsonObject toJsonObject(const CreatePrivateConversationRequestDto &request);

/**
 * @brief 将发送文本消息请求 DTO 转成 JSON 对象。
 * @param request 当前待提交的文本消息请求。
 * @return 可直接序列化发送给服务端的 JSON 对象。
 */
QJsonObject toJsonObject(const SendTextMessageRequestDto &request);

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
 * @brief 解析会话列表成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseConversationListSuccessResponse(const QJsonObject &root,
                                          ConversationListResponseDto *out,
                                          QString *errorMessage);

/**
 * @brief 解析单条会话详情成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseConversationDetailSuccessResponse(const QJsonObject &root,
                                            ConversationDetailResponseDto *out,
                                            QString *errorMessage);

/**
 * @brief 解析会话历史消息成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseConversationMessagesSuccessResponse(
    const QJsonObject &root,
    ConversationMessageListResponseDto *out,
    QString *errorMessage);

/**
 * @brief 解析发送文本消息成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseSendTextMessageSuccessResponse(const QJsonObject &root,
                                         SendTextMessageResponseDto *out,
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
Q_DECLARE_METATYPE(chatclient::dto::conversation::ConversationListResponseDto)
Q_DECLARE_METATYPE(chatclient::dto::conversation::ConversationDetailResponseDto)
Q_DECLARE_METATYPE(chatclient::dto::conversation::ConversationMessageListResponseDto)
Q_DECLARE_METATYPE(chatclient::dto::conversation::SendTextMessageResponseDto)
