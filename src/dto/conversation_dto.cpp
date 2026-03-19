#include "dto/conversation_dto.h"

#include <QJsonValue>

namespace chatclient::dto::conversation {
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
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是字符串")
                                .arg(key);
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

QJsonObject toJsonObject(const CreatePrivateConversationRequestDto &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("peer_user_id"), request.peerUserId);
    return object;
}

bool parseCreatePrivateConversationSuccessResponse(
    const QJsonObject &root,
    CreatePrivateConversationResponseDto *out,
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
    const QJsonValue conversationValue =
        dataObject.value(QStringLiteral("conversation"));
    if (!conversationValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 conversation 对象");
        }
        return false;
    }

    CreatePrivateConversationResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));

    const QJsonObject conversationObject = conversationValue.toObject();
    if (!readRequiredString(conversationObject,
                            QStringLiteral("conversation_id"),
                            &response.conversation.conversationId,
                            errorMessage) ||
        !readRequiredString(conversationObject,
                            QStringLiteral("conversation_type"),
                            &response.conversation.conversationType,
                            errorMessage))
    {
        return false;
    }

    const QJsonValue peerUserValue =
        conversationObject.value(QStringLiteral("peer_user"));
    if (!peerUserValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 peer_user 对象");
        }
        return false;
    }

    const QJsonObject peerUserObject = peerUserValue.toObject();
    if (!readRequiredString(peerUserObject,
                            QStringLiteral("user_id"),
                            &response.conversation.peerUser.userId,
                            errorMessage) ||
        !readRequiredString(peerUserObject,
                            QStringLiteral("account"),
                            &response.conversation.peerUser.account,
                            errorMessage) ||
        !readRequiredString(peerUserObject,
                            QStringLiteral("nickname"),
                            &response.conversation.peerUser.nickname,
                            errorMessage))
    {
        return false;
    }

    response.conversation.peerUser.avatarUrl =
        readOptionalString(peerUserObject, QStringLiteral("avatar_url"));

    if (out)
    {
        *out = response;
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

}  // namespace chatclient::dto::conversation
