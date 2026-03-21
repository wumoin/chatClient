#include "api/file_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QImageReader>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUuid>

// FileApiClient 负责聊天附件的 HTTP 传输细节：
// - multipart 临时文件上传
// - 二进制文件下载
// - JSON 成功/失败响应解析
//
// 它不处理附件和消息的关联，也不决定下载后的落盘策略。
namespace chatclient::api {
namespace {

// 客户端上传前的本地前置校验也要和服务端保持同一条 1 GB 口径。
// 这样用户在点“发送图片”后，可以立刻得到明确反馈，而不是等网络传输一段时间后
// 才从服务端收到“文件过大”的失败响应。
constexpr qint64 kAttachmentMaxBytes = 1024LL * 1024LL * 1024LL;

QString fileNameFromContentDisposition(const QByteArray &headerValue)
{
    // 下载成功时服务端可能带 Content-Disposition。
    // 这里尽量提取 filename，方便调用方直接保存文件或给“另存为”默认名。
    const QString contentDisposition = QString::fromUtf8(headerValue).trimmed();
    if (contentDisposition.isEmpty())
    {
        return QString();
    }

    const QRegularExpression quotedPattern(
        QStringLiteral("filename\\s*=\\s*\"([^\"]+)\""),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch quotedMatch =
        quotedPattern.match(contentDisposition);
    if (quotedMatch.hasMatch())
    {
        return quotedMatch.captured(1).trimmed();
    }

    const QRegularExpression plainPattern(
        QStringLiteral("filename\\s*=\\s*([^;]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch plainMatch =
        plainPattern.match(contentDisposition);
    if (plainMatch.hasMatch())
    {
        return plainMatch.captured(1).trimmed();
    }

    return QString();
}

}  // namespace

FileApiClient::FileApiClient(QObject *parent)
    : QObject(parent),
      m_networkAccessManager(new QNetworkAccessManager(this))
{
}

QString FileApiClient::uploadAttachment(
    const QString &accessToken,
    const QString &localFilePath,
    UploadAttachmentSuccessHandler onSuccess,
    UploadAttachmentFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("file_upload"));
    const QUrl uploadUrl =
        chatclient::config::AppConfig::instance().fileUploadUrl();

    CHATCLIENT_LOG_INFO("file.api")
        << "开始上传临时聊天附件，request_id="
        << requestId
        << " url="
        << uploadUrl.toString()
        << " local_file="
        << localFilePath;

    auto *file = new QFile(localFilePath);
    if (!file->open(QIODevice::ReadOnly))
    {
        chatclient::dto::file::ApiErrorDto error;
        error.requestId = requestId;
        error.message = QStringLiteral("无法读取选中的附件文件");

        CHATCLIENT_LOG_WARN("file.api")
            << "打开本地附件文件失败，request_id="
            << requestId
            << " local_file="
            << localFilePath;
        if (onFailure)
        {
            onFailure(error);
        }
        file->deleteLater();
        return requestId;
    }

    const qint64 fileSizeBytes = file->size();
    if (fileSizeBytes > kAttachmentMaxBytes)
    {
        chatclient::dto::file::ApiErrorDto error;
        error.httpStatus = 400;
        error.errorCode = 40001;
        error.requestId = requestId;
        error.message = QStringLiteral("附件大小不能超过 1 GB");

        CHATCLIENT_LOG_WARN("file.api")
            << "本地附件超过上传上限，request_id="
            << requestId
            << " local_file="
            << localFilePath
            << " size_bytes="
            << fileSizeBytes;
        if (onFailure)
        {
            onFailure(error);
        }
        file->deleteLater();
        return requestId;
    }

    const QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(localFilePath);

    // 和服务端 FileController 的约定保持一致：
    // multipart 中优先使用字段名 file。
    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    const QString fileName = QFileInfo(localFilePath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral(
                                    "form-data; name=\"file\"; filename=\"%1\"")
                                    .arg(fileName)));
    if (mimeType.isValid() && !mimeType.name().isEmpty())
    {
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType.name());
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    // 如果当前附件是图片，就顺手把本地解析到的宽高作为可选表单字段带给服务端。
    // 这样后续 message.send_image 确认正式附件时，服务端就不必再次自行探测尺寸。
    if (mimeType.isValid() && mimeType.name().startsWith(QStringLiteral("image/")))
    {
        QImageReader imageReader(localFilePath);
        const QSize imageSize = imageReader.size();
        if (imageSize.isValid())
        {
            QHttpPart widthPart;
            widthPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                QVariant(QStringLiteral(
                                    "form-data; name=\"image_width\"")));
            widthPart.setBody(QByteArray::number(imageSize.width()));
            multiPart->append(widthPart);

            QHttpPart heightPart;
            heightPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                                 QVariant(QStringLiteral(
                                     "form-data; name=\"image_height\"")));
            heightPart.setBody(QByteArray::number(imageSize.height()));
            multiPart->append(heightPart);
        }
    }

