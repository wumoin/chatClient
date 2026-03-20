#include "dto/conversation_dto.h"

#include <QJsonArray>
#include <QJsonValue>

namespace chatclient::dto::conversation {
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

bool readRequiredInt64(const QJsonObject &object,
                       const QString &key,
                       qint64 *out,
                       QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是整数")
                                .arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = static_cast<qint64>(value.toDouble());
    }
    return true;
}

QString readOptionalString(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

bool readOptionalInt64(const QJsonObject &object,
                       const QString &key,
                       qint64 *out)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    if (out)
    {
        *out = static_cast<qint64>(value.toDouble());
    }
    return true;
}

bool readRequiredObject(const QJsonObject &object,
                        const QString &key,
                        QJsonObject *out,
                        QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是对象")
                                .arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = value.toObject();
    }
    return true;
}

bool readRequiredArray(const QJsonObject &object,
                       const QString &key,
                       QJsonArray *out,
                       QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isArray())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是数组")
                                .arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = value.toArray();
    }
    return true;
}

bool parseConversationPeerUser(const QJsonObject &object,
                               ConversationPeerUserDto *out,
                               QString *errorMessage)
{
    ConversationPeerUserDto user;
    if (!readRequiredString(object,
                            QStringLiteral("user_id"),
                            &user.userId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("account"),
                            &user.account,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("nickname"),
                            &user.nickname,
                            errorMessage))
    {
        return false;
    }

    user.avatarUrl = readOptionalString(object, QStringLiteral("avatar_url"));
    if (out)
    {
        *out = user;
    }
    return true;
}

bool parseConversationMember(const QJsonObject &object,
                             ConversationMemberDto *out,
                             QString *errorMessage)
{
    ConversationMemberDto member;
    if (!readRequiredString(object,
                            QStringLiteral("user_id"),
                            &member.userId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("member_role"),
                            &member.memberRole,
                            errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("joined_at_ms"),
                           &member.joinedAtMs,
                           errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("last_read_seq"),
                           &member.lastReadSeq,
                           errorMessage))
    {
        return false;
    }

    member.hasLastReadAtMs = readOptionalInt64(
        object, QStringLiteral("last_read_at_ms"), &member.lastReadAtMs);

    if (out)
    {
        *out = member;
    }
    return true;
}

bool parseConversationSummaryObject(const QJsonObject &object,
                                    ConversationSummaryDto *out,
                                    QString *errorMessage)
{
    ConversationSummaryDto conversation;
    if (!readRequiredString(object,
                            QStringLiteral("conversation_id"),
                            &conversation.conversationId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("conversation_type"),
                            &conversation.conversationType,
                            errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("last_message_seq"),
                           &conversation.lastMessageSeq,
                           errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("last_read_seq"),
                           &conversation.lastReadSeq,
                           errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("unread_count"),
                           &conversation.unreadCount,
                           errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("created_at_ms"),
                           &conversation.createdAtMs,
                           errorMessage))
    {
        return false;
    }

    QJsonObject peerUserObject;
    if (!readRequiredObject(object,
                            QStringLiteral("peer_user"),
                            &peerUserObject,
                            errorMessage) ||
        !parseConversationPeerUser(peerUserObject,
                                   &conversation.peerUser,
                                   errorMessage))
    {
        return false;
    }

    conversation.lastMessagePreview =
        readOptionalString(object, QStringLiteral("last_message_preview"));
    conversation.hasLastMessageAtMs = readOptionalInt64(
        object, QStringLiteral("last_message_at_ms"), &conversation.lastMessageAtMs);

    if (out)
    {
        *out = conversation;
    }
    return true;
}

