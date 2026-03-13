#include "service/auth_service.h"

#include "log/app_logger.h"

#include <QRegularExpression>

namespace chatclient::service {
namespace {

constexpr int kMinAccountLength = 3;
constexpr int kMaxAccountLength = 64;
constexpr int kMinPasswordLength = 8;
constexpr int kMaxPasswordLength = 72;
constexpr int kMaxNicknameLength = 64;

const QRegularExpression kAccountPattern(QStringLiteral("^[A-Za-z0-9_.-]+$"));

}  // namespace

AuthService::AuthService(QObject *parent)
    : QObject(parent),
      m_authApiClient(this)
{
    qRegisterMetaType<chatclient::dto::auth::RegisterUserDto>(
        "chatclient::dto::auth::RegisterUserDto");
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
            emit registerFailed(error.message.isEmpty()
                                    ? QStringLiteral("注册失败，请稍后重试")
                                    : error.message);
        });

    return true;
}

bool AuthService::isRegistering() const
{
    return m_registering;
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

}  // namespace chatclient::service
