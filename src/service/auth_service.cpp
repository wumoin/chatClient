#include "service/auth_service.h"

#include "log/app_logger.h"
#include "service/auth_error_localizer.h"

#include <QSettings>
#include <QRegularExpression>
#include <QSysInfo>
#include <QUuid>

namespace chatclient::service {
namespace {

constexpr int kMinAccountLength = 3;
constexpr int kMaxAccountLength = 64;
constexpr int kMinPasswordLength = 8;
constexpr int kMaxPasswordLength = 72;
constexpr int kMaxNicknameLength = 64;
constexpr int kMaxDeviceIdLength = 128;
constexpr int kMaxDevicePlatformLength = 32;
constexpr int kMaxDeviceNameLength = 128;

const QRegularExpression kAccountPattern(QStringLiteral("^[A-Za-z0-9_.-]+$"));
const QRegularExpression kDevicePlatformPattern(
    QStringLiteral("^[A-Za-z0-9_-]+$"));

constexpr auto kSettingsOrganization = "wumo";
constexpr auto kSettingsApplication = "chatClient";
constexpr auto kSettingsDeviceIdKey = "auth/device_id";
constexpr auto kSettingsAccountKey = "auth/account";
constexpr auto kSettingsUserIdKey = "auth/user_id";
constexpr auto kSettingsNicknameKey = "auth/nickname";
constexpr auto kSettingsAvatarUrlKey = "auth/avatar_url";
constexpr auto kSettingsDeviceSessionIdKey = "auth/device_session_id";
constexpr auto kSettingsAccessTokenKey = "auth/access_token";
constexpr auto kSettingsExpiresInSecKey = "auth/expires_in_sec";

QSettings authSettings()
{
    return QSettings(QString::fromLatin1(kSettingsOrganization),
                     QString::fromLatin1(kSettingsApplication));
}

}  // namespace

AuthService::AuthService(QObject *parent)
    : QObject(parent),
      m_authApiClient(this)
{
    qRegisterMetaType<chatclient::dto::auth::RegisterUserDto>(
        "chatclient::dto::auth::RegisterUserDto");
    qRegisterMetaType<chatclient::dto::auth::LoginSessionDto>(
        "chatclient::dto::auth::LoginSessionDto");

    restoreSession();
}

bool AuthService::registerUser(const QString &account,
                               const QString &nickname,
                               const QString &password,
                               const QString &confirmPassword,
                               QString *errorMessage)
{
    if (m_registering)
    {
        CHATCLIENT_LOG_WARN("auth.service")
            << "register request ignored because another request is still running";
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("注册请求正在提交，请稍候");
        }
        return false;
    }

    chatclient::dto::auth::RegisterRequestDto request;
    if (!buildRegisterRequest(account,
                              nickname,
                              password,
                              confirmPassword,
                              &request,
                              errorMessage))
    {
        CHATCLIENT_LOG_WARN("auth.service")
            << "register validation failed account="
            << account
            << " reason="
            << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    m_registering = true;
    emit registerStarted();
    CHATCLIENT_LOG_INFO("auth.service")
        << "register request started account="
        << request.account
        << " nickname="
        << request.nickname;

    m_authApiClient.registerUser(
        request,
        [this](const chatclient::dto::auth::RegisterResponseDto &response) {
            m_registering = false;
            CHATCLIENT_LOG_INFO("auth.service")
                << "register request completed request_id="
                << response.requestId
                << " user_id="
                << response.user.userId;
            emit registerSucceeded(response.user);
        },
        [this](const chatclient::dto::auth::ApiErrorDto &error) {
            m_registering = false;
            CHATCLIENT_LOG_WARN("auth.service")
                << "register request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeAuthError(error);
            emit registerFailed(localizedMessage.isEmpty()
                                    ? QStringLiteral("注册失败，请稍后重试")
                                    : localizedMessage);
        });

    return true;
}

bool AuthService::loginUser(const QString &account,
                            const QString &password,
                            QString *errorMessage)
{
    if (m_loggingIn)
    {
        CHATCLIENT_LOG_WARN("auth.service")
            << "login request ignored because another request is still running";
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("登录请求正在提交，请稍候");
        }
        return false;
    }

    chatclient::dto::auth::LoginRequestDto request;
    if (!buildLoginRequest(account, password, &request, errorMessage))
    {
        CHATCLIENT_LOG_WARN("auth.service")
            << "login validation failed account="
            << account
            << " reason="
            << (errorMessage != nullptr ? *errorMessage : QString());
        return false;
    }

