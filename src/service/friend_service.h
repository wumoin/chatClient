#pragma once

#include "api/friend_api_client.h"
#include "dto/friend_dto.h"

#include <QObject>

namespace chatclient::service {

class AuthService;

/**
 * @brief 客户端好友业务服务。
 *
 * 当前只先承接添加好友弹窗需要的最小链路：
 * 1) 用当前 access token 搜索指定账号；
 * 2) 拉取我发出的好友申请列表；
 * 3) 发送新的好友申请。
 */
class FriendService : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief 构造好友业务服务。
     * @param authService 当前认证服务，用于读取已登录会话。
     * @param parent 父级 QObject，可为空。
     */
    explicit FriendService(AuthService *authService, QObject *parent = nullptr);

    /**
     * @brief 搜索目标账号。
     * @param account 目标账号。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool searchUser(const QString &account, QString *errorMessage = nullptr);

    /**
     * @brief 拉取我发出的好友申请列表。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool fetchOutgoingRequests(QString *errorMessage = nullptr);

    /**
     * @brief 拉取我收到的好友申请列表。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool fetchIncomingRequests(QString *errorMessage = nullptr);

    /**
     * @brief 发送好友申请。
     * @param targetUserId 目标用户 ID。
     * @param requestMessage 申请附言。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool sendFriendRequest(const QString &targetUserId,
                           const QString &requestMessage,
                           QString *errorMessage = nullptr);

    /**
     * @brief 同意指定好友申请。
     * @param requestId 要处理的好友申请 ID。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool acceptFriendRequest(const QString &requestId,
                             QString *errorMessage = nullptr);

    /**
     * @brief 拒绝指定好友申请。
     * @param requestId 要处理的好友申请 ID。
     * @param errorMessage 本地校验失败时写入错误消息，可为空。
     * @return true 表示请求已发出；false 表示本地校验失败或已有请求在进行中。
     */
    bool rejectFriendRequest(const QString &requestId,
                             QString *errorMessage = nullptr);

    /**
     * @brief 判断当前是否正在搜索用户。
     * @return true 表示搜索请求仍在进行中；false 表示当前空闲。
     */
    bool isSearching() const;

    /**
     * @brief 判断当前是否正在拉取我发出的申请列表。
     * @return true 表示拉取仍在进行中；false 表示当前空闲。
     */
    bool isLoadingOutgoingRequests() const;

    /**
     * @brief 判断当前是否正在拉取我收到的申请列表。
     * @return true 表示拉取仍在进行中；false 表示当前空闲。
     */
    bool isLoadingIncomingRequests() const;

    /**
     * @brief 判断当前是否正在发送好友申请。
     * @return true 表示发送仍在进行中；false 表示当前空闲。
     */
    bool isSendingRequest() const;

    /**
     * @brief 判断当前是否正在处理好友申请。
     * @return true 表示同意/拒绝请求仍在进行中；false 表示当前空闲。
     */
    bool isHandlingRequest() const;

  signals:
    /**
     * @brief 搜索用户请求已开始提交。
     */
    void searchStarted();

    /**
     * @brief 搜索用户成功。
     * @param response 搜索结果 DTO。
     */
    void searchSucceeded(
        const chatclient::dto::friendship::SearchUserResponseDto &response);

    /**
     * @brief 搜索用户失败。
     * @param message 可直接展示给用户的错误提示。
     */
    void searchFailed(const QString &message);

    /**
     * @brief 已开始拉取我发出的申请列表。
     */
    void outgoingRequestsStarted();

    /**
     * @brief 我发出的好友申请列表拉取成功。
     * @param requests 当前列表项集合。
     */
    void outgoingRequestsSucceeded(
        const chatclient::dto::friendship::FriendRequestItems &requests);

    /**
     * @brief 我发出的好友申请列表拉取失败。
     * @param message 可直接展示给用户的错误提示。
     */
    void outgoingRequestsFailed(const QString &message);

    /**
     * @brief 已开始拉取我收到的申请列表。
     */
    void incomingRequestsStarted();

    /**
     * @brief 我收到的好友申请列表拉取成功。
     * @param requests 当前列表项集合。
     */
    void incomingRequestsSucceeded(
        const chatclient::dto::friendship::FriendRequestItems &requests);

    /**
     * @brief 我收到的好友申请列表拉取失败。
     * @param message 可直接展示给用户的错误提示。
     */
    void incomingRequestsFailed(const QString &message);

    /**
     * @brief 已开始发送好友申请。
     */
    void sendFriendRequestStarted();

    /**
     * @brief 发送好友申请成功。
     * @param request 服务端返回的好友申请记录。
     */
    void sendFriendRequestSucceeded(
        const chatclient::dto::friendship::FriendRequestItemDto &request);

    /**
     * @brief 发送好友申请失败。
     * @param message 可直接展示给用户的错误提示。
     */
    void sendFriendRequestFailed(const QString &message);

    /**
     * @brief 已开始处理收到的好友申请。
     */
    void handleFriendRequestStarted();

    /**
     * @brief 处理好友申请成功。
     * @param request 服务端返回的最新好友申请记录。
     */
    void handleFriendRequestSucceeded(
        const chatclient::dto::friendship::FriendRequestItemDto &request);

    /**
     * @brief 处理好友申请失败。
     * @param message 可直接展示给用户的错误提示。
     */
    void handleFriendRequestFailed(const QString &message);

  private:
    /**
     * @brief 读取当前可用 access token。
     * @param accessToken 成功时写入当前 access token。
     * @param currentUserId 成功时写入当前登录用户 ID，可为空。
     * @param errorMessage 失败时写入错误原因。
     * @return true 表示当前有可用登录态；false 表示未登录或 token 缺失。
     */
    bool resolveActiveSession(QString *accessToken,
                              QString *currentUserId,
                              QString *errorMessage) const;

    AuthService *m_authService = nullptr;
    chatclient::api::FriendApiClient m_friendApiClient;
    bool m_searching = false;
    bool m_loadingOutgoingRequests = false;
    bool m_loadingIncomingRequests = false;
    bool m_sendingRequest = false;
    bool m_handlingRequest = false;
};

}  // namespace chatclient::service
