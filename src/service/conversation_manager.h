#pragma once

#include "api/conversation_api_client.h"
#include "dto/conversation_dto.h"

#include <QHash>
#include <QObject>
#include <QString>

#include <functional>

class MessageModel;
class MessageModelRegistry;

namespace chatclient::model {
class ConversationListModel;
}

namespace chatclient::service {
class AuthService;
}

namespace chatclient::ws {
class ChatWsClient;
}

namespace chatclient::service {

/**
 * @brief 客户端会话域统一调度入口。
 *
 * 当前第一版职责：
 * 1) 持有 ConversationApiClient / ChatWsClient；
 * 2) 持有 ConversationListModel / MessageModelRegistry；
 * 3) 把 HTTP 返回和 WS 推送统一收口后，再更新两个 model；
 * 4) 在内部维护每个会话的轻量运行时状态。
 */
class ConversationManager : public QObject
{
    Q_OBJECT

  public:
    using CreatePrivateConversationSuccessHandler = std::function<void(
        const chatclient::dto::conversation::CreatePrivateConversationResponseDto &)>;
    using CreatePrivateConversationFailureHandler =
        std::function<void(const chatclient::dto::conversation::ApiErrorDto &)>;

    struct ConversationRuntimeState
    {
        qint64 lastLoadedMaxSeq = 0;
        bool hasMoreBefore = false;
        bool initialized = false;
        bool loading = false;
    };

    /**
     * @brief 构造客户端会话调度器。
     * @param parent 父级 QObject，可为空。
     */
    explicit ConversationManager(QObject *parent = nullptr);

    /**
     * @brief 注入当前认证服务。
     * @param authService 当前认证服务，可为空。
     */
    void setAuthService(chatclient::service::AuthService *authService);

    /**
     * @brief 返回会话列表展示模型。
     * @return 会话列表模型指针。
     */
    chatclient::model::ConversationListModel *conversationListModel() const;

    /**
     * @brief 返回多会话消息模型注册表。
     * @return 消息模型注册表指针。
     */
    MessageModelRegistry *messageModelRegistry() const;

    /**
     * @brief 确保指定会话的消息模型存在。
     * @param conversationId 会话唯一标识。
     * @return 对应的消息模型；会话 id 为空时返回 nullptr。
     */
    MessageModel *ensureMessageModel(const QString &conversationId) const;

    /**
     * @brief 根据当前登录态连接 WebSocket 实时通道。
     */
    void connectRealtimeChannel();

    /**
     * @brief 主动断开 WebSocket 实时通道。
     */
    void disconnectRealtimeChannel();

    /**
     * @brief 在首次进入聊天窗口或切换到新的登录会话后，统一拉取会话列表和历史消息。
     */
    void initializeConversationDataIfNeeded();

    /**
     * @brief 发起或复用一对一私聊会话。
     * @param peerUserId 目标好友 user_id。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void createPrivateConversation(
        const QString &peerUserId,
        CreatePrivateConversationSuccessHandler onSuccess,
        CreatePrivateConversationFailureHandler onFailure);

    /**
     * @brief 用完整会话列表快照刷新列表模型。
     * @param response 服务端会话列表响应 DTO。
     */
    void applyConversationListSnapshot(
        const chatclient::dto::conversation::ConversationListResponseDto
            &response);

    /**
     * @brief 用完整历史消息快照刷新指定会话的消息模型。
     * @param conversationId 会话唯一标识。
     * @param response 服务端历史消息响应 DTO。
     */
    void applyConversationMessagesSnapshot(
        const QString &conversationId,
        const chatclient::dto::conversation::ConversationMessageListResponseDto
            &response);

    /**
     * @brief 追加一条仅用于当前客户端演示态的本地文本消息。
     * @param conversationId 会话唯一标识。
     * @param author 发送者名称。
     * @param text 文本内容。
     * @param timeText 展示时间。
     * @param fromSelf 是否为当前用户发送。
     */
    void appendLocalTextMessage(const QString &conversationId,
                                const QString &author,
                                const QString &text,
                                const QString &timeText,
                                bool fromSelf);

    /**
     * @brief 返回指定会话当前的轻量运行时状态。
     * @param conversationId 会话唯一标识。
     * @return 该会话当前运行时状态的副本。
     */
    ConversationRuntimeState conversationState(
        const QString &conversationId) const;

  signals:
    /**
     * @brief 实时通道状态文本发生变化。
     * @param statusText 适合直接展示到界面的状态文本。
     */
    void realtimeStatusChanged(const QString &statusText);

    /**
     * @brief 实时通道认证成功。
     * @param userId 服务端确认的当前用户 ID。
     * @param deviceSessionId 服务端确认的设备会话 ID。
     */
    void realtimeAuthenticated(const QString &userId,
                               const QString &deviceSessionId);

    /**
     * @brief 实时通道认证失败。
     * @param message 适合直接展示给用户的失败原因。
     */
    void realtimeAuthenticationFailed(const QString &message);

    /**
     * @brief 会话初始化同步已经开始。
     */
    void conversationBootstrapStarted();

    /**
     * @brief 会话列表展示数据已刷新。
     */
    void conversationListUpdated();

    /**
     * @brief 会话初始化同步已完成。
     */
    void conversationBootstrapFinished();

    /**
     * @brief 会话初始化同步失败。
     * @param message 适合直接展示给用户的失败原因。
     */
    void conversationBootstrapFailed(const QString &message);

    /**
     * @brief 收到服务端主动推送的 `ws.new` 业务事件。
     * @param route 当前业务路由。
     * @param data 当前业务载荷。
     */
    void realtimeNewEventReceived(const QString &route, const QJsonObject &data);

  private:
    ConversationRuntimeState &ensureState(const QString &conversationId);
    void handleRealtimeNewEvent(const QString &route, const QJsonObject &data);
    void resetConversationData();
    QString currentSessionKey() const;
    static QString localizeConversationApiError(
        const chatclient::dto::conversation::ApiErrorDto &error);

    chatclient::api::ConversationApiClient *m_conversationApiClient = nullptr;
    chatclient::ws::ChatWsClient *m_chatWsClient = nullptr;
    chatclient::model::ConversationListModel *m_conversationListModel = nullptr;
    MessageModelRegistry *m_messageModelRegistry = nullptr;
    chatclient::service::AuthService *m_authService = nullptr;
    QHash<QString, ConversationRuntimeState> m_runtimeStates;
    QMetaObject::Connection m_logoutConnection;
    QString m_loadedSessionKey;
    QString m_bootstrapSessionKey;
    bool m_bootstrapInProgress = false;
};

}  // namespace chatclient::service
