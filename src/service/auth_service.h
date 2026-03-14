#pragma once

#include "api/auth_api_client.h"
#include "dto/auth_dto.h"

#include <QObject>

namespace chatclient::service {

/**
 * @brief 客户端认证业务服务。
 *
 * 当前承接客户端最小认证闭环：
 * 1) 做注册 / 登录表单本地校验；
 * 2) 调用 AuthApiClient 发起真实 HTTP 请求；
 * 3) 在登录成功后保存本地 access_token 和 device_session；
 * 4) 把结果转换成界面更容易消费的信号。
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
     * @brief 发起登录流程。
     * @param account 登录账号。
     * @param password 登录密码。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool loginUser(const QString &account,
                   const QString &password,
                   QString *errorMessage = nullptr);

    /**
     * @brief 发起当前本地会话的登出流程。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出或已按本地幂等完成；false 表示已有请求在进行中。
     */
    bool logoutUser(QString *errorMessage = nullptr);

    /**
     * @brief 以同步方式发起登出请求并清理本地登录态。
     * @param errorMessage 服务端登出失败时写入提示，可为空。
     * @return true 表示服务端确认登出或 token 已失效；false 表示仅完成本地清理。
     */
    bool logoutUserBlocking(QString *errorMessage = nullptr);

    /**
     * @brief 判断当前是否正在提交注册请求。
     * @return true 表示注册请求仍在进行中；false 表示当前空闲。
     */
    bool isRegistering() const;

    /**
     * @brief 判断当前是否正在提交登录请求。
     * @return true 表示登录请求仍在进行中；false 表示当前空闲。
     */
    bool isLoggingIn() const;

    /**
     * @brief 判断当前是否正在提交登出请求。
     * @return true 表示登出请求仍在进行中；false 表示当前空闲。
     */
    bool isLoggingOut() const;

    /**
     * @brief 判断当前是否已持有本地登录态。
     * @return true 表示当前已有可复用的本地会话；false 表示尚未登录。
     */
    bool hasActiveSession() const;

    /**
     * @brief 返回当前本地登录会话。
     * @return 当前保存的登录会话 DTO。
     */
    const chatclient::dto::auth::LoginSessionDto &currentSession() const;

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

    /**
     * @brief 登录请求已开始提交。
     */
    void loginStarted();

    /**
     * @brief 登录成功。
     * @param session 客户端当前保存的登录会话。
     */
    void loginSucceeded(const chatclient::dto::auth::LoginSessionDto &session);

    /**
     * @brief 登录失败。
     * @param message 适合直接展示给用户的错误提示。
     */
    void loginFailed(const QString &message);

    /**
     * @brief 登出请求已开始提交。
     */
    void logoutStarted();

    /**
     * @brief 登出成功。
     */
    void logoutSucceeded();

    /**
     * @brief 登出失败。
     * @param message 适合直接展示给用户的错误提示。
     */
    void logoutFailed(const QString &message);

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

    /**
     * @brief 对登录表单做本地校验，并构造请求 DTO。
     * @param account 账号原始输入。
     * @param password 密码原始输入。
     * @param out 成功时写入标准化后的登录请求 DTO。
     * @param errorMessage 失败时写入错误消息。
     * @return true 表示本地校验通过；false 表示存在非法输入。
     */
    static bool buildLoginRequest(const QString &account,
                                  const QString &password,
                                  chatclient::dto::auth::LoginRequestDto *out,
                                  QString *errorMessage);

    /**
     * @brief 读取或生成当前客户端的稳定设备标识。
     * @return 可用于登录接口 `device_id` 的字符串。
     */
    static QString deviceId();

    /**
     * @brief 返回当前客户端设备平台标识。
     * @return 当前固定返回 `desktop`。
     */
    static QString devicePlatform();

    /**
     * @brief 返回当前客户端设备展示名。
     * @return 适合服务端设备页展示的设备名称。
     */
    static QString deviceName();

    /**
     * @brief 保存本地登录态。
     * @param session 当前登录成功后的会话信息。
     */
    void persistSession(const chatclient::dto::auth::LoginSessionDto &session);

    /**
     * @brief 清理当前本地登录态。
     */
    void clearPersistedSession();

    /**
     * @brief 从本地持久化介质恢复登录态。
     */
    void restoreSession();

    chatclient::api::AuthApiClient m_authApiClient;
    bool m_registering = false;
    bool m_loggingIn = false;
    bool m_loggingOut = false;
    bool m_hasActiveSession = false;
    chatclient::dto::auth::LoginSessionDto m_currentSession;
};

}  // namespace chatclient::service
