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
 * 当前只承接一条最小链路：
 * 1) 创建或复用一对一私聊会话。
 */
class ConversationApiClient : public QObject
{
    Q_OBJECT

  public:
    using CreatePrivateConversationSuccessHandler = std::function<void(
        const chatclient::dto::conversation::CreatePrivateConversationResponseDto &)>;
    using CreatePrivateConversationFailureHandler =
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
