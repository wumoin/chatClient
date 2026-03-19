#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

#include <QMetaType>

namespace chatclient::dto::friendship {

/**
 * @brief 好友域里最小用户信息 DTO。
 */
struct FriendUserDto
{
    QString userId;
    QString account;
    QString nickname;
    QString avatarUrl;
};

/**
 * @brief 账号搜索结果响应 DTO。
 */
struct SearchUserResponseDto
{
    QString requestId;
    bool exists = false;
    FriendUserDto user;
};

/**
 * @brief 发送好友申请请求 DTO。
 */
struct SendFriendRequestRequestDto
{
    QString targetUserId;
    QString requestMessage;
};

/**
 * @brief 好友申请列表项 DTO。
 */
struct FriendRequestItemDto
{
    QString requestId;
    FriendUserDto peerUser;
    QString requestMessage;
    QString status;
    qint64 createdAtMs = 0;
    qint64 handledAtMs = 0;
    bool hasHandledAt = false;
};

using FriendRequestItems = QList<FriendRequestItemDto>;

/**
 * @brief 正式好友列表项 DTO。
 */
struct FriendListItemDto
{
    FriendUserDto user;
    qint64 createdAtMs = 0;
};

using FriendListItems = QList<FriendListItemDto>;

/**
 * @brief 已发送或已收到好友申请列表响应 DTO。
 */
struct FriendRequestListResponseDto
{
    QString requestId;
    FriendRequestItems requests;
};

/**
 * @brief 好友列表响应 DTO。
 */
struct FriendListResponseDto
{
    QString requestId;
    FriendListItems friends;
};

/**
 * @brief 发送好友申请成功响应 DTO。
 */
struct SendFriendRequestResponseDto
{
    QString requestId;
    FriendRequestItemDto request;
};

/**
 * @brief 好友域接口失败 DTO。
 */
struct ApiErrorDto
{
    int httpStatus = 0;
    int errorCode = 0;
    QString message;
    QString requestId;
};

/**
 * @brief 将发送好友申请请求 DTO 转成 JSON 对象。
 * @param request 当前待提交的好友申请请求。
 * @return 可直接序列化发送给服务端的 JSON 对象。
 */
QJsonObject toJsonObject(const SendFriendRequestRequestDto &request);

/**
 * @brief 解析按账号搜索用户成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseSearchUserSuccessResponse(const QJsonObject &root,
                                    SearchUserResponseDto *out,
                                    QString *errorMessage);

/**
 * @brief 解析好友申请列表成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseFriendRequestListSuccessResponse(
    const QJsonObject &root,
    FriendRequestListResponseDto *out,
    QString *errorMessage);

/**
 * @brief 解析好友列表成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseFriendListSuccessResponse(const QJsonObject &root,
                                    FriendListResponseDto *out,
                                    QString *errorMessage);

/**
 * @brief 解析发送好友申请成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseSendFriendRequestSuccessResponse(
    const QJsonObject &root,
    SendFriendRequestResponseDto *out,
    QString *errorMessage);

/**
 * @brief 解析好友域接口失败响应。
 * @param root 服务端响应根 JSON 对象。
 * @param httpStatus 当前 HTTP 状态码。
 * @param fallbackMessage 当响应体不完整时使用的兜底消息。
 * @return 统一的失败 DTO。
 */
ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  int httpStatus,
                                  const QString &fallbackMessage);

}  // namespace chatclient::dto::friendship

Q_DECLARE_METATYPE(chatclient::dto::friendship::SearchUserResponseDto)
Q_DECLARE_METATYPE(chatclient::dto::friendship::FriendRequestItemDto)
Q_DECLARE_METATYPE(chatclient::dto::friendship::FriendRequestItems)
Q_DECLARE_METATYPE(chatclient::dto::friendship::FriendListItemDto)
Q_DECLARE_METATYPE(chatclient::dto::friendship::FriendListItems)
