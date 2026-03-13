#pragma once

#include "api/auth_api_client.h"
#include "dto/auth_dto.h"

#include <QObject>

namespace chatclient::service {

/**
 * @brief 客户端认证业务服务。
 *
 * 当前先落地注册功能：
 * 1) 做本地参数校验；
 * 2) 调用 AuthApiClient 发起 HTTP 注册请求；
 * 3) 把结果转换成界面更容易消费的信号。
 */
class AuthService : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造认证业务服务。
     * @param parent 父级 QObject，可为空。
     */
    explicit AuthService(QObject *parent = nullptr);

    /**
     * @brief 发起注册流程。
     * @param account 注册账号。
     * @param nickname 注册昵称。
     * @param password 登录密码。
     * @param confirmPassword 确认密码。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool registerUser(const QString &account,
                      const QString &nickname,
                      const QString &password,
                      const QString &confirmPassword,
                      QString *errorMessage = nullptr);

    /**
     * @brief 判断当前是否正在提交注册请求。
     * @return true 表示注册请求仍在进行中；false 表示当前空闲。
     */
    bool isRegistering() const;

signals:
    /**
     * @brief 注册请求已开始提交。
     */
    void registerStarted();

    /**
     * @brief 注册成功。
     * @param user 服务端返回的注册用户信息。
     */
    void registerSucceeded(const chatclient::dto::auth::RegisterUserDto &user);

    /**
     * @brief 注册失败。
     * @param message 适合直接展示给用户的错误提示。
     */
    void registerFailed(const QString &message);

private:
    /**
     * @brief 对注册表单做本地校验，并构造请求 DTO。
     * @param account 账号原始输入。
     * @param nickname 昵称原始输入。
     * @param password 密码原始输入。
     * @param confirmPassword 确认密码原始输入。
     * @param out 成功时写入标准化后的请求 DTO。
     * @param errorMessage 失败时写入错误消息。
     * @return true 表示本地校验通过；false 表示存在非法输入。
     */
    static bool buildRegisterRequest(const QString &account,
                                     const QString &nickname,
                                     const QString &password,
                                     const QString &confirmPassword,
                                     chatclient::dto::auth::RegisterRequestDto *out,
                                     QString *errorMessage);

    chatclient::api::AuthApiClient m_authApiClient;
    bool m_registering = false;
};

}  // namespace chatclient::service