    m_loggingIn = true;
    emit loginStarted();
    CHATCLIENT_LOG_INFO("auth.service")
        << "login request started account="
        << request.account
        << " device_id="
        << request.deviceId;

    m_authApiClient.loginUser(
        request,
        [this, account](const chatclient::dto::auth::LoginResponseDto &response) {
            m_loggingIn = false;

            chatclient::dto::auth::LoginSessionDto session;
            session.account = account;
            session.user = response.user;
            session.deviceSessionId = response.deviceSessionId;
            session.accessToken = response.accessToken;
            session.expiresInSec = response.expiresInSec;

            persistSession(session);

            CHATCLIENT_LOG_INFO("auth.service")
                << "login request completed request_id="
                << response.requestId
                << " user_id="
                << response.user.userId
                << " device_session_id="
                << response.deviceSessionId;
            emit loginSucceeded(m_currentSession);
        },
        [this](const chatclient::dto::auth::ApiErrorDto &error) {
            m_loggingIn = false;
            CHATCLIENT_LOG_WARN("auth.service")
                << "login request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeAuthError(error);
            emit loginFailed(localizedMessage.isEmpty()
                                 ? QStringLiteral("登录失败，请稍后重试")
                                 : localizedMessage);
        });

    return true;
}

bool AuthService::logoutUser(QString *errorMessage)
{
    if (m_loggingOut)
    {
        CHATCLIENT_LOG_WARN("auth.service")
            << "logout request ignored because another logout is still running";
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("正在退出登录，请稍候");
        }
        return false;
    }

    if (m_loggingIn || m_registering)
    {
        CHATCLIENT_LOG_WARN("auth.service")
            << "logout request ignored because another auth request is still running";
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("当前仍有认证请求在处理中，请稍候");
        }
        return false;
    }

    if (!m_hasActiveSession || m_currentSession.accessToken.trimmed().isEmpty())
    {
        CHATCLIENT_LOG_INFO("auth.service")
            << "logout requested without active session, clearing local state directly";
        clearPersistedSession();
        emit logoutSucceeded();
        return true;
    }

    m_loggingOut = true;
    emit logoutStarted();
    CHATCLIENT_LOG_INFO("auth.service")
        << "logout request started user_id="
        << m_currentSession.user.userId
        << " device_session_id="
        << m_currentSession.deviceSessionId;

    m_authApiClient.logoutUser(
        m_currentSession.accessToken,
        [this](const chatclient::dto::auth::LogoutResponseDto &response) {
            m_loggingOut = false;
            clearPersistedSession();
            CHATCLIENT_LOG_INFO("auth.service")
                << "logout request completed request_id="
                << response.requestId;
            emit logoutSucceeded();
        },
        [this](const chatclient::dto::auth::ApiErrorDto &error) {
            m_loggingOut = false;
            CHATCLIENT_LOG_WARN("auth.service")
                << "logout request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;

            // access_token 已失效时，客户端本地会话也不再可用，直接视为已退出。
            if (error.errorCode == 40102 || error.message.trimmed() == QStringLiteral("invalid access token"))
            {
                clearPersistedSession();
                emit logoutSucceeded();
                return;
            }

            const QString localizedMessage = localizeAuthError(error);
            emit logoutFailed(localizedMessage.isEmpty()
                                  ? QStringLiteral("退出登录失败，请稍后重试")
                                  : localizedMessage);
        });

    return true;
}

bool AuthService::logoutUserBlocking(QString *errorMessage)
{
    if (m_hasActiveSession && !m_currentSession.accessToken.trimmed().isEmpty())
    {
        chatclient::dto::auth::ApiErrorDto error;
        const bool ok = m_authApiClient.logoutUserBlocking(
            m_currentSession.accessToken, nullptr, &error);
        if (!ok)
        {
            CHATCLIENT_LOG_WARN("auth.service")
                << "blocking logout request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;

            const bool tokenInvalid =
                error.errorCode == 40102 ||
                error.message.trimmed() == QStringLiteral("invalid access token");
            if (!tokenInvalid && errorMessage)
            {
                const QString localizedMessage = localizeAuthError(error);
                *errorMessage = localizedMessage.isEmpty()
                                    ? QStringLiteral("退出登录失败")
                                    : localizedMessage;
            }

            clearPersistedSession();
            return tokenInvalid;
        }
    }

    clearPersistedSession();
    return true;
}

bool AuthService::isRegistering() const
{
    return m_registering;
}

