#pragma once

#include "dto/auth_dto.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkRequest;

namespace chatclient::api {

/**
 * @brief 认证 HTTP API 客户端。
 *
 * 这一层只负责把注册 / 登录请求发给服务端，并把响应解析成 DTO。
 * 它不负责界面提示、不负责登录态保存，也不直接操作窗口控件。
 */
class AuthApiClient : public QObject
{
    Q_OBJECT

public:
    using RegisterSuccessHandler =
        std::function<void(const chatclient::dto::auth::RegisterResponseDto &)>;
    using RegisterFailureHandler =
        std::function<void(const chatclient::dto::auth::ApiErrorDto &)>;
    using LoginSuccessHandler =
        std::function<void(const chatclient::dto::auth::LoginResponseDto &)>;
    using LoginFailureHandler =
        std::function<void(const chatclient::dto::auth::ApiErrorDto &)>;
    using LogoutSuccessHandler =
        std::function<void(const chatclient::dto::auth::LogoutResponseDto &)>;
    using LogoutFailureHandler =
        std::function<void(const chatclient::dto::auth::ApiErrorDto &)>;

    /**
     * @brief 构造认证接口客户端。
     * @param parent 父级 QObject，可为空。
     */
    explicit AuthApiClient(QObject *parent = nullptr);

    /**
     * @brief 发起注册请求。
     * @param request 注册请求 DTO。
     * @param onSuccess 服务端返回成功响应时调用的回调。
     * @param onFailure 请求失败、网络错误或响应解析失败时调用的回调。
     */
    void registerUser(const chatclient::dto::auth::RegisterRequestDto &request,
                      RegisterSuccessHandler onSuccess,
                      RegisterFailureHandler onFailure);

    /**
     * @brief 发起登录请求。
     * @param request 登录请求 DTO。
     * @param onSuccess 服务端返回成功响应时调用的回调。
     * @param onFailure 请求失败、网络错误或响应解析失败时调用的回调。
     */
    void loginUser(const chatclient::dto::auth::LoginRequestDto &request,
                   LoginSuccessHandler onSuccess,
                   LoginFailureHandler onFailure);

    /**
     * @brief 发起登出请求。
     * @param accessToken 当前登录会话的访问令牌。
     * @param onSuccess 服务端返回成功响应时调用的回调。
     * @param onFailure 请求失败、网络错误或响应解析失败时调用的回调。
     */
    void logoutUser(const QString &accessToken,
                    LogoutSuccessHandler onSuccess,
                    LogoutFailureHandler onFailure);

    /**
     * @brief 以同步方式发起登出请求。
     * @param accessToken 当前登录会话的访问令牌。
     * @param out 成功时写入解析后的登出响应 DTO，可为空。
     * @param error 失败时写入统一错误 DTO，可为空。
     * @return true 表示服务端已确认登出或当前 token 已失效；false 表示请求失败或超时。
     */
    bool logoutUserBlocking(const QString &accessToken,
                            chatclient::dto::auth::LogoutResponseDto *out = nullptr,
                            chatclient::dto::auth::ApiErrorDto *error = nullptr);

private:
    /**
     * @brief 创建客户端本地 request_id。
     * @param action 当前请求动作，例如 `register` 或 `login`。
     * @return 带业务前缀的请求追踪 ID。
     */
    static QString createRequestId(const QString &action);

    /**
     * @brief 为 JSON 请求统一设置请求头。
     * @param request 待发送的网络请求对象。
     * @param requestId 当前请求追踪 ID。
     */
    static void applyJsonHeaders(QNetworkRequest *request,
                                 const QString &requestId);

    /**
     * @brief 为需要认证的请求注入 Bearer Token。
     * @param request 待发送的网络请求对象。
     * @param accessToken 当前访问令牌。
     */
    static void applyAuthorizationHeader(QNetworkRequest *request,
                                         const QString &accessToken);

    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

}  // namespace chatclient::api
