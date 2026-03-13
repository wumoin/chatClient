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
 * 这一层只负责把注册请求发给服务端，并把响应解析成 DTO。
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

private:
    /**
     * @brief 创建客户端本地 request_id。
     * @return 带业务前缀的请求追踪 ID。
     */
    static QString createRequestId();

    /**
     * @brief 为 JSON 请求统一设置请求头。
     * @param request 待发送的网络请求对象。
     * @param requestId 当前请求追踪 ID。
     */
    static void applyJsonHeaders(QNetworkRequest *request,
                                 const QString &requestId);

    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

}  // namespace chatclient::api