bool parseConversationDetail(const QJsonObject &object,
                             ConversationDetailDto *out,
                             QString *errorMessage)
{
    ConversationDetailDto detail;
    if (!readRequiredString(object,
                            QStringLiteral("conversation_id"),
                            &detail.conversationId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("conversation_type"),
                            &detail.conversationType,
                            errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("last_message_seq"),
                           &detail.lastMessageSeq,
                           errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("unread_count"),
                           &detail.unreadCount,
                           errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("created_at_ms"),
                           &detail.createdAtMs,
                           errorMessage))
    {
        return false;
    }

    QJsonObject peerUserObject;
    QJsonObject myMemberObject;
    if (!readRequiredObject(object,
                            QStringLiteral("peer_user"),
                            &peerUserObject,
                            errorMessage) ||
        !parseConversationPeerUser(peerUserObject,
                                   &detail.peerUser,
                                   errorMessage) ||
        !readRequiredObject(object,
                            QStringLiteral("my_member"),
                            &myMemberObject,
                            errorMessage) ||
        !parseConversationMember(myMemberObject,
                                 &detail.myMember,
                                 errorMessage))
    {
        return false;
    }

    detail.lastMessagePreview =
        readOptionalString(object, QStringLiteral("last_message_preview"));
    detail.hasLastMessageAtMs = readOptionalInt64(
        object, QStringLiteral("last_message_at_ms"), &detail.lastMessageAtMs);

    if (out)
    {
        *out = detail;
    }
    return true;
}

bool parseConversationMessage(const QJsonObject &object,
                              ConversationMessageDto *out,
                              QString *errorMessage)
{
    ConversationMessageDto message;
    if (!readRequiredString(object,
                            QStringLiteral("message_id"),
                            &message.messageId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("conversation_id"),
                            &message.conversationId,
                            errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("seq"),
                           &message.seq,
                           errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("sender_id"),
                            &message.senderId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("type"),
                            &message.messageType,
                            errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("created_at_ms"),
                           &message.createdAtMs,
                           errorMessage))
    {
        return false;
    }

    const QJsonValue clientMessageIdValue =
        object.value(QStringLiteral("client_message_id"));
    if (clientMessageIdValue.isString())
    {
        message.hasClientMessageId = true;
        message.clientMessageId = clientMessageIdValue.toString();
    }

    QJsonObject contentObject;
    if (!readRequiredObject(object,
                            QStringLiteral("content"),
                            &contentObject,
                            errorMessage))
    {
        return false;
    }
    message.content = contentObject;

    if (out)
    {
        *out = message;
    }
    return true;
}

bool ensureSuccessCode(const QJsonObject &root, QString *errorMessage)
{
    if (root.value(QStringLiteral("code")).toInt(-1) == 0)
    {
        return true;
    }

    if (errorMessage)
    {
        *errorMessage = QStringLiteral("服务端返回了非成功业务码");
    }
    return false;
}

}  // namespace

QJsonObject toJsonObject(const CreatePrivateConversationRequestDto &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("peer_user_id"), request.peerUserId);
    return object;
}

QJsonObject toJsonObject(const SendTextMessageRequestDto &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("text"), request.text);
    if (!request.clientMessageId.trimmed().isEmpty())
    {
        object.insert(QStringLiteral("client_message_id"),
                      request.clientMessageId.trimmed());
    }
    return object;
}

bool parseCreatePrivateConversationSuccessResponse(
    const QJsonObject &root,
    CreatePrivateConversationResponseDto *out,
    QString *errorMessage)
{
    if (!ensureSuccessCode(root, errorMessage))
    {
        return false;
    }

    QJsonObject dataObject;
    QJsonObject conversationObject;
    if (!readRequiredObject(root, QStringLiteral("data"), &dataObject, errorMessage) ||
        !readRequiredObject(dataObject,
                            QStringLiteral("conversation"),
                            &conversationObject,
                            errorMessage))
    {
        return false;
    }

    CreatePrivateConversationResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));
    if (!parseConversationSummaryObject(conversationObject,
                                        &response.conversation,
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

bool parseConversationListSuccessResponse(const QJsonObject &root,
                                          ConversationListResponseDto *out,
                                          QString *errorMessage)
{
    if (!ensureSuccessCode(root, errorMessage))
    {
        return false;
    }

    QJsonObject dataObject;
    QJsonArray items;
    if (!readRequiredObject(root, QStringLiteral("data"), &dataObject, errorMessage) ||
        !readRequiredArray(dataObject,
                           QStringLiteral("conversations"),
                           &items,
                           errorMessage))
    {
        return false;
    }

    ConversationListResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));

    for (const QJsonValue &itemValue : items)
    {
        if (!itemValue.isObject())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("会话列表项不是对象");
            }
            return false;
        }

        ConversationSummaryDto item;
        if (!parseConversationSummaryObject(itemValue.toObject(),
                                            &item,
                                            errorMessage))
        {
            return false;
        }
        response.conversations.push_back(item);
    }

    if (out)
    {
        *out = response;
    }
    return true;
}

