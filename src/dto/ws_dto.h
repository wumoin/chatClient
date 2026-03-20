#pragma once

#include <QJsonObject>
#include <QString>

namespace chatclient::dto::ws {

/**
 * @brief WebSocket 统一信封 DTO。
 *
 * 客户端和服务端当前都先按统一信封收发 JSON 文本，
 * 这样后续继续扩 `message.new`、`conversation.created` 时，
 * 不需要再改最外层结构。
 */
struct WsEnvelopeDto
{
    qint64 version = 1;
    QString type;
    QString requestId;
    qint64 tsMs = 0;
    QJsonObject payload;
};

/**
 * @brief `ws.auth` 请求载荷 DTO。
 */
struct WsAuthRequestDto
{
    QString accessToken;
    QString deviceId;
    QString deviceSessionId;
    QString clientVersion;
};

/**
 * @brief `ws.auth.ok` 成功载荷 DTO。
 */
struct WsAuthOkDto
{
    QString userId;
    QString deviceSessionId;
};

/**
 * @brief `ws.error` 错误载荷 DTO。
 */
struct WsErrorDto
{
    int code = 0;
    QString message;
};

/**
 * @brief `ws.new` 通用业务事件载荷 DTO。
 */
struct WsNewEventDto
{
    QString route;
    QJsonObject data;
};

/**
 * @brief 把统一信封 DTO 转成 JSON 对象。
 * @param envelope 已组装完成的信封 DTO。
 * @return 可直接序列化发给服务端的 JSON 对象。
 */
QJsonObject toJsonObject(const WsEnvelopeDto &envelope);

/**
 * @brief 把 `ws.auth` 请求 DTO 转成 JSON 对象。
 * @param request 认证请求 DTO。
 * @return 可直接写入统一信封 `payload` 的 JSON 对象。
 */
QJsonObject toJsonObject(const WsAuthRequestDto &request);

/**
 * @brief 解析统一信封。
 * @param json 收到的根 JSON 对象。
 * @param out 成功时写入解析后的信封 DTO。
 * @param errorMessage 失败时写入错误信息，可为空。
 * @return true 表示结构合法；false 表示字段缺失或类型不匹配。
 */
bool parseWsEnvelope(const QJsonObject &json,
                     WsEnvelopeDto *out,
                     QString *errorMessage);

/**
 * @brief 解析 `ws.auth.ok` 载荷。
 * @param json 信封中的 `payload` 对象。
 * @param out 成功时写入认证成功载荷。
 * @param errorMessage 失败时写入错误信息，可为空。
 * @return true 表示解析成功；false 表示结构不合法。
 */
bool parseWsAuthOkPayload(const QJsonObject &json,
                          WsAuthOkDto *out,
                          QString *errorMessage);

/**
 * @brief 解析 `ws.error` 载荷。
 * @param json 信封中的 `payload` 对象。
 * @param out 成功时写入错误载荷。
 * @param errorMessage 失败时写入错误信息，可为空。
 * @return true 表示解析成功；false 表示结构不合法。
 */
bool parseWsErrorPayload(const QJsonObject &json,
                         WsErrorDto *out,
                         QString *errorMessage);

/**
 * @brief 解析 `ws.new` 载荷。
 * @param json 信封中的 `payload` 对象。
 * @param out 成功时写入业务事件载荷。
 * @param errorMessage 失败时写入错误信息，可为空。
 * @return true 表示解析成功；false 表示结构不合法。
 */
bool parseWsNewPayload(const QJsonObject &json,
                       WsNewEventDto *out,
                       QString *errorMessage);

}  // namespace chatclient::dto::ws
