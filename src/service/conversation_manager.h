#pragma once

#include "api/conversation_api_client.h"
#include "dto/conversation_dto.h"
#include "model/messagemodel.h"

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVector>

#include <functional>

class MessageModel;
class MessageModelRegistry;

namespace chatclient::api {
class FileApiClient;
}

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

    struct PendingTextMessage
    {
        QString conversationId;
        QString clientMessageId;
        QString text;
        qint64 sentAtMs = 0;
    };

    struct PendingImageMessage
    {
        // 当前图片消息所属会话。
        QString conversationId;
        // 客户端本地消息 ID；用于把上传、ack、new 三段结果收敛到同一行消息。
        QString clientMessageId;
        // 本地图片路径；上传中和刚 ack 完成时都优先用它做即时预览。
        QString localPath;
        // 当前图片说明文本。
        QString caption;
        // 本地探测到的图片尺寸，便于图片气泡在正式消息到来前先稳定布局。
        int imageWidth = 0;
        int imageHeight = 0;
        // 客户端本地发起时间；在服务端 created_at_ms 回来前先用于时间展示。
        qint64 sentAtMs = 0;
        // 若服务端已确认正式消息，则会写入下面两个字段。
        QString messageId;
        qint64 seq = 0;
        qint64 createdAtMs = 0;
        // 当前图片消息的传输状态与直接展示给 delegate 的状态文案。
        MessageTransferState transferState = MessageTransferState::Uploading;
        int transferProgress = 0;
        QString transferStatusText;
    };

    struct RemoteImageHydrationTarget
    {
        // 目标图片消息所在会话；后续局部刷新时需要先路由到正确的 MessageModel。
        QString conversationId;
        // 优先使用正式 message_id 命中同一条消息。
        QString messageId;
        // 若本地仍在依赖 client_message_id，也允许作为第二层身份键。
        QString clientMessageId;
        // 某些历史快照可能只剩 seq，这里继续保留第三层兜底键。
        qint64 seq = 0;
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
     * @brief 发送一条图片消息。
     *
     * 当前链路会依次执行：
     * 1. 先把本地图片以“上传中”占位消息写入会话模型；
     * 2. 再通过 HTTP 上传临时附件；
     * 3. 上传成功后通过 `ws.send + route=message.send_image` 确认正式消息；
     * 4. 最终由 `ws.ack / ws.new` 把本地占位消息收敛成正式消息。
     *
     * @param conversationId 会话唯一标识。
     * @param localPath 本地图片路径。
     * @param caption 可选图片说明文本。
     * @return true 表示图片发送流程已经启动；false 表示当前参数或运行时条件不满足。
     */
    bool sendImageMessage(const QString &conversationId,
                          const QString &localPath,
                          const QString &caption = QString());

    /**
     * @brief 通过 WS 发送一条文本消息。
     * @param conversationId 会话唯一标识。
     * @param text 待发送的文本内容。
     * @return true 表示请求已发出；false 表示当前通道不可用或参数无效。
     */
    bool sendTextMessage(const QString &conversationId, const QString &text);

    /**
     * @brief 在本地将指定会话标记为已读，并清空未读数。
     * @param conversationId 会话唯一标识。
     * @return true 表示本地摘要已发生变化；false 表示无需更新。
     */
    bool markConversationReadLocally(const QString &conversationId);

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
    void handleRealtimeAckEvent(const QString &route,
                                bool ok,
                                int code,
                                const QString &message,
                                const QJsonObject &data,
                                const QString &requestId);
    void handleMessageSendTextAck(bool ok,
                                  int code,
                                  const QString &message,
                                  const QJsonObject &data,
                                  const QString &requestId);
    void handleMessageSendImageAck(bool ok,
                                   int code,
                                   const QString &message,
                                   const QJsonObject &data,
                                   const QString &requestId);
    void handleAttachmentUploadProgress(const QString &requestId,
                                        qint64 bytesSent,
                                        qint64 bytesTotal);
    void handleRealtimeNewEvent(const QString &route, const QJsonObject &data);
    void handleMessageCreatedEvent(const QJsonObject &data);
    void handleConversationCreatedEvent(const QJsonObject &data);
    void handleFriendRealtimeEvent(const QString &route);
    MessageItem buildPendingImageMessageItem(
        const PendingImageMessage &pending) const;
    void upsertPendingImageMessage(const PendingImageMessage &pending);
    void markPendingImageMessageFailed(const QString &clientMessageId,
                                       const QString &statusText);
    void removePendingImageMessage(const QString &clientMessageId);
    /**
     * @brief 若图片本地缓存已存在，则直接把缓存路径写回消息项。
     * @param item 待补齐本地图片路径的消息项；非图片消息或本来已有 localPath 时不会改动。
     */
    void applyCachedImageLocalPath(MessageItem *item) const;
    /**
     * @brief 为单条正式图片消息补齐“远端下载 -> 本地缓存 -> model 局部刷新”链路。
     * @param item 目标消息项；通常来自 HTTP 历史快照或 `ws.new(message.created)`。
     */
    void ensureRemoteImageAvailable(const MessageItem &item);
    /**
     * @brief 为一批消息统一补齐远端图片缓存。
     * @param items 刚刚写入 model 的消息集合。
     */
    void ensureRemoteImagesAvailable(const QVector<MessageItem> &items);
    void resetConversationData();
    QString currentSessionKey() const;
    static QString localizeConversationApiError(
        const chatclient::dto::conversation::ApiErrorDto &error);
    void updateConversationSummaryFromMessage(
        const chatclient::dto::conversation::ConversationMessageDto &message,
        const QString &previewText);
    QString currentUserId() const;
    /**
     * @brief 返回远端图片在本地缓存目录中的稳定路径。
     * @param remoteUrl 服务端返回的相对或绝对图片下载地址。
     * @return 对应缓存文件的绝对路径；remoteUrl 为空时返回空字符串。
     */
    QString remoteImageCacheFilePath(const QString &remoteUrl) const;

    chatclient::api::ConversationApiClient *m_conversationApiClient = nullptr;
    chatclient::api::FileApiClient *m_fileApiClient = nullptr;
    chatclient::ws::ChatWsClient *m_chatWsClient = nullptr;
    chatclient::model::ConversationListModel *m_conversationListModel = nullptr;
    MessageModelRegistry *m_messageModelRegistry = nullptr;
    chatclient::service::AuthService *m_authService = nullptr;
    QHash<QString, ConversationRuntimeState> m_runtimeStates;
    QMetaObject::Connection m_logoutConnection;
    QString m_loadedSessionKey;
    QString m_bootstrapSessionKey;
    bool m_bootstrapInProgress = false;
    QHash<QString, PendingTextMessage> m_pendingTextMessagesByRequestId;
    // client_message_id 是图片发送链路里最稳定的本地关联键。
    QHash<QString, PendingImageMessage> m_pendingImageMessagesByClientMessageId;
    // FileApiClient 的进度信号以 upload request_id 为主键，因此这里做一层映射。
    QHash<QString, QString> m_pendingImageClientIdsByUploadRequestId;
    // WS ack 同样只会带 request_id，因此再维护一层 ws request_id -> client_message_id。
    QHash<QString, QString> m_pendingImageClientIdsByWsRequestId;
    // 相同 remoteUrl 的正式图片只下载一次；下载完成后再统一回写所有等待中的消息行。
    QHash<QString, QVector<RemoteImageHydrationTarget>>
        m_remoteImageTargetsByUrl;
};

}  // namespace chatclient::service