bool parseConversationDetailSuccessResponse(const QJsonObject &root,
                                            ConversationDetailResponseDto *out,
                                            QString *errorMessage)
{
    if (!ensureSuccessCode(root, errorMessage))
    {
        return false;
    }

    QJsonObject dataObject;
    QJsonObject conversationObject;
    if (!readRequiredObject(root, QStringLiteral("data"), &dataObject, errorMessage) ||
        !readRequiredObject(dataObject,
                            QStringLiteral("conversation"),
                            &conversationObject,
                            errorMessage))
    {
        return false;
    }

    ConversationDetailResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));
    if (!parseConversationDetail(conversationObject,
                                 &response.conversation,
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

bool parseConversationMessagesSuccessResponse(
    const QJsonObject &root,
    ConversationMessageListResponseDto *out,
    QString *errorMessage)
{
    if (!ensureSuccessCode(root, errorMessage))
    {
        return false;
    }

    QJsonObject dataObject;
    QJsonArray items;
    if (!readRequiredObject(root, QStringLiteral("data"), &dataObject, errorMessage) ||
        !readRequiredArray(dataObject,
                           QStringLiteral("items"),
                           &items,
                           errorMessage))
    {
        return false;
    }

    ConversationMessageListResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));

    const QJsonValue hasMoreValue = dataObject.value(QStringLiteral("has_more"));
    if (!hasMoreValue.isBool())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 has_more 缺失或不是布尔值");
        }
        return false;
    }
    response.hasMore = hasMoreValue.toBool();

    response.hasNextBeforeSeq = readOptionalInt64(
        dataObject, QStringLiteral("next_before_seq"), &response.nextBeforeSeq);
    response.hasNextAfterSeq = readOptionalInt64(
        dataObject, QStringLiteral("next_after_seq"), &response.nextAfterSeq);

    for (const QJsonValue &itemValue : items)
    {
        if (!itemValue.isObject())
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("消息列表项不是对象");
            }
            return false;
        }

        ConversationMessageDto item;
        if (!parseConversationMessage(itemValue.toObject(), &item, errorMessage))
        {
            return false;
        }
        response.items.push_back(item);
    }

    if (out)
    {
        *out = response;
    }
    return true;
}

bool parseSendTextMessageSuccessResponse(const QJsonObject &root,
                                         SendTextMessageResponseDto *out,
                                         QString *errorMessage)
{
    if (!ensureSuccessCode(root, errorMessage))
    {
        return false;
    }

    QJsonObject dataObject;
    QJsonObject messageObject;
    if (!readRequiredObject(root, QStringLiteral("data"), &dataObject, errorMessage) ||
        !readRequiredObject(dataObject,
                            QStringLiteral("message"),
                            &messageObject,
                            errorMessage))
    {
        return false;
    }

    SendTextMessageResponseDto response;
    response.requestId = readOptionalString(root, QStringLiteral("request_id"));
    if (!parseConversationMessage(messageObject, &response.message, errorMessage))
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

bool parseConversationSummary(const QJsonObject &object,
                              ConversationSummaryDto *out,
                              QString *errorMessage)
{
    return parseConversationSummaryObject(object, out, errorMessage);
}

}  // namespace chatclient::dto::conversation
