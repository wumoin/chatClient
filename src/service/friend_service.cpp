#include "service/friend_service.h"

#include "log/app_logger.h"
#include "service/auth_service.h"
#include "service/friend_error_localizer.h"

namespace chatclient::service {
namespace {

constexpr int kMaxRequestMessageLength = 200;

}  // namespace

FriendService::FriendService(AuthService *authService, QObject *parent)
    : QObject(parent),
      m_authService(authService),
      m_friendApiClient(this)
{
    qRegisterMetaType<chatclient::dto::friendship::SearchUserResponseDto>(
        "chatclient::dto::friendship::SearchUserResponseDto");
    qRegisterMetaType<chatclient::dto::friendship::FriendRequestItemDto>(
        "chatclient::dto::friendship::FriendRequestItemDto");
    qRegisterMetaType<chatclient::dto::friendship::FriendRequestItems>(
        "chatclient::dto::friendship::FriendRequestItems");
    qRegisterMetaType<chatclient::dto::friendship::FriendListItemDto>(
        "chatclient::dto::friendship::FriendListItemDto");
    qRegisterMetaType<chatclient::dto::friendship::FriendListItems>(
        "chatclient::dto::friendship::FriendListItems");
}

bool FriendService::searchUser(const QString &account, QString *errorMessage)
{
    if (m_searching)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("搜索请求正在进行中，请稍候");
        }
        return false;
    }

    QString accessToken;
    if (!resolveActiveSession(&accessToken, nullptr, errorMessage))
    {
        return false;
    }

    const QString trimmedAccount = account.trimmed();
    if (trimmedAccount.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("请输入要搜索的账号");
        }
        return false;
    }

    m_searching = true;
    emit searchStarted();
    CHATCLIENT_LOG_INFO("friend.service")
        << "search user started account="
        << trimmedAccount;

    m_friendApiClient.searchUserByAccount(
        accessToken,
        trimmedAccount,
        [this](const chatclient::dto::friendship::SearchUserResponseDto &response) {
            m_searching = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "search user completed request_id="
                << response.requestId
                << " exists="
                << response.exists
                << " user_id="
                << response.user.userId;
            emit searchSucceeded(response);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_searching = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "search user failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit searchFailed(localizedMessage.isEmpty()
                                  ? QStringLiteral("搜索用户失败，请稍后重试")
                                  : localizedMessage);
        });

    return true;
}

bool FriendService::fetchOutgoingRequests(QString *errorMessage)
{
    if (m_loadingOutgoingRequests)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("申请记录正在刷新，请稍候");
        }
        return false;
    }

    QString accessToken;
    if (!resolveActiveSession(&accessToken, nullptr, errorMessage))
    {
        return false;
    }

    m_loadingOutgoingRequests = true;
    emit outgoingRequestsStarted();
    CHATCLIENT_LOG_INFO("friend.service")
        << "fetch outgoing requests started";

    m_friendApiClient.fetchOutgoingRequests(
        accessToken,
        [this](const chatclient::dto::friendship::FriendRequestListResponseDto &response) {
            m_loadingOutgoingRequests = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "fetch outgoing requests completed request_id="
                << response.requestId
                << " count="
                << response.requests.size();
            emit outgoingRequestsSucceeded(response.requests);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_loadingOutgoingRequests = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "fetch outgoing requests failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit outgoingRequestsFailed(localizedMessage.isEmpty()
                                            ? QStringLiteral("获取已发送申请失败，请稍后重试")
                                            : localizedMessage);
        });

    return true;
}

