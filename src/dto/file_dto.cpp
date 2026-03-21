#include "dto/file_dto.h"

#include <QJsonValue>

namespace chatclient::dto::file {
namespace {

/**
 * @brief 读取 JSON 对象中的必填字符串字段。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @param out 成功时写入字符串值。
 * @param errorMessage 失败时写入错误原因。
 * @return true 表示字段存在且类型正确；false 表示字段缺失或类型不匹配。
 */
bool readRequiredString(const QJsonObject &object,
                        const QString &key,
                        QString *out,
                        QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是字符串").arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = value.toString();
    }
    return true;
}

/**
 * @brief 读取 JSON 对象中的必填 64 位整数。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @param out 成功时写入整数值。
 * @param errorMessage 失败时写入错误原因。
 * @return true 表示字段存在且类型正确；false 表示字段缺失或类型不匹配。
 */
bool readRequiredInt64(const QJsonObject &object,
                       const QString &key,
                       qint64 *out,
                       QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应字段 %1 缺失或不是数字").arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = static_cast<qint64>(value.toDouble());
    }
    return true;
}

/**
 * @brief 读取 JSON 对象中的可选字符串字段。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @return 如果字段存在且是字符串则返回其值；否则返回空字符串。
 */
QString readOptionalString(const QJsonObject &object, const QString &key)
{
    const QJsonValue value = object.value(key);
    return value.isString() ? value.toString() : QString();
}

/**
 * @brief 读取 JSON 对象中的可选整数。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @param out 成功时写入整数值。
 * @return true 表示读取到了合法数字；false 表示字段不存在或类型不匹配。
 */
bool readOptionalInt(const QJsonObject &object,
                     const QString &key,
                     int *out)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    if (out)
    {
        *out = value.toInt();
    }
    return true;
}

/**
 * @brief 读取 JSON 对象中的可选 64 位整数。
 * @param object 当前 JSON 对象。
 * @param key 目标字段名。
 * @param out 成功时写入整数值。
 * @return true 表示读取到了合法数字；false 表示字段不存在或类型不匹配。
 */
bool readOptionalInt64(const QJsonObject &object,
                       const QString &key,
                       qint64 *out)
{
    const QJsonValue value = object.value(key);
    if (!value.isDouble())
    {
        return false;
    }

    if (out)
    {
        *out = static_cast<qint64>(value.toDouble());
    }
    return true;
}

}  // namespace

bool parseAttachmentObject(const QJsonObject &object,
                           AttachmentDto *out,
                           QString *errorMessage)
{
    // 这里解析的是服务端 attachments 表对应的最小对外视图：
    // 稳定标识、展示信息、下载地址，以及图片类附件的可选尺寸。
    AttachmentDto parsed;
    if (!readRequiredString(object,
                            QStringLiteral("attachment_id"),
                            &parsed.attachmentId,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("file_name"),
                            &parsed.fileName,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("mime_type"),
                            &parsed.mimeType,
                            errorMessage) ||
        !readRequiredInt64(object,
                           QStringLiteral("size_bytes"),
                           &parsed.sizeBytes,
                           errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("media_kind"),
                            &parsed.mediaKind,
                            errorMessage) ||
        !readRequiredString(object,
                            QStringLiteral("download_url"),
                            &parsed.downloadUrl,
                            errorMessage))
    {
        return false;
    }

    parsed.storageKey = readOptionalString(object, QStringLiteral("storage_key"));
    parsed.hasStorageKey = !parsed.storageKey.isEmpty();
    parsed.hasImageWidth = readOptionalInt(object,
                                           QStringLiteral("image_width"),
                                           &parsed.imageWidth);
    parsed.hasImageHeight = readOptionalInt(object,
                                            QStringLiteral("image_height"),
                                            &parsed.imageHeight);
    parsed.hasCreatedAtMs = readOptionalInt64(object,
                                              QStringLiteral("created_at_ms"),
                                              &parsed.createdAtMs);

    if (out)
    {
        *out = parsed;
    }
    return true;
}

bool parseUploadAttachmentSuccessResponse(
    const QJsonObject &root,
    UploadAttachmentResponseDto *out,
    QString *errorMessage)
{
    if (root.value(QStringLiteral("code")).toInt(-1) != 0)
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("服务端返回了非成功业务码");
        }
        return false;
    }

    const QJsonValue dataValue = root.value(QStringLiteral("data"));
    if (!dataValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 data 对象");
        }
        return false;
    }

    const QJsonValue attachmentValue =
        dataValue.toObject().value(QStringLiteral("attachment"));
    if (!attachmentValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("响应缺少 attachment 对象");
        }
        return false;
    }

    UploadAttachmentResponseDto parsed;
    // request_id 仍然取顶层字段，方便把上传日志和后续错误回溯串起来。
    parsed.requestId = readOptionalString(root, QStringLiteral("request_id"));
    if (!parseAttachmentObject(attachmentValue.toObject(),
                               &parsed.attachment,
                               errorMessage))
    {
        return false;
    }

    if (out)
    {
        *out = parsed;
    }
    return true;
}

ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  const int httpStatus,
                                  const QString &fallbackMessage)
{
    ApiErrorDto error;
    error.httpStatus = httpStatus;
    error.errorCode = root.value(QStringLiteral("code")).toInt(0);

    const QString message = root.value(QStringLiteral("message")).toString();
    error.message = message.isEmpty() ? fallbackMessage : message;
    error.requestId = root.value(QStringLiteral("request_id")).toString();
    return error;
}

}  // namespace chatclient::dto::file
