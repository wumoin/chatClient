#include "api/conversation_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>
#include <QUrlQuery>

// ConversationApiClient 是会话域的纯 HTTP 客户端。
// 这里保持一个很重要的边界：它只负责把 Qt 网络请求和 JSON 响应
// 转换成 DTO，不负责缓存、模型更新或 WS 实时同步。
namespace chatclient::api {
namespace {

// 会话域接口的错误结构和成功结构比较统一，这个 helper 用来把
// “解析 + 日志 + 成功失败分发” 这套样板逻辑集中起来，避免每个接口重复实现。
template <typename ResponseDto, typename ParseSuccessFn, typename SuccessHandler,
          typename FailureHandler>
void handleConversationReply(QNetworkReply *reply,
                             const QString &requestId,
                             const QString &fallbackMessage,
                             ParseSuccessFn parseSuccess,
                             SuccessHandler onSuccess,
                             FailureHandler onFailure,
                             const char *successLogTag,
                             std::function<QString(const ResponseDto &)> successLogMessage)
{
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray responseBody = reply->readAll();
    const QString resolvedFallbackMessage =
        reply->error() == QNetworkReply::NoError ? fallbackMessage
                                                 : reply->errorString();

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(responseBody, &parseError);
    const bool hasJsonObject =
        parseError.error == QJsonParseError::NoError && document.isObject();

    if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
    {
        ResponseDto response;
        QString errorMessage;
        if (parseSuccess(document.object(), &response, &errorMessage))
        {
            if (response.requestId.isEmpty())
            {
                response.requestId = requestId;
            }

            CHATCLIENT_LOG_INFO(successLogTag) << successLogMessage(response);
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
            error.message =
                errorMessage.isEmpty() ? resolvedFallbackMessage : errorMessage;
            onFailure(error);
        }

        reply->deleteLater();
        return;
    }

    if (onFailure)
    {
        if (hasJsonObject)
        {
            auto error = chatclient::dto::conversation::parseApiErrorResponse(
                document.object(), httpStatus, resolvedFallbackMessage);
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
            error.message = resolvedFallbackMessage;
            onFailure(error);
        }
    }

    reply->deleteLater();
}

}  // namespace

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
        << "开始创建私聊会话，request_id="
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
                handleConversationReply<
                    chatclient::dto::conversation::
                        CreatePrivateConversationResponseDto>(
                    reply,
                    requestId,
                    QStringLiteral("服务端返回了无法识别的会话响应"),
                    chatclient::dto::conversation::
                        parseCreatePrivateConversationSuccessResponse,
                    std::move(onSuccess),
                    std::move(onFailure),
                    "conversation.api",
                    [](const auto &response) {
                        return QStringLiteral("创建私聊会话成功，request_id=%1 conversation_id=%2")
                            .arg(response.requestId,
                                 response.conversation.conversationId);
                    });
            });
}

void ConversationApiClient::fetchConversations(
    const QString &accessToken,
    ConversationListSuccessHandler onSuccess,
    ConversationListFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("list"));
    const QUrl url =
        chatclient::config::AppConfig::instance().conversationListUrl();

    CHATCLIENT_LOG_INFO("conversation.api")
        << "开始获取会话列表，request_id=" << requestId
        << " url=" << url.toString();

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
                handleConversationReply<
                    chatclient::dto::conversation::ConversationListResponseDto>(
                    reply,
                    requestId,
                    QStringLiteral("服务端返回了无法识别的会话列表响应"),
                    chatclient::dto::conversation::
                        parseConversationListSuccessResponse,
                    std::move(onSuccess),
                    std::move(onFailure),
                    "conversation.api",
                    [](const auto &response) {
                        return QStringLiteral("会话列表获取成功，request_id=%1 count=%2")
                            .arg(response.requestId)
                            .arg(response.conversations.size());
                    });
            });
}