bool AuthService::isLoggingIn() const
{
    return m_loggingIn;
}

bool AuthService::isLoggingOut() const
{
    return m_loggingOut;
}

bool AuthService::hasActiveSession() const
{
    return m_hasActiveSession;
}

const chatclient::dto::auth::LoginSessionDto &AuthService::currentSession() const
{
    return m_currentSession;
}

bool AuthService::buildRegisterRequest(
    const QString &account,
    const QString &nickname,
    const QString &password,
    const QString &confirmPassword,
    chatclient::dto::auth::RegisterRequestDto *out,
    QString *errorMessage)
{
    // 账号规则尽量与服务端保持一致。
    // 这样可以在用户点击注册按钮后第一时间给出明确反馈，而不是把所有错误都拖到网络往返之后。
    if (account.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("请输入注册账号");
        }
        return false;
    }

    if (account != account.trimmed())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("账号不能包含前后空格");
        }
        return false;
    }

    if (account.size() < kMinAccountLength || account.size() > kMaxAccountLength)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("账号长度必须在 %1 到 %2 个字符之间")
                                .arg(kMinAccountLength)
                                .arg(kMaxAccountLength);
        }
        return false;
    }

    if (!kAccountPattern.match(account).hasMatch())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("账号只允许字母、数字、下划线、点和短横线");
        }
        return false;
    }

    const QString trimmedNickname = nickname.trimmed();
    if (trimmedNickname.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("请输入昵称");
        }
        return false;
    }

    if (trimmedNickname.size() > kMaxNicknameLength)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("昵称长度不能超过 %1 个字符")
                                .arg(kMaxNicknameLength);
        }
        return false;
    }

    if (password.size() < kMinPasswordLength || password.size() > kMaxPasswordLength)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("密码长度必须在 %1 到 %2 个字符之间")
                                .arg(kMinPasswordLength)
                                .arg(kMaxPasswordLength);
        }
        return false;
    }

    if (password != confirmPassword)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("两次输入的密码不一致");
        }
        return false;
    }

    if (out)
    {
        out->account = account;
        out->nickname = trimmedNickname;
        out->password = password;
        out->avatarUrl.clear();
    }

    return true;
}

bool AuthService::buildLoginRequest(
    const QString &account,
    const QString &password,
    chatclient::dto::auth::LoginRequestDto *out,
    QString *errorMessage)
{
    if (account.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("请输入登录账号");
        }
        return false;
    }

    if (account != account.trimmed())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("账号不能包含前后空格");
        }
        return false;
    }

    if (account.size() < kMinAccountLength || account.size() > kMaxAccountLength)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("账号长度必须在 %1 到 %2 个字符之间")
                                .arg(kMinAccountLength)
                                .arg(kMaxAccountLength);
        }
        return false;
    }

    if (!kAccountPattern.match(account).hasMatch())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("账号只允许字母、数字、下划线、点和短横线");
        }
        return false;
    }

    if (password.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("请输入登录密码");
        }
        return false;
    }

    const QString resolvedDeviceId = deviceId();
    if (resolvedDeviceId.isEmpty() ||
        resolvedDeviceId != resolvedDeviceId.trimmed() ||
        resolvedDeviceId.size() > kMaxDeviceIdLength)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("本机设备标识无效，请重启客户端后重试");
        }
        return false;
    }

    const QString resolvedDevicePlatform = devicePlatform().trimmed().toLower();
    if (resolvedDevicePlatform.isEmpty() ||
        resolvedDevicePlatform.size() > kMaxDevicePlatformLength ||
        !kDevicePlatformPattern.match(resolvedDevicePlatform).hasMatch())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("当前客户端设备平台标识无效");
        }
        return false;
    }

    QString resolvedDeviceName = deviceName().trimmed();
    if (resolvedDeviceName.size() > kMaxDeviceNameLength)
    {
        resolvedDeviceName = resolvedDeviceName.left(kMaxDeviceNameLength);
    }

    if (out)
    {
        out->account = account;
        out->password = password;
        out->deviceId = resolvedDeviceId;
        out->devicePlatform = resolvedDevicePlatform;
        out->deviceName = resolvedDeviceName;
        out->clientVersion.clear();
    }

    return true;
}

