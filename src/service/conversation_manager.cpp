#include "service/conversation_manager.h"

#include "log/app_logger.h"
#include "model/conversationlistmodel.h"
#include "model/messagemodel.h"
#include "model/messagemodelregistry.h"
#include "service/auth_service.h"
#include "ws/chat_ws_client.h"

#include <QDateTime>
#include <QImageReader>
#include <QUuid>
#include <functional>
#include <memory>

namespace chatclient::service {
namespace {

QString messageTimeText(qint64 createdAtMs)
{
    // 当前聊天气泡先统一展示“时:分”，让时间信息稳定且简洁。
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
    // 会话列表只需要一条很轻量的预览文案，不在这里做完整富文本或资源解析。
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
    // 这一层把服务端 DTO 转成消息视图真正消费的 MessageItem，
    // 这样 HTTP 历史消息和 WS 实时消息最终都能落到同一套模型结构里。
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
    // 聊天窗口的冷启动策略：
    // 1. 同一登录会话只做一次 HTTP 快照初始化
    // 2. 先拉会话列表
    // 3. 再为每个会话拉一页历史消息
    // 4. 后续切换会话不再走 HTTP，主要依赖本地 model + WS 增量更新
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
                // 没有会话时直接完成启动，不需要继续为会话逐个拉历史消息。
                m_loadedSessionKey = sessionKey;
                m_bootstrapInProgress = false;
                emit conversationBootstrapFinished();
                return;
            }

            auto pending =
                std::make_shared<int>(response.conversations.size());
            for (const auto &conversation : response.conversations) {
                // 冷启动时为每个会话预拉一页历史消息，后续切换会话就能直接绑定本地 model。
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
            // 私聊创建成功后，先立刻把摘要写进会话列表，这样 UI 不需要等下一次全量刷新。
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
    // 当前客户端策略是：第一次进入聊天页时，把已有会话统一视为“本地已读”，
    // 避免冷启动后所有历史会话都带着旧未读数压到界面上。
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
    // 历史消息快照会整体替换该会话当前的消息基线，并同步刷新本地已加载到的最大 seq。
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

bool ConversationManager::appendLocalImageMessage(const QString &conversationId,
                                                  const QString &localPath)
{
    // 这条路径只做“本地图片预览”：
    // 图片先直接插入当前会话的 MessageModel，等后面接入正式上传协议再升级为服务端消息。
    const QString trimmedConversationId = conversationId.trimmed();
    const QString trimmedLocalPath = localPath.trimmed();
    if (trimmedConversationId.isEmpty() || trimmedLocalPath.isEmpty()) {
        return false;
    }

    QImageReader reader(trimmedLocalPath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        CHATCLIENT_LOG_WARN("conversation.manager")
            << "本地图片消息追加失败，图片文件不可读取，conversation_id="
            << trimmedConversationId
            << " local_path="
            << trimmedLocalPath;
        return false;
    }

    QSize imageSize = reader.size();
    if (!imageSize.isValid()) {
        const QImage image = reader.read();
        if (image.isNull()) {
            CHATCLIENT_LOG_WARN("conversation.manager")
                << "本地图片消息追加失败，无法解析图片尺寸，conversation_id="
                << trimmedConversationId
                << " local_path="
                << trimmedLocalPath
                << " reader_error="
                << reader.errorString();
            return false;
        }
        imageSize = image.size();
    }

    const qint64 createdAtMs = QDateTime::currentMSecsSinceEpoch();
    m_messageModelRegistry->addImageMessage(trimmedConversationId,
                                            QStringLiteral("我"),
                                            messageTimeText(createdAtMs),
                                            true,
                                            trimmedLocalPath,
                                            QString(),
                                            imageSize.width(),
                                            imageSize.height(),
                                            QString());

    ConversationRuntimeState &state = ensureState(trimmedConversationId);
    state.initialized = true;

    chatclient::dto::conversation::ConversationSummaryDto conversation;
    if (m_conversationListModel->conversationById(trimmedConversationId,
                                                  &conversation))
    {
        conversation.lastMessagePreview = QStringLiteral("[图片消息]");
        conversation.hasLastMessageAtMs = true;
        conversation.lastMessageAtMs = createdAtMs;
        conversation.unreadCount = 0;
        conversation.lastReadSeq =
            qMax(conversation.lastReadSeq, conversation.lastMessageSeq);
        m_conversationListModel->upsertConversation(conversation);
        emit conversationListUpdated();
    }

    CHATCLIENT_LOG_INFO("conversation.manager")
        << "已追加本地图片消息，conversation_id="
        << trimmedConversationId
        << " local_path="
        << trimmedLocalPath
        << " width="
        << imageSize.width()
        << " height="
        << imageSize.height();
    return true;
}

bool ConversationManager::sendTextMessage(const QString &conversationId,
                                          const QString &text)
{
    // 当前文本发送策略是：先发 ws.send，不在发送瞬间直接插入一条伪正式消息；
    // 等收到 ws.ack / ws.new 后，再把正式消息写回本地 model。
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
    // 目前已读处理还是本地策略：进入会话后立即清零未读并推进 lastReadSeq，
    // 后续如果补服务端已读同步，再把这一步扩成真实回执。
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
    // ack 入口只做 route 分发，真正的业务处理放到各自单独函数里。
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
    // ws.ack 是“请求级确认”：表示刚才那次 ws.send 已被服务端处理完，
    // 但它和后续会话里的正式 message.created 仍然是两件事。
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
    // new 入口同样只做路由分发；同时先把事件向上抛，让好友页等其它界面也能复用同一条事件流。
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
    // 这里是正式消息进入本地状态的统一入口。
    // 无论是别人发来的消息，还是自己发送后广播回来的正式消息，都会走这里。
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

    // 如果这条正式消息带回了 client_message_id，就把对应的待确认发送记录清掉，
    // 避免同一条消息在 pending 表里长期残留。
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
    // 新建会话事件只需要把会话摘要注入列表即可；
    // 消息内容仍然由后续历史同步或实时 message.created 来补齐。
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
    // 当前 manager 只负责记录和向上分发好友实时事件，具体刷新哪个好友界面由外层窗口决定。
    CHATCLIENT_LOG_INFO("conversation.manager")
        << "已接入实时好友事件，route=" << route;
}

void ConversationManager::resetConversationData()
{
    // 登录态切换或登出后，要把会话列表、消息模型、运行时状态和 pending 发送全部清空，
    // 避免上一位用户的数据残留在当前窗口里。
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
    // 所有“最后一条消息摘要 + 未读数”的本地更新统一收口到这里，
    // 避免 HTTP、ack、new 各自维护一套摘要逻辑。
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
        // 当前最小未读规则：只要是别人发来的正式消息，就把未读数 +1。
        // 进入会话时再通过 markConversationReadLocally 归零。
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
