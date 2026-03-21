#pragma once

#include "dto/conversation_dto.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkRequest;

namespace chatclient::api {

/**
 * @brief 会话域 HTTP API 客户端。
 *
 * 当前承接会话域 5 条 HTTP 链路，但先不强制接入界面状态管理：
 * 1) 创建或复用一对一私聊会话；
 * 2) 获取当前用户会话列表；
 * 3) 获取指定会话详情；
 * 4) 获取指定会话历史消息；
 * 5) 发送文本消息。
 *
 * 这些能力都属于“HTTP 快照 / 命令式请求”：
 * - 拉列表、拉详情、拉历史消息时拿到的是某个时刻的快照；
 * - sendTextMessage 只表示本次 HTTP 提交成功，不负责补做 WS 实时分发或本地去重。
 */
class ConversationApiClient : public QObject
{
    Q_OBJECT

  public:
    using CreatePrivateConversationSuccessHandler = std::function<void(
        const chatclient::dto::conversation::CreatePrivateConversationResponseDto &)>;
    using CreatePrivateConversationFailureHandler =
        std::function<void(const chatclient::dto::conversation::ApiErrorDto &)>;
    using ConversationListSuccessHandler =
        std::function<void(const chatclient::dto::conversation::ConversationListResponseDto &)>;
    using ConversationListFailureHandler =
        std::function<void(const chatclient::dto::conversation::ApiErrorDto &)>;
    using ConversationDetailSuccessHandler = std::function<void(
        const chatclient::dto::conversation::ConversationDetailResponseDto &)>;
    using ConversationDetailFailureHandler =
        std::function<void(const chatclient::dto::conversation::ApiErrorDto &)>;
    using ConversationMessagesSuccessHandler = std::function<void(
        const chatclient::dto::conversation::ConversationMessageListResponseDto &)>;
    using ConversationMessagesFailureHandler =
        std::function<void(const chatclient::dto::conversation::ApiErrorDto &)>;
    using SendTextMessageSuccessHandler =
        std::function<void(const chatclient::dto::conversation::SendTextMessageResponseDto &)>;
    using SendTextMessageFailureHandler =
        std::function<void(const chatclient::dto::conversation::ApiErrorDto &)>;

    /**
     * @brief 构造会话接口客户端。
     * @param parent 父级 QObject，可为空。
     */
    explicit ConversationApiClient(QObject *parent = nullptr);

    /**
     * @brief 创建或复用一对一私聊会话。
     * @param accessToken 当前登录态 access token。
     * @param request 创建会话请求 DTO。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void createPrivateConversation(
        const QString &accessToken,
        const chatclient::dto::conversation::CreatePrivateConversationRequestDto &request,
        CreatePrivateConversationSuccessHandler onSuccess,
        CreatePrivateConversationFailureHandler onFailure);

    /**
     * @brief 获取当前登录用户的会话列表。
     * @param accessToken 当前登录态 access token。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void fetchConversations(const QString &accessToken,
                            ConversationListSuccessHandler onSuccess,
                            ConversationListFailureHandler onFailure);

    /**
     * @brief 获取指定会话详情。
     * @param accessToken 当前登录态 access token。
     * @param conversationId 目标会话 ID。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void fetchConversationDetail(const QString &accessToken,
                                 const QString &conversationId,
                                 ConversationDetailSuccessHandler onSuccess,
                                 ConversationDetailFailureHandler onFailure);

    /**
     * @brief 获取指定会话历史消息。
     * @param accessToken 当前登录态 access token。
     * @param conversationId 目标会话 ID。
     * @param request 历史消息查询请求 DTO。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void fetchConversationMessages(
        const QString &accessToken,
        const QString &conversationId,
        const chatclient::dto::conversation::ListConversationMessagesRequestDto &request,
        ConversationMessagesSuccessHandler onSuccess,
        ConversationMessagesFailureHandler onFailure);

    /**
     * @brief 向指定会话发送文本消息。
     * @param accessToken 当前登录态 access token。
     * @param conversationId 目标会话 ID。
     * @param request 文本消息请求 DTO。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void sendTextMessage(
        const QString &accessToken,
        const QString &conversationId,
        const chatclient::dto::conversation::SendTextMessageRequestDto &request,
        SendTextMessageSuccessHandler onSuccess,
        SendTextMessageFailureHandler onFailure);

  private:
    /**
     * @brief 创建客户端本地 request_id。
     * @param action 当前请求动作。
     * @return 带业务前缀的请求追踪 ID。
     */
    static QString createRequestId(const QString &action);

    /**
     * @brief 为请求设置统一的 Accept / request_id 请求头。
     * @param request 待发送的网络请求对象。
     * @param requestId 当前请求追踪 ID。
     */
    static void applyRequestHeaders(QNetworkRequest *request,
                                    const QString &requestId);

    /**
     * @brief 为需要 JSON 请求体的请求额外设置 Content-Type。
     * @param request 待发送的网络请求对象。
     */
    static void applyJsonRequestHeader(QNetworkRequest *request);

    /**
     * @brief 为请求注入 Bearer Token。
     * @param request 待发送的网络请求对象。
     * @param accessToken 当前访问令牌。
     */
    static void applyAuthorizationHeader(QNetworkRequest *request,
                                         const QString &accessToken);

    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

}  // namespace chatclient::api
