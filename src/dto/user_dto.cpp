#include "dto/user_dto.h"

#include <QJsonValue>

namespace chatclient::dto::user {
namespace {

/**
 * @brief 读取 JSON 对象中的必填字符串字段。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @param out 成功时写入字符串值。
 * @param errorMessage 失败时写入错误原因。
 * @return true 表示字段存在且类型正确；false 表示字段缺失或类型不匹配。
 */
bool readRequiredString(const QJsonObject &object,
                        const QString &key,
                        QString *out,
                        QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是字符串").arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = value.toString();
    }
    return true;
}

/**
 * @brief 读取 JSON 对象中的可选字符串字段。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @return 如果字段存在且是字符串则返回其值；否则返回空字符串。
 */
QString readOptionalString(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

}  // namespace

bool parseTemporaryAvatarUploadSuccessResponse(
    const QJsonObject &root,
    TemporaryAvatarUploadResponseDto *out,
    QString *errorMessage)
{
    if (root.value(QStringLiteral("code")).toInt(-1) != 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("服务端返回了非成功业务码");
        }
        return false;
    }

    const QJsonValue dataValue = root.value(QStringLiteral("data"));
    if (!dataValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 data 对象");
        }
        return false;
    }

    TemporaryAvatarUploadResponseDto parsedResponse;
    parsedResponse.requestId =
        readOptionalString(root, QStringLiteral("request_id"));

    const QJsonObject dataObject = dataValue.toObject();
    if (!readRequiredString(dataObject,
                            QStringLiteral("avatar_upload_key"),
                            &parsedResponse.avatarUploadKey,
                            errorMessage) ||
        !readRequiredString(dataObject,
                            QStringLiteral("preview_url"),
                            &parsedResponse.previewUrl,
                            errorMessage))
    {
        return false;
    }

    if (out)
    {
        *out = parsedResponse;
    }
    return true;
}

ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  const int httpStatus,
                                  const QString &fallbackMessage)
{
    ApiErrorDto error;
    error.httpStatus = httpStatus;
    error.errorCode = root.value(QStringLiteral("code")).toInt(0);

    const QString message = root.value(QStringLiteral("message")).toString();
    error.message = message.isEmpty() ? fallbackMessage : message;

    error.requestId = root.value(QStringLiteral("request_id")).toString();
    return error;
}

}  // namespace chatclient::dto::user
