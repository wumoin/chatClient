#pragma once

#include "dto/friend_dto.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkRequest;

namespace chatclient::api {

/**
 * @brief 好友域 HTTP API 客户端。
 *
 * 当前承接好友域当前已落地的最小 HTTP 能力：
 * 1) 按账号搜索用户是否存在；
 * 2) 查询我发出的好友申请；
 * 3) 查询我收到的好友申请；
 * 4) 查询当前正式好友列表；
 * 5) 向指定用户发送好友申请；
 * 6) 同意 / 拒绝好友申请。
 *
 * 这些接口都只负责“快照读取”或“一次命令提交”。
 * 是否刷新本地列表、是否等待实时事件补齐界面，由 FriendService 决定。
 */
class FriendApiClient : public QObject
{
    Q_OBJECT

  public:
    using SearchUserSuccessHandler =
        std::function<void(const chatclient::dto::friendship::SearchUserResponseDto &)>;
    using SearchUserFailureHandler =
        std::function<void(const chatclient::dto::friendship::ApiErrorDto &)>;
    using FriendRequestListSuccessHandler = std::function<void(
        const chatclient::dto::friendship::FriendRequestListResponseDto &)>;
    using FriendRequestListFailureHandler =
        std::function<void(const chatclient::dto::friendship::ApiErrorDto &)>;
    using FriendListSuccessHandler =
        std::function<void(const chatclient::dto::friendship::FriendListResponseDto &)>;
    using FriendListFailureHandler =
        std::function<void(const chatclient::dto::friendship::ApiErrorDto &)>;
    using SendFriendRequestSuccessHandler = std::function<void(
        const chatclient::dto::friendship::SendFriendRequestResponseDto &)>;
    using SendFriendRequestFailureHandler =
        std::function<void(const chatclient::dto::friendship::ApiErrorDto &)>;

    /**
     * @brief 构造好友接口客户端。
     * @param parent 父级 QObject，可为空。
     */
    explicit FriendApiClient(QObject *parent = nullptr);

    /**
     * @brief 按账号搜索目标用户。
     * @param accessToken 当前登录态 access token。
     * @param account 要搜索的账号。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void searchUserByAccount(const QString &accessToken,
                             const QString &account,
                             SearchUserSuccessHandler onSuccess,
                             SearchUserFailureHandler onFailure);

    /**
     * @brief 查询当前用户发出的好友申请列表。
     * @param accessToken 当前登录态 access token。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void fetchOutgoingRequests(const QString &accessToken,
                               FriendRequestListSuccessHandler onSuccess,
                               FriendRequestListFailureHandler onFailure);

    /**
     * @brief 查询当前用户的正式好友列表。
     * @param accessToken 当前登录态 access token。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void fetchFriends(const QString &accessToken,
                      FriendListSuccessHandler onSuccess,
                      FriendListFailureHandler onFailure);

    /**
     * @brief 查询当前用户收到的好友申请列表。
     * @param accessToken 当前登录态 access token。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void fetchIncomingRequests(const QString &accessToken,
                               FriendRequestListSuccessHandler onSuccess,
                               FriendRequestListFailureHandler onFailure);

    /**
     * @brief 发送好友申请。
     * @param accessToken 当前登录态 access token。
     * @param request 好友申请请求 DTO。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void sendFriendRequest(
        const QString &accessToken,
        const chatclient::dto::friendship::SendFriendRequestRequestDto &request,
        SendFriendRequestSuccessHandler onSuccess,
        SendFriendRequestFailureHandler onFailure);

    /**
     * @brief 同意指定好友申请。
     * @param accessToken 当前登录态 access token。
     * @param requestId 要同意的好友申请 ID。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void acceptFriendRequest(const QString &accessToken,
                             const QString &requestId,
                             SendFriendRequestSuccessHandler onSuccess,
                             SendFriendRequestFailureHandler onFailure);

    /**
     * @brief 拒绝指定好友申请。
     * @param accessToken 当前登录态 access token。
     * @param requestId 要拒绝的好友申请 ID。
     * @param onSuccess 成功回调。
     * @param onFailure 失败回调。
     */
    void rejectFriendRequest(const QString &accessToken,
                             const QString &requestId,
                             SendFriendRequestSuccessHandler onSuccess,
                             SendFriendRequestFailureHandler onFailure);

  private:
    /**
     * @brief 创建客户端本地 request_id。
     * @param action 当前请求动作。
     * @return 带业务前缀的请求追踪 ID。
     */
    static QString createRequestId(const QString &action);

    /**
     * @brief 为请求设置统一的 Accept / request_id 请求头。
     * @param request 待发送的网络请求对象。
     * @param requestId 当前请求追踪 ID。
     */
    static void applyRequestHeaders(QNetworkRequest *request,
                                    const QString &requestId);

    /**
     * @brief 为需要 JSON 请求体的请求额外设置 Content-Type。
     * @param request 待发送的网络请求对象。
     */
    static void applyJsonRequestHeader(QNetworkRequest *request);

    /**
     * @brief 为请求注入 Bearer Token。
     * @param request 待发送的网络请求对象。
     * @param accessToken 当前访问令牌。
     */
    static void applyAuthorizationHeader(QNetworkRequest *request,
                                         const QString &accessToken);

    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

}  // namespace chatclient::api