    QNetworkRequest networkRequest(uploadUrl);
    applyRequestHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply =
        m_networkAccessManager->post(networkRequest, multiPart);
    multiPart->setParent(reply);

    connect(reply,
            &QNetworkReply::uploadProgress,
            this,
            [this, requestId](qint64 bytesSent, qint64 bytesTotal) {
                // 直接桥接底层 reply 的上传进度；
                // 调用方只需要监听 FileApiClient，不必接触 QNetworkReply 指针。
                emit uploadProgressChanged(requestId, bytesSent, bytesTotal);
            });

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
                        ? QStringLiteral("服务端返回了无法识别的附件上传响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::file::UploadAttachmentResponseDto response;
                    QString errorMessage;
                    if (chatclient::dto::file::parseUploadAttachmentSuccessResponse(
                            document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("file.api")
                            << "临时聊天附件上传成功，request_id="
                            << response.requestId
                            << " http_status="
                            << httpStatus
                            << " upload_key="
                            << response.upload.attachmentUploadKey;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::file::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = requestId;
                        error.message = errorMessage.isEmpty()
                                            ? fallbackMessage
                                            : errorMessage;

                        CHATCLIENT_LOG_ERROR("file.api")
                            << "解析附件上传响应失败，request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " error="
                            << error.message;
                        onFailure(error);
                    }

                    reply->deleteLater();
                    return;
                }

                if (onFailure)
                {
                    if (hasJsonObject)
                    {
                        auto error = chatclient::dto::file::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }

                        CHATCLIENT_LOG_WARN("file.api")
                            << "临时聊天附件上传失败，request_id="
                            << error.requestId
                            << " http_status="
                            << httpStatus
                            << " error_code="
                            << error.errorCode
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                    else
                    {
                        chatclient::dto::file::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;

                        CHATCLIENT_LOG_WARN("file.api")
                            << "临时聊天附件上传返回了非 JSON 响应，request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " error="
                            << error.message;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
    return requestId;
}

QString FileApiClient::downloadAttachment(
    const QString &accessToken,
    const QString &attachmentId,
    DownloadAttachmentSuccessHandler onSuccess,
    DownloadAttachmentFailureHandler onFailure)
{
    // 按 attachment_id 下载时，完整 URL 由 AppConfig 统一拼接，
    // 调用方不需要自己硬编码 /api/v1/files/{attachment_id}。
    const QUrl downloadUrl =
        chatclient::config::AppConfig::instance().fileDownloadUrl(attachmentId);
    return downloadAttachmentByUrl(accessToken,
                                   downloadUrl.toString(),
                                   std::move(onSuccess),
                                   std::move(onFailure));
}

QString FileApiClient::downloadAttachmentByUrl(
    const QString &accessToken,
    const QString &downloadUrl,
    DownloadAttachmentSuccessHandler onSuccess,
    DownloadAttachmentFailureHandler onFailure)
{
    const QString requestId =
        createRequestId(QStringLiteral("file_download"));
    // 正式附件引用里的 download_url 当前通常是相对路径，
    // 这里统一兼容“相对路径”与“完整绝对 URL”两种输入形式。
    const QUrl resolvedUrl = resolveDownloadUrl(downloadUrl);

    CHATCLIENT_LOG_INFO("file.api")
        << "开始下载聊天附件，request_id="
        << requestId
        << " url="
        << resolvedUrl.toString();

    QNetworkRequest networkRequest(resolvedUrl);
    applyRequestHeaders(&networkRequest, requestId);
    applyAuthorizationHeader(&networkRequest, accessToken);

    QNetworkReply *reply = m_networkAccessManager->get(networkRequest);
    connect(reply,
            &QNetworkReply::downloadProgress,
            this,
            [this, requestId](qint64 bytesReceived, qint64 bytesTotal) {
                // 下载进度同样统一从 FileApiClient 往外抛，
                // 调用方无需自己管理每个 reply 的生命周期。
                emit downloadProgressChanged(requestId,
                                             bytesReceived,
                                             bytesTotal);
            });
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

                if (httpStatus >= 200 && httpStatus < 300 &&
                    reply->error() == QNetworkReply::NoError)
                {
                    chatclient::dto::file::DownloadedAttachmentDto result;
                    result.content = responseBody;
                    // 附件下载成功时没有 JSON 包装，mimeType / fileName 需要从响应头里恢复。
                    result.mimeType =
                        reply->header(QNetworkRequest::ContentTypeHeader)
                            .toString()
                            .trimmed();
                    result.fileName = fileNameFromContentDisposition(
                        reply->rawHeader("Content-Disposition"));

                    CHATCLIENT_LOG_INFO("file.api")
                        << "聊天附件下载成功，request_id="
                        << requestId
                        << " http_status="
                        << httpStatus
                        << " bytes="
                        << result.content.size()
                        << " mime_type="
                        << result.mimeType;
                    if (onSuccess)
                    {
                        onSuccess(result);
                    }

                    reply->deleteLater();
                    return;
                }

                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("附件文件响应不可用")
                        : reply->errorString();

                chatclient::dto::file::ApiErrorDto error;
                error.httpStatus = httpStatus;
                error.requestId = requestId;
                error.message = fallbackMessage;

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                if (parseError.error == QJsonParseError::NoError &&
                    document.isObject())
                {
                    error = chatclient::dto::file::parseApiErrorResponse(
                        document.object(), httpStatus, fallbackMessage);
                    if (error.requestId.isEmpty())
                    {
                        error.requestId = requestId;
                    }
                }

                CHATCLIENT_LOG_WARN("file.api")
                    << "聊天附件下载失败，request_id="
                    << error.requestId
                    << " http_status="
                    << error.httpStatus
                    << " error_code="
                    << error.errorCode
                    << " message="
                    << error.message;
                if (onFailure)
                {
                    onFailure(error);
                }

                reply->deleteLater();
            });
    return requestId;
}

QString FileApiClient::createRequestId(const QString &action)
{
    return QStringLiteral("req_client_%1_%2")
        .arg(action, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void FileApiClient::applyRequestHeaders(QNetworkRequest *request,
                                        const QString &requestId)
{
    if (!request)
    {
        return;
    }

    request->setRawHeader("Accept", "*/*");
    // 下载可能返回任意二进制类型，因此这里不强制 Accept=application/json。
    request->setRawHeader("X-Request-Id", requestId.toUtf8());
}

void FileApiClient::applyAuthorizationHeader(QNetworkRequest *request,
                                             const QString &accessToken)
{
    if (!request || accessToken.trimmed().isEmpty())
    {
        return;
    }

    request->setRawHeader(
        "Authorization",
        QStringLiteral("Bearer %1").arg(accessToken.trimmed()).toUtf8());
}

QUrl FileApiClient::resolveDownloadUrl(const QString &downloadUrl)
{
    const QUrl rawUrl(downloadUrl);
    if (rawUrl.isValid() && !rawUrl.isRelative())
    {
        return rawUrl;
    }

    // 相对 download_url 统一按当前 HTTP base URL 解析，避免调用方重复拼接主机地址。
    return chatclient::config::AppConfig::instance().httpBaseUrl().resolved(
        QUrl(downloadUrl));
}

}  // namespace chatclient::api
