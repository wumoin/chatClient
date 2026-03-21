#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace chatclient::dto::file {

/**
 * @brief 附件接口失败 DTO。
 */
struct ApiErrorDto
{
    // 当前 HTTP 状态码，例如 400 / 401 / 500。
    int httpStatus = 0;
    // 服务端业务错误码。
    int errorCode = 0;
    // 适合直接展示或记录日志的错误消息。
    QString message;
    // 本次请求的 request_id，便于前后端日志对齐。
    QString requestId;
};

/**
 * @brief 临时附件上传摘要 DTO。
 *
 * 这里描述的是“已经上传到服务端临时目录”的文件，
 * 还不是可供下载接口直接访问的正式 attachment。
 */
struct TemporaryAttachmentUploadDto
{
    // 临时上传引用，后续 message.send_image 要靠它确认正式附件。
    QString attachmentUploadKey;
    // 原始文件名，适合界面展示或日志记录。
    QString fileName;
    // 服务端记录的 MIME 类型。
    QString mimeType;
    // 文件字节大小。
    qint64 sizeBytes = 0;
    // 附件媒体类别，当前只允许 image / file。
    QString mediaKind;
    // true 表示当前临时上传带了宽度信息。
    bool hasImageWidth = false;
    // 图片宽度。
    int imageWidth = 0;
    // true 表示当前临时上传带了高度信息。
    bool hasImageHeight = false;
    // 图片高度。
    int imageHeight = 0;
};

/**
 * @brief 附件上传成功响应 DTO。
 */
struct UploadAttachmentResponseDto
{
    // 顶层 request_id。
    QString requestId;
    // 本次上传成功后的临时上传摘要。
    TemporaryAttachmentUploadDto upload;
};

/**
 * @brief 附件下载成功结果 DTO。
 */
struct DownloadedAttachmentDto
{
    // 下载到的原始二进制内容。
    QByteArray content;
    // 响应头里的 Content-Type。
    QString mimeType;
    // 从 Content-Disposition 中尽量提取出来的文件名，可能为空。
    QString fileName;
};

/**
 * @brief 解析单个临时附件上传对象。
 * @param object 当前临时上传 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示结构不符合当前协议。
 */
bool parseTemporaryAttachmentUploadObject(
    const QJsonObject &object,
    TemporaryAttachmentUploadDto *out,
    QString *errorMessage);

/**
 * @brief 解析附件上传成功响应。
 * @param root 服务端响应根 JSON 对象。
 * @param out 成功时写入解析后的 DTO。
 * @param errorMessage 解析失败时写入原因，可为空。
 * @return true 表示解析成功；false 表示响应结构不符合当前协议。
 */
bool parseUploadAttachmentSuccessResponse(
    const QJsonObject &root,
    UploadAttachmentResponseDto *out,
    QString *errorMessage);

/**
 * @brief 解析附件接口失败响应。
 * @param root 服务端响应根 JSON 对象。
 * @param httpStatus 当前 HTTP 状态码。
 * @param fallbackMessage 当响应体不完整时使用的兜底消息。
 * @return 统一的失败 DTO。
 */
ApiErrorDto parseApiErrorResponse(const QJsonObject &root,
                                  int httpStatus,
                                  const QString &fallbackMessage);

}  // namespace chatclient::dto::file
