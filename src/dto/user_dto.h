#pragma once

#include <QJsonObject>
#include <QString>

namespace chatclient::dto::user {

/**
 * @brief 临时头像上传成功响应 DTO。
 */
struct TemporaryAvatarUploadResponseDto
{
    QString requestId;
    QString avatarUploadKey;
    QString previewUrl;
};

/**
 * @brief 用户相关接口失败 DTO。
 *
 * 统一承接 HTTP 状态码、服务端业务码、错误消息和 `request_id`，
 * 便于窗口层把错误进一步映射成中文提示。
 */
struct ApiErrorDto
{
    int httpStatus = 0;
    int errorCode = 0;
    QString message;
    QString requestId;
};

/**
 * @brief 解析临时头像上传成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseTemporaryAvatarUploadSuccessResponse(
    const QJsonObject &root,
    TemporaryAvatarUploadResponseDto *out,
    QString *errorMessage);

/**
 * @brief 解析用户相关接口失败响应。
 * @param root 服务端响应根 JSON 对象。
 * @param httpStatus 当前 HTTP 状态码。
 * @param fallbackMessage 当响应体不完整时使用的兜底消息。
 * @return 统一的失败 DTO。
 */
ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  int httpStatus,
                                  const QString &fallbackMessage);

}  // namespace chatclient::dto::user
