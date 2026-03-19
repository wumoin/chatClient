#include "dto/friend_dto.h"

#include <QJsonArray>
#include <QJsonValue>

namespace chatclient::dto::friendship {
namespace {

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

QString readOptionalString(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

bool parseFriendUser(const QJsonObject &object,
                     FriendUserDto *out,
                     QString *errorMessage)
{
    FriendUserDto parsedUser;
    if (!readRequiredString(object,
                            QStringLiteral("user_id"),
                            &parsedUser.userId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("account"),
                            &parsedUser.account,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("nickname"),
                            &parsedUser.nickname,
                            errorMessage))
    {
        return false;
    }

    parsedUser.avatarUrl = readOptionalString(object, QStringLiteral("avatar_url"));
    if (out)
    {
        *out = parsedUser;
    }
    return true;
}

bool parseFriendRequestItem(const QJsonObject &object,
                            FriendRequestItemDto *out,
                            QString *errorMessage)
{
    const QJsonValue peerUserValue = object.value(QStringLiteral("peer_user"));
    if (!peerUserValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 peer_user 对象");
        }
        return false;
    }

    FriendRequestItemDto parsedItem;
    if (!readRequiredString(object,
                            QStringLiteral("request_id"),
                            &parsedItem.requestId,
                            errorMessage) ||
        !parseFriendUser(peerUserValue.toObject(),
                         &parsedItem.peerUser,
                         errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("status"),
                            &parsedItem.status,
                            errorMessage))
    {
        return false;
    }

    parsedItem.requestMessage =
        readOptionalString(object, QStringLiteral("request_message"));

    const QJsonValue createdAtValue = object.value(QStringLiteral("created_at_ms"));
    if (!createdAtValue.isDouble())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("响应字段 created_at_ms 缺失或不是数字");
        }
        return false;
    }
    parsedItem.createdAtMs = static_cast<qint64>(createdAtValue.toDouble());

    const QJsonValue handledAtValue = object.value(QStringLiteral("handled_at_ms"));
    if (handledAtValue.isDouble())
    {
        parsedItem.handledAtMs = static_cast<qint64>(handledAtValue.toDouble());
        parsedItem.hasHandledAt = true;
    }

    if (out)
    {
        *out = parsedItem;
    }
    return true;
}

}  // namespace

QJsonObject toJsonObject(const SendFriendRequestRequestDto &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("target_user_id"), request.targetUserId);

    if (!request.requestMessage.trimmed().isEmpty())
    {
        object.insert(QStringLiteral("request_message"), request.requestMessage);
    }

    return object;
}

bool parseSearchUserSuccessResponse(const QJsonObject &root,
                                    SearchUserResponseDto *out,
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

    SearchUserResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));

    const QJsonObject dataObject = dataValue.toObject();
    const QJsonValue existsValue = dataObject.value(QStringLiteral("exists"));
    if (!existsValue.isBool())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 exists 缺失或不是布尔值");
        }
        return false;
    }

    response.exists = existsValue.toBool();
    if (response.exists)
    {
        const QJsonValue userValue = dataObject.value(QStringLiteral("user"));
        if (!userValue.isObject())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("响应缺少 user 对象");
            }
            return false;
        }

        if (!parseFriendUser(userValue.toObject(), &response.user, errorMessage))
        {
            return false;
        }
    }

    if (out)
    {
        *out = response;
    }
    return true;
}

bool parseFriendRequestListSuccessResponse(const QJsonObject &root,
                                           FriendRequestListResponseDto *out,
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

    const QJsonValue requestsValue =
        dataValue.toObject().value(QStringLiteral("requests"));
    if (!requestsValue.isArray())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 requests 缺失或不是数组");
        }
        return false;
    }

    FriendRequestListResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));

    const QJsonArray requestsArray = requestsValue.toArray();
    for (const QJsonValue &itemValue : requestsArray)
    {
        if (!itemValue.isObject())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("请求列表项不是对象");
            }
            return false;
        }

        FriendRequestItemDto item;
        if (!parseFriendRequestItem(itemValue.toObject(), &item, errorMessage))
        {
            return false;
        }
        response.requests.append(item);
    }

    if (out)
    {
        *out = response;
    }
    return true;
}

bool parseSendFriendRequestSuccessResponse(
    const QJsonObject &root,
    SendFriendRequestResponseDto *out,
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

    const QJsonValue requestValue =
        dataValue.toObject().value(QStringLiteral("request"));
    if (!requestValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 request 对象");
        }
        return false;
    }

    SendFriendRequestResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));
    if (!parseFriendRequestItem(requestValue.toObject(),
                                &response.request,
                                errorMessage))
    {
        return false;
    }

    if (out)
    {
        *out = response;
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
    error.message = readOptionalString(root, QStringLiteral("message"));
    error.requestId = readOptionalString(root, QStringLiteral("request_id"));

    if (error.message.isEmpty())
    {
        error.message = fallbackMessage;
    }

    return error;
}

}  // namespace chatclient::dto::friendship
