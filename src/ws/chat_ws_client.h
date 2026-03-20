#pragma once

#include <QAbstractSocket>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTimer;
class QWebSocket;

namespace chatclient::ws {

/**
 * @brief 客户端最小 WebSocket 长连接客户端。
 *
 * 当前只负责：
 * 1) 建立到 `/ws` 的 WebSocket 连接；
 * 2) 连接建立后发送 `ws.auth`；
 * 3) 暴露统一的状态和失败信号给聊天窗口使用。
 */
class ChatWsClient : public QObject
{
    Q_OBJECT

  public:
    /**
     * @brief 构造最小 WebSocket 客户端。
     * @param parent 父级 QObject，可为空。
     */
    explicit ChatWsClient(QObject *parent = nullptr);

    /**
     * @brief 设置当前 WS 鉴权所需的最小登录态。
     * @param accessToken 当前 access token。
     * @param deviceId 当前稳定设备标识。
     * @param deviceSessionId 当前设备会话 ID。
     * @param clientVersion 可选客户端版本，当前可为空。
     */
    void setSession(const QString &accessToken,
                    const QString &deviceId,
                    const QString &deviceSessionId,
                    const QString &clientVersion = QString());

    /**
     * @brief 建立到服务端的长连接并在连接成功后发起 `ws.auth`。
     */
    void connectToServer();

    /**
     * @brief 主动关闭当前长连接，并停止自动重连。
     */
    void disconnectFromServer();

    /**
     * @brief 发送统一业务事件。
     * @param route 业务路由。
     * @param data 业务载荷。
     * @return 成功发出时返回当前 request_id；失败时返回空字符串。
     */
    QString sendBusinessEvent(const QString &route, const QJsonObject &data);

    /**
     * @brief 判断当前长连接是否已经完成 `ws.auth`。
     * @return true 表示已认证；false 表示尚未完成认证或已断开。
     */
    bool isAuthenticated() const;

  signals:
    /**
     * @brief WS 状态文本发生变化。
     * @param statusText 适合直接展示到界面的状态文本。
     */
    void statusChanged(const QString &statusText);

    /**
     * @brief `ws.auth` 成功。
     * @param userId 服务端确认的当前用户 ID。
     * @param deviceSessionId 服务端确认的设备会话 ID。
     */
    void authenticated(const QString &userId, const QString &deviceSessionId);

    /**
     * @brief `ws.auth` 失败。
     * @param message 适合直接展示给用户的失败原因。
     */
    void authenticationFailed(const QString &message);

    /**
     * @brief 收到服务端主动推送的 `ws.new` 业务事件。
     * @param route 当前事件的业务路由。
     * @param data 当前事件的业务载荷。
     */
    void newEventReceived(const QString &route, const QJsonObject &data);

    /**
     * @brief 收到服务端返回的 `ws.ack` 业务确认。
     * @param route 当前确认对应的业务路由。
     * @param ok 本次业务动作是否成功。
     * @param code 业务结果码。
     * @param message 业务说明文本。
     * @param data 当前确认附带的业务数据。
     * @param requestId 当前确认对应的 request_id。
     */
    void ackReceived(const QString &route,
                     bool ok,
                     int code,
                     const QString &message,
                     const QJsonObject &data,
                     const QString &requestId);

  private:
    /**
     * @brief 处理底层连接成功事件，并发送 `ws.auth`。
     */
    void handleConnected();

    /**
     * @brief 处理收到的文本帧。
     * @param message 原始 JSON 文本。
     */
    void handleTextMessageReceived(const QString &message);

    /**
     * @brief 处理底层断开事件，并按需要触发重连。
     */
    void handleDisconnected();

    /**
     * @brief 处理底层连接即将关闭事件，补充关闭诊断日志。
     */
    void handleAboutToClose();

    /**
     * @brief 处理底层套接字状态变化事件。
     * @param state 当前套接字状态。
     */
    void handleSocketStateChanged(QAbstractSocket::SocketState state);

    /**
     * @brief 处理底层套接字错误。
     * @param error Qt 套接字错误枚举值。
     */
    void handleSocketError(QAbstractSocket::SocketError error);

    /**
     * @brief 发送 `ws.auth` 信封。
     */
    void sendAuth();

    /**
     * @brief 发送统一信封。
     * @param type 事件类型。
     * @param requestId 当前请求追踪 ID。
     * @param payload 统一信封中的 `payload` 对象。
     */
    void sendEnvelope(const QString &type,
                      const QString &requestId,
                      const QJsonObject &payload);

    /**
     * @brief 判断当前是否具备最小鉴权上下文。
     * @return true 表示 access token / device 信息完整。
     */
    bool hasSessionContext() const;

    /**
     * @brief 生成当前事件的本地 request_id。
     * @param action 当前动作标识。
     * @return 带前缀的 request_id。
     */
    static QString createRequestId(const QString &action);

    /**
     * @brief 返回当前毫秒级 Unix 时间戳。
     * @return 统一信封使用的 `ts_ms`。
     */
    static qint64 nowEpochMs();

    /**
     * @brief 更新当前界面状态文本。
     * @param statusText 新状态文本。
     */
    void updateStatus(const QString &statusText);

    QWebSocket *m_socket = nullptr;
    QTimer *m_reconnectTimer = nullptr;
    QString m_accessToken;
    QString m_deviceId;
    QString m_deviceSessionId;
    QString m_clientVersion;
    bool m_authenticated = false;
    bool m_reconnectEnabled = false;
};

}  // namespace chatclient::ws
