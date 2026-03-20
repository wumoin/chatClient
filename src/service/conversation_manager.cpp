#include "service/conversation_manager.h"

#include "log/app_logger.h"
#include "model/conversationlistmodel.h"
#include "model/messagemodel.h"
#include "model/messagemodelregistry.h"
#include "service/auth_service.h"
#include "ws/chat_ws_client.h"

#include <QDateTime>
#include <memory>

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

QVector<MessageItem> toMessageItems(
    const QVector<chatclient::dto::conversation::ConversationMessageDto>
        &messages,
    const QString &currentUserId)
{
    QVector<MessageItem> items;
    items.reserve(messages.size());

    for (const auto &message : messages) {
        MessageItem item;
        item.author = message.senderId == currentUserId ? QStringLiteral("我")
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
    m_conversationListModel->setConversations(response.conversations);
    for (const auto &conversation : response.conversations) {
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
    m_messageModelRegistry->replaceMessageItems(
        conversationId, toMessageItems(response.items, currentUserId));

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

void ConversationManager::handleRealtimeNewEvent(const QString &route,
                                                 const QJsonObject &data)
{
    emit realtimeNewEventReceived(route, data);

    if (route == QStringLiteral("conversation.created"))
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
        return;
    }

    if (route == QStringLiteral("friend.request.new") ||
        route == QStringLiteral("friend.request.accepted") ||
        route == QStringLiteral("friend.request.rejected"))
    {
        CHATCLIENT_LOG_INFO("conversation.manager")
            << "已接入实时好友事件，route=" << route;
        return;
    }

    CHATCLIENT_LOG_DEBUG("conversation.manager")
        << "忽略当前未接入的实时业务路由，route=" << route;
}

void ConversationManager::resetConversationData()
{
    m_conversationListModel->clear();
    m_messageModelRegistry->clearAll();
    m_runtimeStates.clear();
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

}  // namespace chatclient::service
