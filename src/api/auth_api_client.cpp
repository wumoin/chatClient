#include "api/auth_api_client.h"

#include "config/appconfig.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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
    const QString requestId = createRequestId();
    const QUrl registerUrl = chatclient::config::AppConfig::instance().registerUrl();

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
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::auth::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

QString AuthApiClient::createRequestId()
{
    return QStringLiteral("req_client_register_%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
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

}  // namespace chatclient::api
