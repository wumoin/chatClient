#include "dto/auth_dto.h"

#include <QJsonValue>

namespace chatclient::dto::auth {
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

QJsonObject toJsonObject(const RegisterRequestDto &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("account"), request.account);
    object.insert(QStringLiteral("password"), request.password);
    object.insert(QStringLiteral("nickname"), request.nickname);

    if (!request.avatarUrl.isEmpty())
    {
        object.insert(QStringLiteral("avatar_url"), request.avatarUrl);
    }

    return object;
}

bool parseRegisterSuccessResponse(const QJsonObject &root,
                                  RegisterResponseDto *out,
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

    const QJsonObject dataObject = dataValue.toObject();
    const QJsonValue userValue = dataObject.value(QStringLiteral("user"));
    if (!userValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 user 对象");
        }
        return false;
    }

    RegisterResponseDto parsedResponse;
    parsedResponse.requestId = readOptionalString(root, QStringLiteral("request_id"));

    const QJsonObject userObject = userValue.toObject();
    if (!readRequiredString(userObject,
                            QStringLiteral("user_id"),
                            &parsedResponse.user.userId,
                            errorMessage) ||
        !readRequiredString(userObject,
                            QStringLiteral("account"),
                            &parsedResponse.user.account,
                            errorMessage) ||
        !readRequiredString(userObject,
                            QStringLiteral("nickname"),
                            &parsedResponse.user.nickname,
                            errorMessage))
    {
        return false;
    }

    parsedResponse.user.avatarUrl =
        readOptionalString(userObject, QStringLiteral("avatar_url"));

    const QJsonValue createdAtValue = userObject.value(QStringLiteral("created_at_ms"));
    if (!createdAtValue.isDouble())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("响应字段 created_at_ms 缺失或不是数字");
        }
        return false;
    }

    parsedResponse.user.createdAtMs = static_cast<qint64>(createdAtValue.toDouble());

    if (out)
    {
        *out = parsedResponse;
    }
    return true;
}

ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  int httpStatus,
                                  const QString &fallbackMessage)
{
    ApiErrorDto error;
    error.httpStatus = httpStatus;

    const QJsonValue codeValue = root.value(QStringLiteral("code"));
    if (codeValue.isDouble())
    {
        error.errorCode = codeValue.toInt();
    }

    error.message = readOptionalString(root, QStringLiteral("message"));
    error.requestId = readOptionalString(root, QStringLiteral("request_id"));

    if (error.message.isEmpty())
    {
        error.message = fallbackMessage;
    }

    return error;
}

}  // namespace chatclient::dto::auth
