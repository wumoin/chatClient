#include "api/friend_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

namespace chatclient::api {

FriendApiClient::FriendApiClient(QObject *parent)
    : QObject(parent),
      m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void FriendApiClient::searchUserByAccount(const QString &accessToken,
                                          const QString &account,
                                          SearchUserSuccessHandler onSuccess,
                                          SearchUserFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("friend_search"));
    const QUrl searchUrl =
        chatclient::config::AppConfig::instance().userSearchUrl(account);

    CHATCLIENT_LOG_INFO("friend.api")
        << "searching user by account request_id="
        << requestId
        << " url="
        << searchUrl.toString()
        << " account="
        << account;

    QNetworkRequest networkRequest(searchUrl);
    applyRequestHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->get(networkRequest);
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
                        ? QStringLiteral("服务端返回了无法识别的用户搜索响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::SearchUserResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseSearchUserSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "search user succeeded request_id="
                            << response.requestId
                            << " exists="
                            << response.exists
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
                        chatclient::dto::friendship::ApiErrorDto error;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void FriendApiClient::fetchOutgoingRequests(
    const QString &accessToken,
    FriendRequestListSuccessHandler onSuccess,
    FriendRequestListFailureHandler onFailure)
{
    const QString requestId =
        createRequestId(QStringLiteral("friend_outgoing"));
    const QUrl url =
        chatclient::config::AppConfig::instance().friendOutgoingRequestsUrl();

    CHATCLIENT_LOG_INFO("friend.api")
        << "fetching outgoing friend requests request_id="
        << requestId
        << " url="
        << url.toString();

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->get(networkRequest);
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
                        ? QStringLiteral("服务端返回了无法识别的好友申请列表响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::FriendRequestListResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseFriendRequestListSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "outgoing friend requests fetched request_id="
                            << response.requestId
                            << " count="
                            << response.requests.size();

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void FriendApiClient::fetchFriends(const QString &accessToken,
                                   FriendListSuccessHandler onSuccess,
                                   FriendListFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("friend_list"));
    const QUrl url = chatclient::config::AppConfig::instance().friendListUrl();

    CHATCLIENT_LOG_INFO("friend.api")
        << "fetching friends request_id="
        << requestId
        << " url="
        << url.toString();

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->get(networkRequest);
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
                        ? QStringLiteral("服务端返回了无法识别的好友列表响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::FriendListResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseFriendListSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "friends fetched request_id="
                            << response.requestId
                            << " count="
                            << response.friends.size();

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void FriendApiClient::fetchIncomingRequests(
    const QString &accessToken,
    FriendRequestListSuccessHandler onSuccess,
    FriendRequestListFailureHandler onFailure)
{
    const QString requestId =
        createRequestId(QStringLiteral("friend_incoming"));
    const QUrl url =
        chatclient::config::AppConfig::instance().friendIncomingRequestsUrl();

    CHATCLIENT_LOG_INFO("friend.api")
        << "fetching incoming friend requests request_id="
        << requestId
        << " url="
        << url.toString();

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->get(networkRequest);
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
                        ? QStringLiteral("服务端返回了无法识别的好友申请列表响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::FriendRequestListResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseFriendRequestListSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "incoming friend requests fetched request_id="
                            << response.requestId
                            << " count="
                            << response.requests.size();

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void FriendApiClient::sendFriendRequest(
    const QString &accessToken,
    const chatclient::dto::friendship::SendFriendRequestRequestDto &request,
    SendFriendRequestSuccessHandler onSuccess,
    SendFriendRequestFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("friend_send"));
    const QUrl url =
        chatclient::config::AppConfig::instance().friendSendRequestUrl();

    CHATCLIENT_LOG_INFO("friend.api")
        << "sending friend request request_id="
        << requestId
        << " url="
        << url.toString()
        << " target_user_id="
        << request.targetUserId;

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, requestId);
    applyJsonRequestHeader(&networkRequest);
    applyAuthorizationHeader(&networkRequest, accessToken);

    const QByteArray requestBody =
        QJsonDocument(chatclient::dto::friendship::toJsonObject(request))
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
                        ? QStringLiteral("服务端返回了无法识别的好友申请响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::SendFriendRequestResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseSendFriendRequestSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "friend request sent request_id="
                            << response.requestId
                            << " friend_request_id="
                            << response.request.requestId;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void FriendApiClient::acceptFriendRequest(
    const QString &accessToken,
    const QString &requestId,
    SendFriendRequestSuccessHandler onSuccess,
    SendFriendRequestFailureHandler onFailure)
{
    const QString clientRequestId =
        createRequestId(QStringLiteral("friend_accept"));
    const QUrl url =
        chatclient::config::AppConfig::instance().friendAcceptRequestUrl(
            requestId);

    CHATCLIENT_LOG_INFO("friend.api")
        << "accepting friend request request_id="
        << clientRequestId
        << " url="
        << url.toString()
        << " friend_request_id="
        << requestId;

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, clientRequestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->post(networkRequest, QByteArray());
    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply,
             clientRequestId,
             onSuccess = std::move(onSuccess),
             onFailure = std::move(onFailure)]() mutable {
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const QByteArray responseBody = reply->readAll();
                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("服务端返回了无法识别的好友申请处理响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::SendFriendRequestResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseSendFriendRequestSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = clientRequestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "accept friend request succeeded request_id="
                            << response.requestId
                            << " friend_request_id="
                            << response.request.requestId;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = clientRequestId;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = clientRequestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = clientRequestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void FriendApiClient::rejectFriendRequest(
    const QString &accessToken,
    const QString &requestId,
    SendFriendRequestSuccessHandler onSuccess,
    SendFriendRequestFailureHandler onFailure)
{
    const QString clientRequestId =
        createRequestId(QStringLiteral("friend_reject"));
    const QUrl url =
        chatclient::config::AppConfig::instance().friendRejectRequestUrl(
            requestId);

    CHATCLIENT_LOG_INFO("friend.api")
        << "rejecting friend request request_id="
        << clientRequestId
        << " url="
        << url.toString()
        << " friend_request_id="
        << requestId;

    QNetworkRequest networkRequest(url);
    applyRequestHeaders(&networkRequest, clientRequestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->post(networkRequest, QByteArray());
    connect(reply,
            &QNetworkReply::finished,
            this,
            [reply,
             clientRequestId,
             onSuccess = std::move(onSuccess),
             onFailure = std::move(onFailure)]() mutable {
                const int httpStatus =
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                        .toInt();
                const QByteArray responseBody = reply->readAll();
                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("服务端返回了无法识别的好友申请处理响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::friendship::SendFriendRequestResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::friendship::parseSendFriendRequestSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = clientRequestId;
                        }

                        CHATCLIENT_LOG_INFO("friend.api")
                            << "reject friend request succeeded request_id="
                            << response.requestId
                            << " friend_request_id="
                            << response.request.requestId;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = clientRequestId;
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
                        auto error = chatclient::dto::friendship::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = clientRequestId;
                        }
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::friendship::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = clientRequestId;
                        error.message = fallbackMessage;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

QString FriendApiClient::createRequestId(const QString &action)
{
    return QStringLiteral("req_client_%1_%2")
        .arg(action, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void FriendApiClient::applyRequestHeaders(QNetworkRequest *request,
                                          const QString &requestId)
{
    if (!request)
    {
        return;
    }

    request->setRawHeader("Accept", "application/json");
    request->setRawHeader("X-Request-Id", requestId.toUtf8());
}

void FriendApiClient::applyJsonRequestHeader(QNetworkRequest *request)
{
    if (!request)
    {
        return;
    }

    request->setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/json"));
}

void FriendApiClient::applyAuthorizationHeader(QNetworkRequest *request,
                                               const QString &accessToken)
{
    if (!request)
    {
        return;
    }

    request->setRawHeader(
        "Authorization",
        QStringLiteral("Bearer %1").arg(accessToken.trimmed()).toUtf8());
}

}  // namespace chatclient::api
