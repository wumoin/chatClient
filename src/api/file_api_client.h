#pragma once

#include "dto/file_dto.h"

#include <QObject>

#include <functional>

class QNetworkAccessManager;
class QNetworkRequest;
class QUrl;

namespace chatclient::api {

/**
 * @brief 聊天附件 HTTP API 客户端。
 *
 * 当前承接两条附件链路：
 * 1) 上传聊天附件到服务端临时区；
 * 2) 下载指定 attachment_id 或 download_url 对应的文件流。
 *
 * 它只负责 multipart / 二进制传输和统一响应解析，
 * 不把临时上传自动确认成正式消息，也不负责和会话消息做关联。
 */
class FileApiClient : public QObject
{
    Q_OBJECT

  public:
    using UploadAttachmentSuccessHandler =
        std::function<void(const chatclient::dto::file::UploadAttachmentResponseDto &)>;
    using UploadAttachmentFailureHandler =
        std::function<void(const chatclient::dto::file::ApiErrorDto &)>;
    using DownloadAttachmentSuccessHandler =
        std::function<void(const chatclient::dto::file::DownloadedAttachmentDto &)>;
    using DownloadAttachmentFailureHandler =
        std::function<void(const chatclient::dto::file::ApiErrorDto &)>;

    /**
     * @brief 构造附件接口客户端。
     * @param parent 父级 QObject，可为空。
     */
    explicit FileApiClient(QObject *parent = nullptr);

    /**
     * @brief 上传一个本地附件文件到服务端临时区。
     * @param accessToken 当前登录态 access token。
     * @param localFilePath 本地文件绝对路径。
     * @param onSuccess 服务端返回临时 upload key 时调用的回调。
     * @param onFailure 请求失败、网络错误或响应解析失败时调用的回调。
     * @return 本次上传请求的 request_id，可用于关联进度信号与最终结果。
     */
    QString uploadAttachment(const QString &accessToken,
                             const QString &localFilePath,
                             UploadAttachmentSuccessHandler onSuccess,
                             UploadAttachmentFailureHandler onFailure);

    /**
     * @brief 按附件 ID 下载附件文件。
     * @param accessToken 当前登录态 access token。
     * @param attachmentId 目标附件 ID。
     * @param onSuccess 成功返回附件二进制时调用的回调。
     * @param onFailure 请求失败、网络错误或服务端返回错误响应时调用的回调。
     * @return 本次下载请求的 request_id，可用于关联进度信号与最终结果。
     */
    QString downloadAttachment(const QString &accessToken,
                               const QString &attachmentId,
                               DownloadAttachmentSuccessHandler onSuccess,
                               DownloadAttachmentFailureHandler onFailure);

    /**
     * @brief 按服务端返回的 download_url 下载附件文件。
     * @param accessToken 当前登录态 access token。
     * @param downloadUrl 服务端返回的相对或绝对下载地址。
     * @param onSuccess 成功返回附件二进制时调用的回调。
     * @param onFailure 请求失败、网络错误或服务端返回错误响应时调用的回调。
     * @return 本次下载请求的 request_id，可用于关联进度信号与最终结果。
     */
    QString downloadAttachmentByUrl(const QString &accessToken,
                                    const QString &downloadUrl,
                                    DownloadAttachmentSuccessHandler onSuccess,
                                    DownloadAttachmentFailureHandler onFailure);

  signals:
    /**
     * @brief 附件上传进度发生变化。
     * @param requestId 本次上传请求的 request_id。
     * @param bytesSent 当前已上传字节数。
     * @param bytesTotal 当前总字节数；若底层暂时未知，可能小于等于 0。
     */
    void uploadProgressChanged(const QString &requestId,
                               qint64 bytesSent,
                               qint64 bytesTotal);

    /**
     * @brief 附件下载进度发生变化。
     * @param requestId 本次下载请求的 request_id。
     * @param bytesReceived 当前已下载字节数。
     * @param bytesTotal 当前总字节数；若服务端未提供 Content-Length，可能小于等于 0。
     */
    void downloadProgressChanged(const QString &requestId,
                                 qint64 bytesReceived,
                                 qint64 bytesTotal);

  private:
    /**
     * @brief 创建客户端本地 request_id。
     * @param action 当前请求动作。
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

    /**
     * @brief 为需要认证的请求注入 Bearer Token。
     * @param request 待发送的网络请求对象。
     * @param accessToken 当前访问令牌。
     */
    static void applyAuthorizationHeader(QNetworkRequest *request,
                                         const QString &accessToken);

    /**
     * @brief 将服务端返回的相对 download_url 解析成完整 URL。
     * @param downloadUrl 相对或绝对下载地址。
     * @return 可直接发起请求的完整 URL。
     */
    static QUrl resolveDownloadUrl(const QString &downloadUrl);

    // 统一承接附件上传/下载的 Qt 网络请求。
    QNetworkAccessManager *m_networkAccessManager = nullptr;
};

}  // namespace chatclient::api
