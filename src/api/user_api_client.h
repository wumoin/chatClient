#pragma once

#include "dto/user_dto.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkRequest;

namespace chatclient::api {

/**
 * @brief 用户资料相关 HTTP API 客户端。
 *
 * 当前只先承接和头像有关的两条最小链路：
 * 1) 上传注册阶段使用的临时头像；
 * 2) 拉取指定用户当前正式头像文件。
 */
class UserApiClient : public QObject
{
    Q_OBJECT

  public:
    using TemporaryAvatarUploadSuccessHandler = std::function<void(
        const chatclient::dto::user::TemporaryAvatarUploadResponseDto &)>;
    using TemporaryAvatarUploadFailureHandler =
        std::function<void(const chatclient::dto::user::ApiErrorDto &)>;
    using UserAvatarSuccessHandler = std::function<void(const QByteArray &)>;
    using UserAvatarFailureHandler =
        std::function<void(const chatclient::dto::user::ApiErrorDto &)>;

    /**
     * @brief 构造用户接口客户端。
     * @param parent 父级 QObject，可为空。
     */
    explicit UserApiClient(QObject *parent = nullptr);

    /**
     * @brief 上传注册阶段使用的临时头像文件。
     * @param localFilePath 本地头像文件绝对路径。
     * @param onSuccess 服务端返回成功响应时调用的回调。
     * @param onFailure 请求失败、网络错误或响应解析失败时调用的回调。
     */
    void uploadTemporaryAvatar(
        const QString &localFilePath,
        TemporaryAvatarUploadSuccessHandler onSuccess,
        TemporaryAvatarUploadFailureHandler onFailure);

    /**
     * @brief 拉取指定用户当前正式头像文件。
     * @param userId 目标用户 ID。
     * @param onSuccess 成功返回头像文件二进制时调用的回调。
     * @param onFailure 请求失败、网络错误或服务端返回错误响应时调用的回调。
     */
    void downloadUserAvatar(const QString &userId,
                            UserAvatarSuccessHandler onSuccess,
                            UserAvatarFailureHandler onFailure);

  private:
    /**
     * @brief 创建客户端本地 request_id。
     * @param action 当前请求动作，例如 `avatar_upload`。
     * @return 带业务前缀的请求追踪 ID。
     */
    static QString createRequestId(const QString &action);

    /**
     * @brief 为请求统一设置 request_id 和 Accept 请求头。
     * @param request 待发送的网络请求对象。
     * @param requestId 当前请求追踪 ID。
     */
    static void applyRequestHeaders(QNetworkRequest *request,
                                    const QString &requestId);

    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

}  // namespace chatclient::api
