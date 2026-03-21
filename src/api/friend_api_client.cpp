#include "api/friend_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

// FriendApiClient 处理好友域 HTTP 请求，例如搜索用户、发送申请、
// 查询申请列表和好友列表。上层 service 会在这里的 DTO 结果基础上
// 再做业务编排和界面更新。
//
// 一个边界要特别明确：
// - 这里返回的是 HTTP 快照或命令执行结果；
// - 它不负责把返回数据并入本地列表，也不负责和 WS 实时事件做对账。
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
        << "开始按账号搜索用户，request_id="
        << requestId
        << " url="
        << searchUrl.toString()
        << " account="
        << account;

    // 搜索接口只是“账号 -> 用户是否存在 / 基本资料”的只读查询，
    // 不会建立好友关系，也不会自动触发后续列表刷新。
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
                            << "用户搜索成功，request_id="
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
        << "开始获取已发送的好友申请，request_id="
        << requestId
        << " url="
        << url.toString();

    // “已发送申请”是一个快照列表接口。
    // 如果界面要保持最新状态，通常由 FriendService 在发送申请成功或收到实时事件后重新拉取。
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
                            << "已发送好友申请获取成功，request_id="
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
        << "开始获取好友列表，request_id="
        << requestId
        << " url="
        << url.toString();

    // 好友列表表示“当前已经成为好友的正式关系”，
    // 不包含待处理申请，也不试图在这里做增量合并。
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
                            << "好友列表获取成功，request_id="
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
        << "开始获取收到的好友申请，request_id="
        << requestId
        << " url="
        << url.toString();

    // “收到的申请”与“已发送的申请”共享同一套 DTO 结构，
    // 但语义不同：这一份更适合驱动审批 UI，而不是正式好友列表。
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
                            << "收到的好友申请获取成功，request_id="
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
        << "开始发送好友申请，request_id="
        << requestId
        << " url="
        << url.toString()
        << " target_user_id="
        << request.targetUserId;

    // 发送申请属于命令式写操作。
    // 成功时这里只返回本次新建的 friend_request 摘要，后续列表刷新交给上层决定。
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
                            << "好友申请发送成功，request_id="
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
        << "开始同意好友申请，request_id="
        << clientRequestId
        << " url="
        << url.toString()
        << " friend_request_id="
        << requestId;

    // 同意申请之后，服务端关系面会发生两件事：
    // 申请状态结束、双方好友关系建立。
    // 但本地好友列表不会在这里自动更新，仍需依赖上层刷新或实时事件。
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
                            << "同意好友申请成功，request_id="
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
        << "开始拒绝好友申请，request_id="
        << clientRequestId
        << " url="
        << url.toString()
        << " friend_request_id="
        << requestId;

    // 拒绝申请只会改变 friend_request 的状态，不会影响正式好友列表。
    // UI 若要立刻反映结果，也需要上层主动刷新对应快照。
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
                            << "拒绝好友申请成功，request_id="
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
    // 与其它 API client 保持一致，方便跨层串起一次好友操作的完整日志。
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