bool FriendService::fetchFriends(QString *errorMessage)
{
    if (m_loadingFriends)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("好友列表正在刷新，请稍候");
        }
        return false;
    }

    QString accessToken;
    if (!resolveActiveSession(&accessToken, nullptr, errorMessage))
    {
        return false;
    }

    m_loadingFriends = true;
    emit friendsStarted();
    CHATCLIENT_LOG_INFO("friend.service") << "fetch friends started";

    m_friendApiClient.fetchFriends(
        accessToken,
        [this](const chatclient::dto::friendship::FriendListResponseDto &response) {
            m_loadingFriends = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "fetch friends completed request_id="
                << response.requestId
                << " count="
                << response.friends.size();
            emit friendsSucceeded(response.friends);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_loadingFriends = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "fetch friends failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit friendsFailed(localizedMessage.isEmpty()
                                   ? QStringLiteral("获取好友列表失败，请稍后重试")
                                   : localizedMessage);
        });

    return true;
}

bool FriendService::fetchIncomingRequests(QString *errorMessage)
{
    if (m_loadingIncomingRequests)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("新的朋友列表正在刷新，请稍候");
        }
        return false;
    }

    QString accessToken;
    if (!resolveActiveSession(&accessToken, nullptr, errorMessage))
    {
        return false;
    }

    m_loadingIncomingRequests = true;
    emit incomingRequestsStarted();
    CHATCLIENT_LOG_INFO("friend.service")
        << "fetch incoming requests started";

    m_friendApiClient.fetchIncomingRequests(
        accessToken,
        [this](const chatclient::dto::friendship::FriendRequestListResponseDto &response) {
            m_loadingIncomingRequests = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "fetch incoming requests completed request_id="
                << response.requestId
                << " count="
                << response.requests.size();
            emit incomingRequestsSucceeded(response.requests);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_loadingIncomingRequests = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "fetch incoming requests failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit incomingRequestsFailed(localizedMessage.isEmpty()
                                            ? QStringLiteral("获取新的朋友失败，请稍后重试")
                                            : localizedMessage);
        });

    return true;
}

bool FriendService::sendFriendRequest(const QString &targetUserId,
                                      const QString &requestMessage,
                                      QString *errorMessage)
{
    if (m_sendingRequest)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("好友申请正在发送，请稍候");
        }
        return false;
    }

    QString accessToken;
    QString currentUserId;
    if (!resolveActiveSession(&accessToken, &currentUserId, errorMessage))
    {
        return false;
    }

    const QString trimmedTargetUserId = targetUserId.trimmed();
    const QString trimmedMessage = requestMessage.trimmed();
    if (trimmedTargetUserId.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("缺少目标用户信息，请重新搜索");
        }
        return false;
    }

    if (trimmedTargetUserId == currentUserId)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("不能添加自己为好友");
        }
        return false;
    }

    if (trimmedMessage.size() > kMaxRequestMessageLength)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("申请附言不能超过 200 个字符");
        }
        return false;
    }

    chatclient::dto::friendship::SendFriendRequestRequestDto request;
    request.targetUserId = trimmedTargetUserId;
    request.requestMessage = trimmedMessage;

    m_sendingRequest = true;
    emit sendFriendRequestStarted();
    CHATCLIENT_LOG_INFO("friend.service")
        << "send friend request started target_user_id="
        << request.targetUserId;

    m_friendApiClient.sendFriendRequest(
        accessToken,
        request,
        [this](const chatclient::dto::friendship::SendFriendRequestResponseDto &response) {
            m_sendingRequest = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "send friend request completed request_id="
                << response.requestId
                << " friend_request_id="
                << response.request.requestId;
            emit sendFriendRequestSucceeded(response.request);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_sendingRequest = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "send friend request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit sendFriendRequestFailed(localizedMessage.isEmpty()
                                             ? QStringLiteral("发送好友申请失败，请稍后重试")
                                             : localizedMessage);
        });

    return true;
}

