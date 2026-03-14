#include "api/auth_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUuid>

namespace chatclient::api {

AuthApiClient::AuthApiClient(QObject *parent)
    : QObject(parent),
      m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void AuthApiClient::registerUser(
    const chatclient::dto::auth::RegisterRequestDto &request,
    RegisterSuccessHandler onSuccess,
    RegisterFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("register"));
    const QUrl registerUrl = chatclient::config::AppConfig::instance().registerUrl();

    CHATCLIENT_LOG_INFO("auth.api")
        << "sending register request request_id="
        << requestId
        << " url="
        << registerUrl.toString()
        << " account="
        << request.account;

    // 注册接口属于标准 JSON POST 请求。
    // 这里统一从 AppConfig 读取地址，并注入 request_id，避免窗口层重复拼接 URL。
    QNetworkRequest networkRequest(registerUrl);
    applyJsonHeaders(&networkRequest, requestId);

    const QByteArray requestBody =
        QJsonDocument(chatclient::dto::auth::toJsonObject(request))
            .toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_networkAccessManager->post(networkRequest, requestBody);

    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply,
             requestId,
             onSuccess = std::move(onSuccess),
             onFailure = std::move(onFailure)]() mutable {
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const QByteArray responseBody = reply->readAll();
                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("服务端返回了无法识别的注册响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError && document.isObject();

                // 只要 HTTP 状态码落在 2xx，就按成功响应尝试解析。
                // 真正的业务码校验由 DTO 解析函数完成，避免重复散落在窗口层。
                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::auth::RegisterResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::auth::parseRegisterSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("auth.api")
                            << "register request succeeded request_id="
                            << response.requestId
                            << " http_status="
                            << httpStatus
                            << " user_id="
                            << response.user.userId;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = requestId;
                        error.message = errorMessage.isEmpty()
                                            ? fallbackMessage
                                            : errorMessage;
                        CHATCLIENT_LOG_ERROR("auth.api")
                            << "register success response parse failed request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " error="
                            << error.message;
                        onFailure(error);
                    }

                    reply->deleteLater();
                    return;
                }

                if (onFailure)
                {
                    if (hasJsonObject)
                    {
                        auto error = chatclient::dto::auth::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        CHATCLIENT_LOG_WARN("auth.api")
                            << "register request failed request_id="
                            << error.requestId
                            << " http_status="
                            << httpStatus
                            << " error_code="
                            << error.errorCode
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        CHATCLIENT_LOG_WARN("auth.api")
                            << "register request returned non-json response request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void AuthApiClient::loginUser(
    const chatclient::dto::auth::LoginRequestDto &request,
    LoginSuccessHandler onSuccess,
    LoginFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("login"));
    const QUrl loginUrl = chatclient::config::AppConfig::instance().loginUrl();

    CHATCLIENT_LOG_INFO("auth.api")
        << "sending login request request_id="
        << requestId
        << " url="
        << loginUrl.toString()
        << " account="
        << request.account
        << " device_id="
        << request.deviceId;

    QNetworkRequest networkRequest(loginUrl);
    applyJsonHeaders(&networkRequest, requestId);

    const QByteArray requestBody =
        QJsonDocument(chatclient::dto::auth::toJsonObject(request))
            .toJson(QJsonDocument::Compact);
    QNetworkReply *reply =
        m_networkAccessManager->post(networkRequest, requestBody);

    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply,
             requestId,
             onSuccess = std::move(onSuccess),
             onFailure = std::move(onFailure)]() mutable {
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const QByteArray responseBody = reply->readAll();
                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("服务端返回了无法识别的登录响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError && document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::auth::LoginResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::auth::parseLoginSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("auth.api")
                            << "login request succeeded request_id="
                            << response.requestId
                            << " http_status="
                            << httpStatus
                            << " user_id="
                            << response.user.userId
                            << " device_session_id="
                            << response.deviceSessionId;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = requestId;
                        error.message = errorMessage.isEmpty()
                                            ? fallbackMessage
                                            : errorMessage;
                        CHATCLIENT_LOG_ERROR("auth.api")
                            << "login success response parse failed request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " error="
                            << error.message;
                        onFailure(error);
                    }

                    reply->deleteLater();
                    return;
                }

                if (onFailure)
                {
                    if (hasJsonObject)
                    {
                        auto error = chatclient::dto::auth::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        CHATCLIENT_LOG_WARN("auth.api")
                            << "login request failed request_id="
                            << error.requestId
                            << " http_status="
                            << httpStatus
                            << " error_code="
                            << error.errorCode
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        CHATCLIENT_LOG_WARN("auth.api")
                            << "login request returned non-json response request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void AuthApiClient::logoutUser(const QString &accessToken,
                               LogoutSuccessHandler onSuccess,
                               LogoutFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("logout"));
    const QUrl logoutUrl = chatclient::config::AppConfig::instance().logoutUrl();

    CHATCLIENT_LOG_INFO("auth.api")
        << "sending logout request request_id="
        << requestId
        << " url="
        << logoutUrl.toString();

    QNetworkRequest networkRequest(logoutUrl);
    applyJsonHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->post(networkRequest, QByteArray());

    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply,
             requestId,
             onSuccess = std::move(onSuccess),
             onFailure = std::move(onFailure)]() mutable {
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const QByteArray responseBody = reply->readAll();
                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("服务端返回了无法识别的登出响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError && document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::auth::LogoutResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::auth::parseLogoutSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("auth.api")
                            << "logout request succeeded request_id="
                            << response.requestId
                            << " http_status="
                            << httpStatus;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = requestId;
                        error.message = errorMessage.isEmpty()
                                            ? fallbackMessage
                                            : errorMessage;
                        CHATCLIENT_LOG_ERROR("auth.api")
                            << "logout success response parse failed request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " error="
                            << error.message;
                        onFailure(error);
                    }

                    reply->deleteLater();
                    return;
                }

                if (onFailure)
                {
                    if (hasJsonObject)
                    {
                        auto error = chatclient::dto::auth::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        CHATCLIENT_LOG_WARN("auth.api")
                            << "logout request failed request_id="
                            << error.requestId
                            << " http_status="
                            << httpStatus
                            << " error_code="
                            << error.errorCode
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        CHATCLIENT_LOG_WARN("auth.api")
                            << "logout request returned non-json response request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

bool AuthApiClient::logoutUserBlocking(
    const QString &accessToken,
    chatclient::dto::auth::LogoutResponseDto *out,
    chatclient::dto::auth::ApiErrorDto *error)
{
    const QString requestId = createRequestId(QStringLiteral("logout"));
    const QUrl logoutUrl = chatclient::config::AppConfig::instance().logoutUrl();

    CHATCLIENT_LOG_INFO("auth.api")
        << "sending blocking logout request request_id="
        << requestId
        << " url="
        << logoutUrl.toString();

    QNetworkRequest networkRequest(logoutUrl);
    applyJsonHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->post(networkRequest, QByteArray());

    QEventLoop eventLoop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    bool finished = false;
    connect(reply, &QNetworkReply::finished, &eventLoop, [&]() {
        finished = true;
        eventLoop.quit();
    });
    connect(&timeoutTimer, &QTimer::timeout, &eventLoop, &QEventLoop::quit);
    timeoutTimer.start(2500);
    eventLoop.exec();

    if (!finished)
    {
        reply->abort();
        if (error)
        {
            error->httpStatus = 0;
            error->errorCode = 50000;
            error->requestId = requestId;
            error->message = QStringLiteral("登出请求超时");
        }
        CHATCLIENT_LOG_WARN("auth.api")
            << "blocking logout request timed out request_id="
            << requestId;
        reply->deleteLater();
        return false;
    }

    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();
    const QString fallbackMessage =
        reply->error() == QNetworkReply::NoError
            ? QStringLiteral("服务端返回了无法识别的登出响应")
            : reply->errorString();

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(responseBody, &parseError);
    const bool hasJsonObject =
        parseError.error == QJsonParseError::NoError && document.isObject();

    if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
    {
        chatclient::dto::auth::LogoutResponseDto response;
        QString errorMessage;
        if (chatclient::dto::auth::parseLogoutSuccessResponse(
                document.object(), &response, &errorMessage))
        {
            if (response.requestId.isEmpty())
            {
                response.requestId = requestId;
            }
            if (out)
            {
                *out = response;
            }
            reply->deleteLater();
            return true;
        }

        if (error)
        {
            error->httpStatus = httpStatus;
            error->errorCode = 50000;
            error->requestId = requestId;
            error->message = errorMessage.isEmpty() ? fallbackMessage : errorMessage;
        }
        reply->deleteLater();
        return false;
    }

    chatclient::dto::auth::ApiErrorDto parsedError;
    if (hasJsonObject)
    {
        parsedError = chatclient::dto::auth::parseApiErrorResponse(
            document.object(), httpStatus, fallbackMessage);
    }
    else
    {
        parsedError.httpStatus = httpStatus;
        parsedError.message = fallbackMessage;
    }

    if (parsedError.requestId.isEmpty())
    {
        parsedError.requestId = requestId;
    }

    if (error)
    {
        *error = parsedError;
    }

    reply->deleteLater();
    return false;
}

QString AuthApiClient::createRequestId(const QString &action)
{
    return QStringLiteral("req_client_%1_%2")
        .arg(action,
             QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void AuthApiClient::applyJsonHeaders(QNetworkRequest *request,
                                     const QString &requestId)
{
    if (!request)
    {
        return;
    }

    request->setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/json"));
    request->setRawHeader("Accept", "application/json");
    request->setRawHeader("X-Request-Id", requestId.toUtf8());
}

void AuthApiClient::applyAuthorizationHeader(QNetworkRequest *request,
                                             const QString &accessToken)
{
    if (!request || accessToken.trimmed().isEmpty())
    {
        return;
    }

    request->setRawHeader(
        "Authorization",
        QStringLiteral("Bearer %1").arg(accessToken.trimmed()).toUtf8());
}

}  // namespace chatclient::api
