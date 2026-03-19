#include "api/user_api_client.h"

#include "config/appconfig.h"
#include "log/app_logger.h"

#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUuid>

namespace chatclient::api {

UserApiClient::UserApiClient(QObject *parent)
    : QObject(parent),
      m_networkAccessManager(new QNetworkAccessManager(this))
{
}

void UserApiClient::uploadTemporaryAvatar(
    const QString &localFilePath,
    TemporaryAvatarUploadSuccessHandler onSuccess,
    TemporaryAvatarUploadFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("avatar_upload"));
    const QUrl uploadUrl =
        chatclient::config::AppConfig::instance().avatarTempUploadUrl();

    CHATCLIENT_LOG_INFO("user.api")
        << "uploading temporary avatar request_id="
        << requestId
        << " url="
        << uploadUrl.toString()
        << " local_file="
        << localFilePath;

    auto *file = new QFile(localFilePath);
    if (!file->open(QIODevice::ReadOnly))
    {
        chatclient::dto::user::ApiErrorDto error;
        error.requestId = requestId;
        error.message = QStringLiteral("无法读取选中的头像文件");

        CHATCLIENT_LOG_WARN("user.api")
            << "failed to open local avatar file request_id="
            << requestId
            << " local_file="
            << localFilePath;
        if (onFailure)
        {
            onFailure(error);
        }
        file->deleteLater();
        return;
    }

    const QMimeDatabase mimeDatabase;
    const QMimeType mimeType = mimeDatabase.mimeTypeForFile(localFilePath);

    auto *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    QHttpPart filePart;
    const QString fileName = QFileInfo(localFilePath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral(
                                    "form-data; name=\"avatar\"; filename=\"%1\"")
                                    .arg(fileName)));
    if (mimeType.isValid() && !mimeType.name().isEmpty())
    {
        filePart.setHeader(QNetworkRequest::ContentTypeHeader, mimeType.name());
    }
    filePart.setBodyDevice(file);
    file->setParent(multiPart);
    multiPart->append(filePart);

    QNetworkRequest networkRequest(uploadUrl);
    applyRequestHeaders(&networkRequest, requestId);

    QNetworkReply *reply =
        m_networkAccessManager->post(networkRequest, multiPart);
    multiPart->setParent(reply);

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
                        ? QStringLiteral("服务端返回了无法识别的头像上传响应")
                        : reply->errorString();

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                const bool hasJsonObject =
                    parseError.error == QJsonParseError::NoError &&
                    document.isObject();

                if (httpStatus >= 200 && httpStatus < 300 && hasJsonObject)
                {
                    chatclient::dto::user::TemporaryAvatarUploadResponseDto
                        response;
                    QString errorMessage;
                    if (chatclient::dto::user::
                            parseTemporaryAvatarUploadSuccessResponse(
                                document.object(), &response, &errorMessage))
                    {
                        if (response.requestId.isEmpty())
                        {
                            response.requestId = requestId;
                        }

                        CHATCLIENT_LOG_INFO("user.api")
                            << "temporary avatar upload succeeded request_id="
                            << response.requestId
                            << " http_status="
                            << httpStatus
                            << " avatar_upload_key="
                            << response.avatarUploadKey;

                        if (onSuccess)
                        {
                            onSuccess(response);
                        }

                        reply->deleteLater();
                        return;
                    }

                    if (onFailure)
                    {
                        chatclient::dto::user::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.errorCode = 50000;
                        error.requestId = requestId;
                        error.message = errorMessage.isEmpty()
                                            ? fallbackMessage
                                            : errorMessage;

                        CHATCLIENT_LOG_ERROR("user.api")
                            << "temporary avatar upload parse failed request_id="
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
                        auto error = chatclient::dto::user::parseApiErrorResponse(
                            document.object(), httpStatus, fallbackMessage);
                        if (error.requestId.isEmpty())
                        {
                            error.requestId = requestId;
                        }

                        CHATCLIENT_LOG_WARN("user.api")
                            << "temporary avatar upload failed request_id="
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
                        chatclient::dto::user::ApiErrorDto error;
                        error.httpStatus = httpStatus;
                        error.requestId = requestId;
                        error.message = fallbackMessage;

                        CHATCLIENT_LOG_WARN("user.api")
                            << "temporary avatar upload returned non-json response request_id="
                            << requestId
                            << " http_status="
                            << httpStatus
                            << " message="
                            << error.message;
                        onFailure(error);
                    }
                }

                reply->deleteLater();
            });
}

void UserApiClient::downloadUserAvatar(const QString &userId,
                                       UserAvatarSuccessHandler onSuccess,
                                       UserAvatarFailureHandler onFailure)
{
    const QString requestId = createRequestId(QStringLiteral("avatar_download"));
    const QUrl avatarUrl =
        chatclient::config::AppConfig::instance().userAvatarUrl(userId);

    CHATCLIENT_LOG_INFO("user.api")
        << "downloading user avatar request_id="
        << requestId
        << " url="
        << avatarUrl.toString()
        << " user_id="
        << userId;

    QNetworkRequest networkRequest(avatarUrl);
    applyRequestHeaders(&networkRequest, requestId);

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

                if (httpStatus >= 200 && httpStatus < 300 &&
                    reply->error() == QNetworkReply::NoError)
                {
                    CHATCLIENT_LOG_INFO("user.api")
                        << "user avatar download succeeded request_id="
                        << requestId
                        << " http_status="
                        << httpStatus
                        << " bytes="
                        << responseBody.size();
                    if (onSuccess)
                    {
                        onSuccess(responseBody);
                    }

                    reply->deleteLater();
                    return;
                }

                const QString fallbackMessage =
                    reply->error() == QNetworkReply::NoError
                        ? QStringLiteral("头像文件响应不可用")
                        : reply->errorString();

                chatclient::dto::user::ApiErrorDto error;
                error.httpStatus = httpStatus;
                error.requestId = requestId;
                error.message = fallbackMessage;

                QJsonParseError parseError;
                const QJsonDocument document =
                    QJsonDocument::fromJson(responseBody, &parseError);
                if (parseError.error == QJsonParseError::NoError &&
                    document.isObject())
                {
                    error = chatclient::dto::user::parseApiErrorResponse(
                        document.object(), httpStatus, fallbackMessage);
                    if (error.requestId.isEmpty())
                    {
                        error.requestId = requestId;
                    }
                }

                CHATCLIENT_LOG_WARN("user.api")
                    << "user avatar download failed request_id="
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
}

QString UserApiClient::createRequestId(const QString &action)
{
    return QStringLiteral("req_client_%1_%2")
        .arg(action, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

void UserApiClient::applyRequestHeaders(QNetworkRequest *request,
                                        const QString &requestId)
{
    if (!request)
    {
        return;
    }

    request->setRawHeader("Accept", "*/*");
    request->setRawHeader("X-Request-Id", requestId.toUtf8());
}

}  // namespace chatclient::api
