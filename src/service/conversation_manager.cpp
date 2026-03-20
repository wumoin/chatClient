#include "service/conversation_manager.h"

#include "log/app_logger.h"
#include "model/conversationlistmodel.h"
#include "model/messagemodel.h"
#include "model/messagemodelregistry.h"
#include "service/auth_service.h"
#include "ws/chat_ws_client.h"

#include <QDateTime>
#include <QUuid>
#include <memory>
#include <functional>

namespace chatclient::service {
namespace {

QString messageTimeText(qint64 createdAtMs)
{
    if (createdAtMs <= 0) {
        return QString();
    }

    return QDateTime::fromMSecsSinceEpoch(createdAtMs)
        .toLocalTime()
        .toString(QStringLiteral("HH:mm"));
}

QString messagePreview(const chatclient::dto::conversation::ConversationMessageDto
                           &message)
{
    if (message.messageType == QStringLiteral("text")) {
        return message.content.value(QStringLiteral("text")).toString();
    }

    if (message.messageType == QStringLiteral("image")) {
        return QStringLiteral("[图片消息]");
    }

    if (message.messageType == QStringLiteral("file")) {
        return QStringLiteral("[文件消息]");
    }

    return QStringLiteral("[未知消息]");
}

QString displayPeerName(
    const chatclient::dto::conversation::ConversationSummaryDto &conversation)
{
    if (!conversation.peerUser.nickname.trimmed().isEmpty()) {
        return conversation.peerUser.nickname.trimmed();
    }

    if (!conversation.peerUser.account.trimmed().isEmpty()) {
        return conversation.peerUser.account.trimmed();
    }

    return conversation.peerUser.userId;
}

QVector<MessageItem> toMessageItems(
    const QVector<chatclient::dto::conversation::ConversationMessageDto>
        &messages,
    const QString &currentUserId,
    const std::function<QString(const chatclient::dto::conversation::
                                    ConversationMessageDto &)> &authorResolver)
{
    QVector<MessageItem> items;
    items.reserve(messages.size());

    for (const auto &message : messages) {
        MessageItem item;
        item.conversationId = message.conversationId;
        item.messageId = message.messageId;
        item.seq = message.seq;
        if (message.hasClientMessageId) {
            item.clientMessageId = message.clientMessageId;
        }
        item.author = authorResolver ? authorResolver(message)
                                     : message.senderId;
        item.text = messagePreview(message);
        item.timeText = messageTimeText(message.createdAtMs);
        item.fromSelf = message.senderId == currentUserId;

        if (message.messageType == QStringLiteral("image")) {
            item.messageType = MessageType::Image;
            item.image.remoteUrl =
                message.content.value(QStringLiteral("url")).toString();
        } else if (message.messageType == QStringLiteral("file")) {
            item.messageType = MessageType::File;
            item.file.fileName =
                message.content.value(QStringLiteral("file_name")).toString();
            item.file.remoteUrl =
                message.content.value(QStringLiteral("url")).toString();
            item.file.sizeBytes = static_cast<qint64>(
                message.content.value(QStringLiteral("size")).toDouble(-1));
        } else {
            item.messageType = MessageType::Text;
            item.text = message.content.value(QStringLiteral("text")).toString();
        }

        items.push_back(item);
    }

    return items;
}

}  // namespace

ConversationManager::ConversationManager(QObject *parent)
    : QObject(parent),
      m_conversationApiClient(new chatclient::api::ConversationApiClient(this)),
      m_chatWsClient(new chatclient::ws::ChatWsClient(this)),
      m_conversationListModel(new chatclient::model::ConversationListModel(this)),
      m_messageModelRegistry(new MessageModelRegistry(this))
{
    qRegisterMetaType<
        chatclient::dto::conversation::CreatePrivateConversationResponseDto>(
        "chatclient::dto::conversation::CreatePrivateConversationResponseDto");
    qRegisterMetaType<
        chatclient::dto::conversation::ConversationListResponseDto>(
        "chatclient::dto::conversation::ConversationListResponseDto");
    qRegisterMetaType<
        chatclient::dto::conversation::ConversationDetailResponseDto>(
        "chatclient::dto::conversation::ConversationDetailResponseDto");
    qRegisterMetaType<
        chatclient::dto::conversation::ConversationMessageListResponseDto>(
        "chatclient::dto::conversation::ConversationMessageListResponseDto");
    qRegisterMetaType<
        chatclient::dto::conversation::SendTextMessageResponseDto>(
        "chatclient::dto::conversation::SendTextMessageResponseDto");

    connect(m_chatWsClient,
            &chatclient::ws::ChatWsClient::statusChanged,
            this,
            &ConversationManager::realtimeStatusChanged);
    connect(m_chatWsClient,
            &chatclient::ws::ChatWsClient::authenticated,
            this,
            &ConversationManager::realtimeAuthenticated);
    connect(m_chatWsClient,
            &chatclient::ws::ChatWsClient::authenticationFailed,
            this,
            &ConversationManager::realtimeAuthenticationFailed);
    connect(m_chatWsClient,
            &chatclient::ws::ChatWsClient::newEventReceived,
            this,
            &ConversationManager::handleRealtimeNewEvent);
    connect(m_chatWsClient,
            &chatclient::ws::ChatWsClient::ackReceived,
            this,
            &ConversationManager::handleRealtimeAckEvent);
}

void ConversationManager::setAuthService(
    chatclient::service::AuthService *authService)
{
    if (m_logoutConnection) {
        disconnect(m_logoutConnection);
        m_logoutConnection = QMetaObject::Connection();
    }

    m_authService = authService;

    if (!m_authService) {
        resetConversationData();
        disconnectRealtimeChannel();
        return;
    }

    m_logoutConnection = connect(
        m_authService,
        &chatclient::service::AuthService::logoutSucceeded,
        this,
        [this]() {
            disconnectRealtimeChannel();
            resetConversationData();
        });
}

chatclient::model::ConversationListModel *
ConversationManager::conversationListModel() const
{
    return m_conversationListModel;
}

MessageModelRegistry *ConversationManager::messageModelRegistry() const
{
    return m_messageModelRegistry;
}

MessageModel *ConversationManager::ensureMessageModel(
    const QString &conversationId) const
{
    return m_messageModelRegistry->ensureModel(conversationId);
}

void ConversationManager::connectRealtimeChannel()
{
    if (!m_chatWsClient) {
        return;
    }

    if (!m_authService || !m_authService->hasActiveSession()) {
        m_chatWsClient->disconnectFromServer();
        return;
    }

    const auto &session = m_authService->currentSession();
    m_chatWsClient->setSession(session.accessToken,
                               m_authService->currentDeviceId(),
                               session.deviceSessionId);
    m_chatWsClient->connectToServer();
}

void ConversationManager::disconnectRealtimeChannel()
{
    if (m_chatWsClient) {
        m_chatWsClient->disconnectFromServer();
    }
}

void ConversationManager::initializeConversationDataIfNeeded()
{
    if (!m_authService || !m_authService->hasActiveSession()) {
        return;
    }

    const QString sessionKey = currentSessionKey();
    if (sessionKey.isEmpty()) {
        return;
    }

    if (m_bootstrapInProgress && m_bootstrapSessionKey == sessionKey) {
        return;
    }

    if (!m_loadedSessionKey.isEmpty() && m_loadedSessionKey == sessionKey) {
        return;
    }

    resetConversationData();
    m_bootstrapInProgress = true;
    m_bootstrapSessionKey = sessionKey;
    emit conversationBootstrapStarted();

    const QString accessToken =
        m_authService->currentSession().accessToken.trimmed();
    m_conversationApiClient->fetchConversations(
        accessToken,
        [this, sessionKey](
            const chatclient::dto::conversation::ConversationListResponseDto
                &response) {
            if (sessionKey != m_bootstrapSessionKey) {
                return;
            }

            applyConversationListSnapshot(response);
            emit conversationListUpdated();

            if (response.conversations.isEmpty()) {
                m_loadedSessionKey = sessionKey;
                m_bootstrapInProgress = false;
                emit conversationBootstrapFinished();
                return;
            }

            auto pending =
                std::make_shared<int>(response.conversations.size());
            for (const auto &conversation : response.conversations) {
                ConversationRuntimeState &state =
                    ensureState(conversation.conversationId);
                state.loading = true;

                chatclient::dto::conversation::
                    ListConversationMessagesRequestDto request;
                request.limit = 50;

                m_conversationApiClient->fetchConversationMessages(
                    m_authService->currentSession().accessToken.trimmed(),
                    conversation.conversationId,
                    request,
                    [this, sessionKey, pending, conversationId = conversation.conversationId](
                        const chatclient::dto::conversation::
                            ConversationMessageListResponseDto &messagesResponse) {
                        if (sessionKey != m_bootstrapSessionKey) {
                            return;
                        }

                        applyConversationMessagesSnapshot(conversationId,
                                                          messagesResponse);

                        *pending -= 1;
                        if (*pending == 0) {
                            m_loadedSessionKey = sessionKey;
                            m_bootstrapInProgress = false;
                            emit conversationBootstrapFinished();
                        }
                    },
                    [this, sessionKey, pending, conversationId = conversation.conversationId](
                        const chatclient::dto::conversation::ApiErrorDto &error) {
                        if (sessionKey != m_bootstrapSessionKey) {
                            return;
                        }

                        CHATCLIENT_LOG_WARN("conversation.manager")
                            << "初始化历史消息失败，conversation_id="
                            << conversationId
                            << " request_id="
                            << error.requestId
                            << " http_status="
                            << error.httpStatus
                            << " error_code="
                            << error.errorCode
                            << " message="
                            << error.message;

                        ConversationRuntimeState &state =
                            ensureState(conversationId);
                        state.loading = false;

                        *pending -= 1;
                        if (*pending == 0) {
                            m_loadedSessionKey = sessionKey;
                            m_bootstrapInProgress = false;
                            emit conversationBootstrapFinished();
                        }
                    });
            }
        },
        [this, sessionKey](const chatclient::dto::conversation::ApiErrorDto
                               &error) {
            if (sessionKey != m_bootstrapSessionKey) {
                return;
            }

            m_bootstrapInProgress = false;
            CHATCLIENT_LOG_WARN("conversation.manager")
                << "初始化会话列表失败，request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            emit conversationBootstrapFailed(localizeConversationApiError(error));
        });
}

void ConversationManager::createPrivateConversation(
    const QString &peerUserId,
    CreatePrivateConversationSuccessHandler onSuccess,
    CreatePrivateConversationFailureHandler onFailure)
{
    if (!m_authService || !m_authService->hasActiveSession()) {
        if (onFailure) {
            chatclient::dto::conversation::ApiErrorDto error;
            error.errorCode = 40102;
            error.message = QStringLiteral("invalid access token");
            onFailure(error);
        }
        return;
    }

    const QString trimmedPeerUserId = peerUserId.trimmed();
    if (trimmedPeerUserId.isEmpty()) {
        if (onFailure) {
            chatclient::dto::conversation::ApiErrorDto error;
            error.httpStatus = 400;
            error.errorCode = 40001;
            error.message = QStringLiteral("peer_user_id is required");
            onFailure(error);
        }
        return;
    }

    chatclient::dto::conversation::CreatePrivateConversationRequestDto request;
    request.peerUserId = trimmedPeerUserId;

    const QString accessToken =
        m_authService->currentSession().accessToken.trimmed();
    m_conversationApiClient->createPrivateConversation(
        accessToken,
        request,
        [this, onSuccess = std::move(onSuccess)](
            const chatclient::dto::conversation::
                CreatePrivateConversationResponseDto &response) mutable {
            m_conversationListModel->upsertConversation(response.conversation);
            ensureState(response.conversation.conversationId);
            if (onSuccess) {
                onSuccess(response);
            }
        },
        std::move(onFailure));
}

void ConversationManager::applyConversationListSnapshot(
    const chatclient::dto::conversation::ConversationListResponseDto &response)
{
    QVector<chatclient::dto::conversation::ConversationSummaryDto> conversations =
        response.conversations;
    for (auto &conversation : conversations)
    {
        conversation.lastReadSeq =
            qMax(conversation.lastReadSeq, conversation.lastMessageSeq);
        conversation.unreadCount = 0;
    }

    m_conversationListModel->setConversations(conversations);
    for (const auto &conversation : conversations) {
        ensureState(conversation.conversationId);
    }
}

void ConversationManager::applyConversationMessagesSnapshot(
    const QString &conversationId,
    const chatclient::dto::conversation::ConversationMessageListResponseDto
        &response)
{
    const QString currentUserId =
        m_authService ? m_authService->currentSession().user.userId : QString();
    QString peerDisplay;
    chatclient::dto::conversation::ConversationSummaryDto conversation;
    if (m_conversationListModel->conversationById(conversationId, &conversation))
    {
        peerDisplay = displayPeerName(conversation);
    }

    m_messageModelRegistry->replaceMessageItems(
        conversationId,
        toMessageItems(
            response.items,
            currentUserId,
            [currentUserId, peerDisplay](
                const chatclient::dto::conversation::ConversationMessageDto
                    &message) {
                if (message.senderId == currentUserId) {
                    return QStringLiteral("我");
                }

                if (!peerDisplay.trimmed().isEmpty()) {
                    return peerDisplay;
                }

                return message.senderId;
            }));

    ConversationRuntimeState &state = ensureState(conversationId);
    state.initialized = true;
    state.loading = false;
    state.hasMoreBefore = response.hasMore;
    state.lastLoadedMaxSeq =
        response.items.isEmpty() ? 0 : response.items.constLast().seq;
}

void ConversationManager::appendLocalTextMessage(const QString &conversationId,
                                                 const QString &author,
                                                 const QString &text,
                                                 const QString &timeText,
                                                 bool fromSelf)
{
    m_messageModelRegistry->addTextMessage(conversationId,
                                           author,
                                           text,
                                           timeText,
                                           fromSelf);

    ConversationRuntimeState &state = ensureState(conversationId);
    state.initialized = true;
    if (state.lastLoadedMaxSeq <= 0) {
        state.lastLoadedMaxSeq = 1;
    }
}

bool ConversationManager::sendTextMessage(const QString &conversationId,
                                          const QString &text)
{
    if (!m_chatWsClient || !m_chatWsClient->isAuthenticated()) {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "拒绝发送文本消息，实时通道当前不可用，conversation_id="
            << conversationId;
        return false;
    }

    const QString trimmedConversationId = conversationId.trimmed();
    const QString trimmedText = text.trimmed();
    if (trimmedConversationId.isEmpty() || trimmedText.isEmpty()) {
        return false;
    }

    const QString clientMessageId =
        QUuid::createUuid().toString(QUuid::WithoutBraces);
    QJsonObject data;
    data.insert(QStringLiteral("conversation_id"), trimmedConversationId);
    data.insert(QStringLiteral("client_message_id"), clientMessageId);
    data.insert(QStringLiteral("text"), trimmedText);

    const QString requestId = m_chatWsClient->sendBusinessEvent(
        QStringLiteral("message.send_text"), data);
    if (requestId.isEmpty()) {
        return false;
    }

    PendingTextMessage pending;
    pending.conversationId = trimmedConversationId;
    pending.clientMessageId = clientMessageId;
    pending.text = trimmedText;
    pending.sentAtMs = QDateTime::currentMSecsSinceEpoch();
    m_pendingTextMessagesByRequestId.insert(requestId, pending);
    return true;
}

bool ConversationManager::markConversationReadLocally(
    const QString &conversationId)
{
    const QString trimmedConversationId = conversationId.trimmed();
    if (trimmedConversationId.isEmpty()) {
        return false;
    }

    chatclient::dto::conversation::ConversationSummaryDto conversation;
    if (!m_conversationListModel->conversationById(trimmedConversationId,
                                                   &conversation))
    {
        return false;
    }

    const qint64 nextLastReadSeq =
        qMax(conversation.lastReadSeq, conversation.lastMessageSeq);
    const bool changed =
        conversation.unreadCount > 0 || nextLastReadSeq != conversation.lastReadSeq;
    if (!changed) {
        return false;
    }

    conversation.unreadCount = 0;
    conversation.lastReadSeq = nextLastReadSeq;
    m_conversationListModel->upsertConversation(conversation);
    emit conversationListUpdated();
    return true;
}

ConversationManager::ConversationRuntimeState
ConversationManager::conversationState(const QString &conversationId) const
{
    return m_runtimeStates.value(conversationId);
}

ConversationManager::ConversationRuntimeState &
ConversationManager::ensureState(const QString &conversationId)
{
    return m_runtimeStates[conversationId];
}

void ConversationManager::handleRealtimeAckEvent(const QString &route,
                                                 const bool ok,
                                                 const int code,
                                                 const QString &message,
                                                 const QJsonObject &data,
                                                 const QString &requestId)
{
    if (route == QStringLiteral("message.send_text"))
    {
        handleMessageSendTextAck(ok, code, message, data, requestId);
        return;
    }

    CHATCLIENT_LOG_DEBUG("conversation.manager")
        << "忽略当前未接入的实时确认业务路由，route=" << route;
}

void ConversationManager::handleMessageSendTextAck(
    const bool ok,
    const int code,
    const QString &message,
    const QJsonObject &data,
    const QString &requestId)
{
    const PendingTextMessage pending =
        m_pendingTextMessagesByRequestId.take(requestId);
    if (!ok)
    {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "文本消息发送确认失败，request_id="
            << requestId
            << " code="
            << code
            << " message="
            << message;
        return;
    }

    const QString conversationId =
        data.value(QStringLiteral("conversation_id")).toString().trimmed();
    const QString messageId =
        data.value(QStringLiteral("message_id")).toString().trimmed();
    const qint64 seq =
        data.value(QStringLiteral("seq")).toVariant().toLongLong();
    const qint64 createdAtMs =
        data.value(QStringLiteral("created_at_ms")).toVariant().toLongLong();
    QString clientMessageId =
        data.value(QStringLiteral("client_message_id")).toString().trimmed();
    if (clientMessageId.isEmpty()) {
        clientMessageId = pending.clientMessageId;
    }

    if (conversationId.isEmpty() || messageId.isEmpty() || seq <= 0)
    {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "文本消息发送确认缺少关键字段，request_id=" << requestId;
        return;
    }

    MessageItem item;
    item.conversationId = conversationId;
    item.messageId = messageId;
    item.clientMessageId = clientMessageId;
    item.seq = seq;
    item.author = QStringLiteral("我");
    item.text = pending.text;
    item.timeText = messageTimeText(createdAtMs > 0 ? createdAtMs
                                                    : pending.sentAtMs);
    item.fromSelf = true;
    item.messageType = MessageType::Text;
    m_messageModelRegistry->upsertMessageItem(conversationId, item);

    ConversationRuntimeState &state = ensureState(conversationId);
    state.initialized = true;
    state.lastLoadedMaxSeq = qMax(state.lastLoadedMaxSeq, seq);

    chatclient::dto::conversation::ConversationMessageDto messageDto;
    messageDto.messageId = messageId;
    messageDto.conversationId = conversationId;
    messageDto.seq = seq;
    messageDto.senderId = currentUserId();
    messageDto.hasClientMessageId = !clientMessageId.isEmpty();
    messageDto.clientMessageId = clientMessageId;
    messageDto.messageType = QStringLiteral("text");
    messageDto.content.insert(QStringLiteral("text"), pending.text);
    messageDto.createdAtMs = createdAtMs > 0 ? createdAtMs : pending.sentAtMs;
    updateConversationSummaryFromMessage(messageDto, pending.text);

    CHATCLIENT_LOG_INFO("conversation.manager")
        << "文本消息发送确认已接入，conversation_id="
        << conversationId
        << " message_id="
        << messageId
        << " seq="
        << seq;
}

void ConversationManager::handleRealtimeNewEvent(const QString &route,
                                                 const QJsonObject &data)
{
    emit realtimeNewEventReceived(route, data);

    if (route == QStringLiteral("message.created"))
    {
        handleMessageCreatedEvent(data);
        return;
    }

    if (route == QStringLiteral("conversation.created"))
    {
        handleConversationCreatedEvent(data);
        return;
    }

    if (route == QStringLiteral("friend.request.new") ||
        route == QStringLiteral("friend.request.accepted") ||
        route == QStringLiteral("friend.request.rejected"))
    {
        handleFriendRealtimeEvent(route);
        return;
    }

    CHATCLIENT_LOG_DEBUG("conversation.manager")
        << "忽略当前未接入的实时业务路由，route=" << route;
}

void ConversationManager::handleMessageCreatedEvent(const QJsonObject &data)
{
    chatclient::dto::conversation::ConversationMessageDto message;
    QString errorMessage;
    if (!chatclient::dto::conversation::parseConversationMessageObject(
            data, &message, &errorMessage))
    {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "解析实时消息事件失败，message=" << errorMessage;
        return;
    }

    const QString userId = currentUserId();
    QString peerDisplay;
    chatclient::dto::conversation::ConversationSummaryDto conversation;
    if (m_conversationListModel->conversationById(message.conversationId,
                                                  &conversation))
    {
        peerDisplay = displayPeerName(conversation);
    }

    const QVector<MessageItem> items = toMessageItems(
        {message},
        userId,
        [userId, peerDisplay](
            const chatclient::dto::conversation::ConversationMessageDto
                &currentMessage) {
            if (currentMessage.senderId == userId) {
                return QStringLiteral("我");
            }

            if (!peerDisplay.trimmed().isEmpty()) {
                return peerDisplay;
            }

            return currentMessage.senderId;
        });
    if (!items.isEmpty())
    {
        m_messageModelRegistry->upsertMessageItem(message.conversationId,
                                                  items.constFirst());
    }

    ConversationRuntimeState &state = ensureState(message.conversationId);
    state.initialized = true;
    state.lastLoadedMaxSeq = qMax(state.lastLoadedMaxSeq, message.seq);
    updateConversationSummaryFromMessage(message, messagePreview(message));

    for (auto it = m_pendingTextMessagesByRequestId.begin();
         it != m_pendingTextMessagesByRequestId.end();)
    {
        if (!message.clientMessageId.isEmpty() &&
            it->clientMessageId == message.clientMessageId)
        {
            it = m_pendingTextMessagesByRequestId.erase(it);
            continue;
        }
        ++it;
    }

    CHATCLIENT_LOG_INFO("conversation.manager")
        << "已接入实时消息事件，conversation_id="
        << message.conversationId
        << " message_id="
        << message.messageId
        << " seq="
        << message.seq;
}

void ConversationManager::handleConversationCreatedEvent(
    const QJsonObject &data)
{
    const QJsonValue conversationValue =
        data.value(QStringLiteral("conversation"));
    if (!conversationValue.isObject())
    {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "实时会话创建事件缺少 conversation 对象";
        return;
    }

    chatclient::dto::conversation::ConversationSummaryDto conversation;
    QString errorMessage;
    if (!chatclient::dto::conversation::parseConversationSummary(
            conversationValue.toObject(), &conversation, &errorMessage))
    {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "解析实时会话创建事件失败，message="
            << errorMessage;
        return;
    }

    m_conversationListModel->upsertConversation(conversation);
    ensureState(conversation.conversationId);
    emit conversationListUpdated();

    CHATCLIENT_LOG_INFO("conversation.manager")
        << "已接入实时会话创建事件，conversation_id="
        << conversation.conversationId
        << " peer_user_id="
        << conversation.peerUser.userId;
}

void ConversationManager::handleFriendRealtimeEvent(const QString &route)
{
    CHATCLIENT_LOG_INFO("conversation.manager")
        << "已接入实时好友事件，route=" << route;
}

void ConversationManager::resetConversationData()
{
    m_conversationListModel->clear();
    m_messageModelRegistry->clearAll();
    m_runtimeStates.clear();
    m_pendingTextMessagesByRequestId.clear();
    m_loadedSessionKey.clear();
    m_bootstrapSessionKey.clear();
    m_bootstrapInProgress = false;
    emit conversationListUpdated();
}

QString ConversationManager::currentSessionKey() const
{
    if (!m_authService || !m_authService->hasActiveSession()) {
        return QString();
    }

    return m_authService->currentSession().deviceSessionId.trimmed();
}

QString ConversationManager::localizeConversationApiError(
    const chatclient::dto::conversation::ApiErrorDto &error)
{
    switch (error.errorCode) {
    case 40102:
        return QStringLiteral("当前登录状态已失效，请重新登录");
    case 40400:
        return QStringLiteral("目标会话不存在");
    default:
        break;
    }

    return QStringLiteral("同步会话数据失败，请稍后重试");
}

void ConversationManager::updateConversationSummaryFromMessage(
    const chatclient::dto::conversation::ConversationMessageDto &message,
    const QString &previewText)
{
    chatclient::dto::conversation::ConversationSummaryDto conversation;
    if (!m_conversationListModel->conversationById(message.conversationId,
                                                   &conversation))
    {
        CHATCLIENT_LOG_DEBUG("conversation.manager")
            << "当前消息对应的会话摘要尚未加载，conversation_id="
            << message.conversationId;
        return;
    }

    conversation.lastMessageSeq = qMax(conversation.lastMessageSeq, message.seq);
    conversation.lastMessagePreview = previewText;
    conversation.hasLastMessageAtMs = true;
    conversation.lastMessageAtMs = message.createdAtMs;
    if (message.senderId == currentUserId())
    {
        conversation.lastReadSeq = qMax(conversation.lastReadSeq, message.seq);
    }
    else
    {
        conversation.unreadCount = qMax<qint64>(0, conversation.unreadCount) + 1;
    }

    m_conversationListModel->upsertConversation(conversation);
    emit conversationListUpdated();
}

QString ConversationManager::currentUserId() const
{
    if (!m_authService || !m_authService->hasActiveSession()) {
        return QString();
    }

    return m_authService->currentSession().user.userId;
}

}  // namespace chatclient::service
