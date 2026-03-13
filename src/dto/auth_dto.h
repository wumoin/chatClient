#pragma once

#include <QJsonObject>
#include <QString>

#include <QMetaType>

namespace chatclient::dto::auth {

/**
 * @brief 注册请求 DTO，承接客户端提交到服务端的注册字段。
 */
struct RegisterRequestDto
{
    QString account;
    QString password;
    QString nickname;
    QString avatarUrl;
};

/**
 * @brief 注册成功后返回的用户信息 DTO。
 */
struct RegisterUserDto
{
    QString userId;
    QString account;
    QString nickname;
    QString avatarUrl;
    qint64 createdAtMs = 0;
};

/**
 * @brief 注册成功响应 DTO。
 *
 * 这个结构承接服务端统一响应体中 `data.user` 的内容，
 * 以及服务端回显的 `request_id`。
 */
struct RegisterResponseDto
{
    QString requestId;
    RegisterUserDto user;
};

/**
 * @brief 认证接口失败 DTO。
 *
 * 统一承接 HTTP 状态码、服务端业务码、错误消息和 `request_id`，
 * 便于 service 层继续映射成界面提示。
 */
struct ApiErrorDto
{
    int httpStatus = 0;
    int errorCode = 0;
    QString message;
    QString requestId;
};

/**
 * @brief 将注册请求 DTO 转成 JSON 对象。
 * @param request 客户端准备提交的注册请求。
 * @return 可直接序列化发送给服务端的 JSON 对象。
 */
QJsonObject toJsonObject(const RegisterRequestDto &request);

/**
 * @brief 解析服务端返回的注册成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的响应 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseRegisterSuccessResponse(const QJsonObject &root,
                                  RegisterResponseDto *out,
                                  QString *errorMessage);

/**
 * @brief 解析认证接口失败响应。
 * @param root 服务端响应根 JSON 对象。
 * @param httpStatus 当前 HTTP 状态码。
 * @param fallbackMessage 当响应体不完整时使用的兜底消息。
 * @return 统一的失败 DTO。
 */
ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  int httpStatus,
                                  const QString &fallbackMessage);

}  // namespace chatclient::dto::auth

Q_DECLARE_METATYPE(chatclient::dto::auth::RegisterUserDto)