void ConversationApiClient::fetchConversationDetail(
    const QString &accessToken,
    const QString &conversationId,
    ConversationDetailSuccessHandler onSuccess,
    ConversationDetailFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("detail"));
    const QUrl url = chatclient::config::AppConfig::instance()
                         .conversationDetailUrl(conversationId);

    CHATCLIENT_LOG_INFO("conversation.api")
        << "开始获取会话详情，request_id=" << requestId
        << " conversation_id=" << conversationId
        << " url=" << url.toString();

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
                handleConversationReply<
                    chatclient::dto::conversation::ConversationDetailResponseDto>(
                    reply,
                    requestId,
                    QStringLiteral("服务端返回了无法识别的会话详情响应"),
                    chatclient::dto::conversation::
                        parseConversationDetailSuccessResponse,
                    std::move(onSuccess),
                    std::move(onFailure),
                    "conversation.api",
                    [](const auto &response) {
                        return QStringLiteral("会话详情获取成功，request_id=%1 conversation_id=%2")
                            .arg(response.requestId,
                                 response.conversation.conversationId);
                    });
            });
}

void ConversationApiClient::fetchConversationMessages(
    const QString &accessToken,
    const QString &conversationId,
    const chatclient::dto::conversation::ListConversationMessagesRequestDto &request,
    ConversationMessagesSuccessHandler onSuccess,
    ConversationMessagesFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("messages"));
    // 历史消息接口是典型的“快照分页”入口：
    // ConversationManager 会决定 before_seq / after_seq 的用法以及如何写回本地 model。
    QUrl url = chatclient::config::AppConfig::instance().conversationMessagesUrl(
        conversationId);
    QUrlQuery query(url);
    query.addQueryItem(QStringLiteral("limit"),
                       QString::number(request.limit));
    if (request.hasBeforeSeq)
    {
        query.addQueryItem(QStringLiteral("before_seq"),
                           QString::number(request.beforeSeq));
    }
    if (request.hasAfterSeq)
    {
        query.addQueryItem(QStringLiteral("after_seq"),
                           QString::number(request.afterSeq));
    }
    url.setQuery(query);

    CHATCLIENT_LOG_INFO("conversation.api")
        << "开始获取会话历史消息，request_id=" << requestId
        << " conversation_id=" << conversationId
        << " url=" << url.toString();

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
                handleConversationReply<
                    chatclient::dto::conversation::
                        ConversationMessageListResponseDto>(
                    reply,
                    requestId,
                    QStringLiteral("服务端返回了无法识别的历史消息响应"),
                    chatclient::dto::conversation::
                        parseConversationMessagesSuccessResponse,
                    std::move(onSuccess),
                    std::move(onFailure),
                    "conversation.api",
                    [](const auto &response) {
                        return QStringLiteral("历史消息获取成功，request_id=%1 count=%2 has_more=%3")
                            .arg(response.requestId)
                            .arg(response.items.size())
                            .arg(response.hasMore);
                    });
            });
}

void ConversationApiClient::sendTextMessage(
    const QString &accessToken,
    const QString &conversationId,
    const chatclient::dto::conversation::SendTextMessageRequestDto &request,
    SendTextMessageSuccessHandler onSuccess,
    SendTextMessageFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("send_text"));
    const QUrl url = chatclient::config::AppConfig::instance()
                         .conversationSendTextUrl(conversationId);

    // 这里是纯 HTTP 发送链路：
    // 它只返回“服务端已接受并落库”的响应，不负责把消息再实时广播给其它在线端。
    // 当前项目里真正的实时发送主路径仍然是 ConversationManager -> WS。
    CHATCLIENT_LOG_INFO("conversation.api")
        << "开始发送文本消息，request_id=" << requestId
        << " conversation_id=" << conversationId
        << " url=" << url.toString();

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
                handleConversationReply<
                    chatclient::dto::conversation::SendTextMessageResponseDto>(
                    reply,
                    requestId,
                    QStringLiteral("服务端返回了无法识别的文本消息响应"),
                    chatclient::dto::conversation::
                        parseSendTextMessageSuccessResponse,
                    std::move(onSuccess),
                    std::move(onFailure),
                    "conversation.api",
                    [](const auto &response) {
                        return QStringLiteral("文本消息发送成功，request_id=%1 message_id=%2 seq=%3")
                            .arg(response.requestId, response.message.messageId)
                            .arg(response.message.seq);
                    });
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
