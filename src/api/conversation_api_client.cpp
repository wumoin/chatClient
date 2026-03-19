#include "api/conversation_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

namespace chatclient::api {

ConversationApiClient::ConversationApiClient(QObject *parent)
    : QObject(parent),
      m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void ConversationApiClient::createPrivateConversation(
    const QString &accessToken,
    const chatclient::dto::conversation::CreatePrivateConversationRequestDto &request,
    CreatePrivateConversationSuccessHandler onSuccess,
    CreatePrivateConversationFailureHandler onFailure)
{
    const QString requestId =
        createRequestId(QStringLiteral("create_private"));
    const QUrl url =
        chatclient::config::AppConfig::instance().conversationPrivateUrl();

    CHATCLIENT_LOG_INFO("conversation.api")
        << "create private conversation request started request_id="
        << requestId
        << " peer_user_id="
        << request.peerUserId
        << " url="
        << url.toString();

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, requestId);
    applyJsonRequestHeader(&networkRequest);
    applyAuthorizationHeader(&networkRequest, accessToken);

    const QByteArray requestBody =
        QJsonDocument(chatclient::dto::conversation::toJsonObject(request))
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
                        ? QStringLiteral("服务端返回了无法识别的会话响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::conversation::CreatePrivateConversationResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::conversation::
                            parseCreatePrivateConversationSuccessResponse(
                                document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("conversation.api")
                            << "create private conversation succeeded request_id="
                            << response.requestId
                            << " conversation_id="
                            << response.conversation.conversationId;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::conversation::ApiErrorDto error;
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
                        auto error =
                            chatclient::dto::conversation::parseApiErrorResponse(
                                document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::conversation::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

QString ConversationApiClient::createRequestId(const QString &action)
{
    return QStringLiteral("req_client_conversation_%1_%2")
        .arg(action, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void ConversationApiClient::applyRequestHeaders(QNetworkRequest *request,
                                                const QString &requestId)
{
    if (!request)
    {
        return;
    }

    request->setRawHeader("Accept", "application/json");
    request->setRawHeader("X-Request-Id", requestId.toUtf8());
}

void ConversationApiClient::applyJsonRequestHeader(QNetworkRequest *request)
{
    if (!request)
    {
        return;
    }

    request->setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
}

void ConversationApiClient::applyAuthorizationHeader(
    QNetworkRequest *request,
    const QString &accessToken)
{
    if (!request)
    {
        return;
    }

    const QString trimmedToken = accessToken.trimmed();
    if (trimmedToken.isEmpty())
    {
        return;
    }

    request->setRawHeader("Authorization",
                          QStringLiteral("Bearer %1")
                              .arg(trimmedToken)
                              .toUtf8());
}

}  // namespace chatclient::api