bool FriendService::acceptFriendRequest(const QString &requestId,
                                        QString *errorMessage)
{
    if (m_handlingRequest)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("好友申请正在处理中，请稍候");
        }
        return false;
    }

    QString accessToken;
    if (!resolveActiveSession(&accessToken, nullptr, errorMessage))
    {
        return false;
    }

    const QString trimmedRequestId = requestId.trimmed();
    if (trimmedRequestId.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("缺少申请记录信息，请刷新后重试");
        }
        return false;
    }

    m_handlingRequest = true;
    emit handleFriendRequestStarted();
    CHATCLIENT_LOG_INFO("friend.service")
        << "accept friend request started request_id="
        << trimmedRequestId;

    m_friendApiClient.acceptFriendRequest(
        accessToken,
        trimmedRequestId,
        [this](const chatclient::dto::friendship::SendFriendRequestResponseDto &response) {
            m_handlingRequest = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "accept friend request completed request_id="
                << response.requestId
                << " friend_request_id="
                << response.request.requestId;
            emit handleFriendRequestSucceeded(response.request);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_handlingRequest = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "accept friend request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit handleFriendRequestFailed(localizedMessage.isEmpty()
                                               ? QStringLiteral("同意好友申请失败，请稍后重试")
                                               : localizedMessage);
        });

    return true;
}

bool FriendService::rejectFriendRequest(const QString &requestId,
                                        QString *errorMessage)
{
    if (m_handlingRequest)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("好友申请正在处理中，请稍候");
        }
        return false;
    }

    QString accessToken;
    if (!resolveActiveSession(&accessToken, nullptr, errorMessage))
    {
        return false;
    }

    const QString trimmedRequestId = requestId.trimmed();
    if (trimmedRequestId.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("缺少申请记录信息，请刷新后重试");
        }
        return false;
    }

    m_handlingRequest = true;
    emit handleFriendRequestStarted();
    CHATCLIENT_LOG_INFO("friend.service")
        << "reject friend request started request_id="
        << trimmedRequestId;

    m_friendApiClient.rejectFriendRequest(
        accessToken,
        trimmedRequestId,
        [this](const chatclient::dto::friendship::SendFriendRequestResponseDto &response) {
            m_handlingRequest = false;
            CHATCLIENT_LOG_INFO("friend.service")
                << "reject friend request completed request_id="
                << response.requestId
                << " friend_request_id="
                << response.request.requestId;
            emit handleFriendRequestSucceeded(response.request);
        },
        [this](const chatclient::dto::friendship::ApiErrorDto &error) {
            m_handlingRequest = false;
            CHATCLIENT_LOG_WARN("friend.service")
                << "reject friend request failed request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            const QString localizedMessage = localizeFriendError(error);
            emit handleFriendRequestFailed(localizedMessage.isEmpty()
                                               ? QStringLiteral("拒绝好友申请失败，请稍后重试")
                                               : localizedMessage);
        });

    return true;
}

bool FriendService::isSearching() const
{
    return m_searching;
}

bool FriendService::isLoadingOutgoingRequests() const
{
    return m_loadingOutgoingRequests;
}

bool FriendService::isLoadingFriends() const
{
    return m_loadingFriends;
}

bool FriendService::isLoadingIncomingRequests() const
{
    return m_loadingIncomingRequests;
}

bool FriendService::isSendingRequest() const
{
    return m_sendingRequest;
}

bool FriendService::isHandlingRequest() const
{
    return m_handlingRequest;
}

bool FriendService::resolveActiveSession(QString *accessToken,
                                         QString *currentUserId,
                                         QString *errorMessage) const
{
    if (!m_authService || !m_authService->hasActiveSession())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("当前尚未登录，请重新登录后再试");
        }
        return false;
    }

    const auto &session = m_authService->currentSession();
    const QString token = session.accessToken.trimmed();
    if (token.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("当前登录状态无效，请重新登录后再试");
        }
        return false;
    }

    if (accessToken)
    {
        *accessToken = token;
    }
    if (currentUserId)
    {
        *currentUserId = session.user.userId.trimmed();
    }
    return true;
}

}  // namespace chatclient::service