QString AuthService::deviceId()
{
    QSettings settings = authSettings();
    const QString storedDeviceId =
        settings.value(QString::fromLatin1(kSettingsDeviceIdKey))
            .toString()
            .trimmed();
    if (!storedDeviceId.isEmpty())
    {
        return storedDeviceId;
    }

    const QString generatedDeviceId = QStringLiteral("desktop_%1")
                                          .arg(QUuid::createUuid().toString(
                                              QUuid::WithoutBraces));
    settings.setValue(QString::fromLatin1(kSettingsDeviceIdKey),
                      generatedDeviceId);
    return generatedDeviceId;
}

QString AuthService::devicePlatform()
{
    return QStringLiteral("desktop");
}

QString AuthService::deviceName()
{
    const QString productName = QSysInfo::prettyProductName().trimmed();
    const QString hostName = QSysInfo::machineHostName().trimmed();

    if (!productName.isEmpty() && !hostName.isEmpty())
    {
        return QStringLiteral("%1 (%2)").arg(productName, hostName);
    }

    if (!productName.isEmpty())
    {
        return productName;
    }

    if (!hostName.isEmpty())
    {
        return hostName;
    }

    return QStringLiteral("Desktop Client");
}

void AuthService::persistSession(
    const chatclient::dto::auth::LoginSessionDto &session)
{
    m_currentSession = session;
    m_hasActiveSession = !session.accessToken.isEmpty() &&
                         !session.deviceSessionId.isEmpty() &&
                         !session.user.userId.isEmpty();

    if (!m_hasActiveSession)
    {
        return;
    }

    QSettings settings = authSettings();
    settings.setValue(QString::fromLatin1(kSettingsAccountKey), session.account);
    settings.setValue(QString::fromLatin1(kSettingsUserIdKey),
                      session.user.userId);
    settings.setValue(QString::fromLatin1(kSettingsNicknameKey),
                      session.user.nickname);
    settings.setValue(QString::fromLatin1(kSettingsAvatarUrlKey),
                      session.user.avatarUrl);
    settings.setValue(QString::fromLatin1(kSettingsDeviceSessionIdKey),
                      session.deviceSessionId);
    settings.setValue(QString::fromLatin1(kSettingsAccessTokenKey),
                      session.accessToken);
    settings.setValue(QString::fromLatin1(kSettingsExpiresInSecKey),
                      session.expiresInSec);
}

void AuthService::clearPersistedSession()
{
    m_currentSession = chatclient::dto::auth::LoginSessionDto();
    m_hasActiveSession = false;

    QSettings settings = authSettings();
    settings.remove(QString::fromLatin1(kSettingsAccountKey));
    settings.remove(QString::fromLatin1(kSettingsUserIdKey));
    settings.remove(QString::fromLatin1(kSettingsNicknameKey));
    settings.remove(QString::fromLatin1(kSettingsAvatarUrlKey));
    settings.remove(QString::fromLatin1(kSettingsDeviceSessionIdKey));
    settings.remove(QString::fromLatin1(kSettingsAccessTokenKey));
    settings.remove(QString::fromLatin1(kSettingsExpiresInSecKey));
}

void AuthService::restoreSession()
{
    QSettings settings = authSettings();

    chatclient::dto::auth::LoginSessionDto session;
    session.account =
        settings.value(QString::fromLatin1(kSettingsAccountKey))
            .toString()
            .trimmed();
    session.user.userId =
        settings.value(QString::fromLatin1(kSettingsUserIdKey))
            .toString()
            .trimmed();
    session.user.nickname =
        settings.value(QString::fromLatin1(kSettingsNicknameKey))
            .toString()
            .trimmed();
    session.user.avatarUrl =
        settings.value(QString::fromLatin1(kSettingsAvatarUrlKey))
            .toString()
            .trimmed();
    session.deviceSessionId =
        settings.value(QString::fromLatin1(kSettingsDeviceSessionIdKey))
            .toString()
            .trimmed();
    session.accessToken =
        settings.value(QString::fromLatin1(kSettingsAccessTokenKey))
            .toString()
            .trimmed();
    session.expiresInSec =
        settings.value(QString::fromLatin1(kSettingsExpiresInSecKey))
            .toLongLong();

    m_currentSession = session;
    m_hasActiveSession = !session.accessToken.isEmpty() &&
                         !session.deviceSessionId.isEmpty() &&
                         !session.user.userId.isEmpty();

    if (m_hasActiveSession)
    {
        CHATCLIENT_LOG_INFO("auth.service")
            << "restored local session user_id="
            << m_currentSession.user.userId
            << " device_session_id="
            << m_currentSession.deviceSessionId;
    }
}

}  // namespace chatclient::service
