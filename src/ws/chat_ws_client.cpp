#include "ws/chat_ws_client.h"

#include "config/appconfig.h"
#include "dto/ws_dto.h"
#include "log/app_logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTimer>
#include <QUuid>
#include <QWebSocket>

namespace chatclient::ws {
namespace {

constexpr auto kWsLogTag = "ws.client";
constexpr int kReconnectIntervalMs = 3000;

QString closeCodeText(QWebSocketProtocol::CloseCode code)
{
    switch (code)
    {
    case QWebSocketProtocol::CloseCodeNormal:
        return QStringLiteral("正常关闭");
    case QWebSocketProtocol::CloseCodeGoingAway:
        return QStringLiteral("对端离开");
    case QWebSocketProtocol::CloseCodeProtocolError:
        return QStringLiteral("协议错误");
    case QWebSocketProtocol::CloseCodeDatatypeNotSupported:
        return QStringLiteral("数据类型不支持");
    case QWebSocketProtocol::CloseCodeWrongDatatype:
        return QStringLiteral("消息类型错误");
    case QWebSocketProtocol::CloseCodeTooMuchData:
        return QStringLiteral("消息过大");
    case QWebSocketProtocol::CloseCodePolicyViolated:
        return QStringLiteral("违反策略");
    case QWebSocketProtocol::CloseCodeMissingExtension:
        return QStringLiteral("缺少扩展");
    case QWebSocketProtocol::CloseCodeBadOperation:
        return QStringLiteral("无效操作");
    case QWebSocketProtocol::CloseCodeTlsHandshakeFailed:
        return QStringLiteral("TLS 握手失败");
    case QWebSocketProtocol::CloseCodeAbnormalDisconnection:
        return QStringLiteral("异常断开");
    case QWebSocketProtocol::CloseCodeReserved1004:
    case QWebSocketProtocol::CloseCodeMissingStatusCode:
        return QStringLiteral("保留关闭码");
    default:
        return QStringLiteral("未定义关闭原因");
    }
}

QString socketStateText(QAbstractSocket::SocketState state)
{
    switch (state)
    {
    case QAbstractSocket::UnconnectedState:
        return QStringLiteral("未连接");
    case QAbstractSocket::HostLookupState:
        return QStringLiteral("解析主机中");
    case QAbstractSocket::ConnectingState:
        return QStringLiteral("连接中");
    case QAbstractSocket::ConnectedState:
        return QStringLiteral("已连接");
    case QAbstractSocket::BoundState:
        return QStringLiteral("已绑定");
    case QAbstractSocket::ClosingState:
        return QStringLiteral("关闭中");
    case QAbstractSocket::ListeningState:
        return QStringLiteral("监听中");
    }

    return QStringLiteral("未知状态");
}

QString localizeWsError(const chatclient::dto::ws::WsErrorDto &error)
{
    switch (error.code)
    {
    case 40102:
        return QStringLiteral("实时通道鉴权失败，请重新登录");
    case 40001:
        return QStringLiteral("实时通道请求格式不正确");
    case 40300:
        return QStringLiteral("当前实时通道连接已被服务端拒绝");
    default:
        break;
    }

    if (error.message == QStringLiteral("invalid access token"))
    {
        return QStringLiteral("实时通道鉴权失败，请重新登录");
    }

    if (error.message == QStringLiteral("websocket connection is not authenticated"))
    {
        return QStringLiteral("实时通道尚未完成认证");
    }

    if (error.message == QStringLiteral("unsupported websocket event type"))
    {
        return QStringLiteral("当前实时通道事件暂未受支持");
    }

    return QStringLiteral("实时通道发生错误，请稍后重试");
}

}  // namespace

ChatWsClient::ChatWsClient(QObject *parent)
    : QObject(parent),
      m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)),
      m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setInterval(kReconnectIntervalMs);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer,
            &QTimer::timeout,
            this,
            &ChatWsClient::connectToServer);

    connect(m_socket, &QWebSocket::connected, this, &ChatWsClient::handleConnected);
    connect(m_socket,
            &QWebSocket::textMessageReceived,
            this,
            &ChatWsClient::handleTextMessageReceived);
    connect(m_socket,
            &QWebSocket::disconnected,
            this,
            &ChatWsClient::handleDisconnected);
    connect(m_socket,
            &QWebSocket::aboutToClose,
            this,
            &ChatWsClient::handleAboutToClose);
    connect(m_socket,
            &QWebSocket::stateChanged,
            this,
            &ChatWsClient::handleSocketStateChanged);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_socket,
            &QWebSocket::errorOccurred,
            this,
            &ChatWsClient::handleSocketError);
#else
    connect(m_socket,
            QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error),
            this,
            &ChatWsClient::handleSocketError);
#endif
}

void ChatWsClient::setSession(const QString &accessToken,
                              const QString &deviceId,
                              const QString &deviceSessionId,
                              const QString &clientVersion)
{
    m_accessToken = accessToken.trimmed();
    m_deviceId = deviceId.trimmed();
    m_deviceSessionId = deviceSessionId.trimmed();
    m_clientVersion = clientVersion.trimmed();
}

void ChatWsClient::connectToServer()
{
    m_reconnectTimer->stop();

    if (!hasSessionContext())
    {
        updateStatus(QStringLiteral("实时通道缺少登录态"));
        return;
    }

    if (m_socket->state() == QAbstractSocket::ConnectedState ||
        m_socket->state() == QAbstractSocket::ConnectingState)
    {
        return;
    }

    const QUrl wsUrl = chatclient::config::AppConfig::instance().webSocketUrl();
    if (!wsUrl.isValid())
    {
        updateStatus(QStringLiteral("实时通道地址无效"));
        return;
    }

    m_reconnectEnabled = true;
    m_authenticated = false;
    updateStatus(QStringLiteral("正在连接实时通道"));

    CHATCLIENT_LOG_INFO(kWsLogTag)
        << "开始连接实时通道，url=" << wsUrl.toString()
        << " device_session_id=" << m_deviceSessionId;

    m_socket->open(wsUrl);
}

void ChatWsClient::disconnectFromServer()
{
    m_reconnectEnabled = false;
    m_authenticated = false;
    m_reconnectTimer->stop();

    if (m_socket->state() == QAbstractSocket::ConnectingState ||
        m_socket->state() == QAbstractSocket::ConnectedState)
    {
        m_socket->close();
    }

    updateStatus(QStringLiteral("实时通道已断开"));
}

QString ChatWsClient::sendBusinessEvent(const QString &route,
                                        const QJsonObject &data)
{
    const QString trimmedRoute = route.trimmed();
    if (trimmedRoute.isEmpty())
    {
        CHATCLIENT_LOG_WARN(kWsLogTag)
            << "拒绝发送实时业务事件，route 为空";
        return QString();
    }

    if (m_socket->state() != QAbstractSocket::ConnectedState || !m_authenticated)
    {
        CHATCLIENT_LOG_WARN(kWsLogTag)
            << "拒绝发送实时业务事件，当前通道不可用，route="
            << trimmedRoute
            << " authenticated=" << m_authenticated;
        return QString();
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("route"), trimmedRoute);
    payload.insert(QStringLiteral("data"), data);

    QString action = trimmedRoute;
    action.replace(QLatin1Char('.'), QLatin1Char('_'));
    const QString requestId =
        createRequestId(action.isEmpty() ? QStringLiteral("send") : action);
    sendEnvelope(QStringLiteral("ws.send"), requestId, payload);
    return requestId;
}

bool ChatWsClient::isAuthenticated() const
{
    return m_authenticated;
}

void ChatWsClient::handleConnected()
{
    updateStatus(QStringLiteral("实时通道已连接，正在认证"));
    sendAuth();
}

void ChatWsClient::handleTextMessageReceived(const QString &message)
{
    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(message.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        CHATCLIENT_LOG_WARN(kWsLogTag)
            << "收到无效的 WebSocket JSON 消息，error="
            << parseError.errorString();
        updateStatus(QStringLiteral("实时通道收到无法识别的数据"));
        return;
    }

    chatclient::dto::ws::WsEnvelopeDto envelope;
    QString errorMessage;
    if (!chatclient::dto::ws::parseWsEnvelope(document.object(),
                                              &envelope,
                                              &errorMessage))
    {
        CHATCLIENT_LOG_WARN(kWsLogTag)
            << "收到无效的 WebSocket 信封消息，message="
            << errorMessage;
        updateStatus(QStringLiteral("实时通道收到无效事件"));
        return;
    }

    if (envelope.type == QStringLiteral("ws.auth.ok"))
    {
        chatclient::dto::ws::WsAuthOkDto payload;
        if (!chatclient::dto::ws::parseWsAuthOkPayload(envelope.payload,
                                                       &payload,
                                                       &errorMessage))
        {
            CHATCLIENT_LOG_WARN(kWsLogTag)
                << "解析 ws.auth.ok 失败，message=" << errorMessage;
            updateStatus(QStringLiteral("实时通道认证响应无效"));
            return;
        }

        m_authenticated = true;
        updateStatus(QStringLiteral("实时通道已连接"));

        CHATCLIENT_LOG_INFO(kWsLogTag)
            << "实时通道认证成功，user_id=" << payload.userId
            << " device_session_id=" << payload.deviceSessionId;
        emit authenticated(payload.userId, payload.deviceSessionId);
        return;
    }

    if (envelope.type == QStringLiteral("ws.error"))
    {
        chatclient::dto::ws::WsErrorDto payload;
        if (!chatclient::dto::ws::parseWsErrorPayload(envelope.payload,
                                                      &payload,
                                                      &errorMessage))
        {
            CHATCLIENT_LOG_WARN(kWsLogTag)
                << "解析 ws.error 失败，message=" << errorMessage;
            updateStatus(QStringLiteral("实时通道返回了无效错误"));
            return;
        }

        const QString localizedMessage = localizeWsError(payload);
        CHATCLIENT_LOG_WARN(kWsLogTag)
            << "实时通道返回错误，code=" << payload.code
            << " message=" << payload.message;

        updateStatus(localizedMessage);
        if (!m_authenticated)
        {
            if (payload.code == 40102 || payload.code == 40300)
            {
                m_reconnectEnabled = false;
            }
            emit authenticationFailed(localizedMessage);
        }
        return;
    }

    if (envelope.type == QStringLiteral("ws.new"))
    {
        chatclient::dto::ws::WsNewEventDto payload;
        if (!chatclient::dto::ws::parseWsNewPayload(envelope.payload,
                                                    &payload,
                                                    &errorMessage))
        {
            CHATCLIENT_LOG_WARN(kWsLogTag)
                << "解析 ws.new 失败，message=" << errorMessage;
            updateStatus(QStringLiteral("实时通道收到无效推送事件"));
            return;
        }

        CHATCLIENT_LOG_INFO(kWsLogTag)
            << "收到实时推送事件，route=" << payload.route;
        emit newEventReceived(payload.route, payload.data);
        return;
    }

    if (envelope.type == QStringLiteral("ws.ack"))
    {
        chatclient::dto::ws::WsAckEventDto payload;
        if (!chatclient::dto::ws::parseWsAckPayload(envelope.payload,
                                                    &payload,
                                                    &errorMessage))
        {
            CHATCLIENT_LOG_WARN(kWsLogTag)
                << "解析 ws.ack 失败，message=" << errorMessage;
            updateStatus(QStringLiteral("实时通道收到无效确认事件"));
            return;
        }

        CHATCLIENT_LOG_INFO(kWsLogTag)
            << "收到实时确认事件，route=" << payload.route
            << " ok=" << payload.ok
            << " code=" << payload.code;
        emit ackReceived(payload.route,
                         payload.ok,
                         payload.code,
                         payload.message,
                         payload.data,
                         envelope.requestId);
        return;
    }

    CHATCLIENT_LOG_DEBUG(kWsLogTag)
        << "忽略暂不支持的实时事件类型，type=" << envelope.type;
}

void ChatWsClient::handleDisconnected()
{
    const bool shouldReconnect = m_reconnectEnabled && hasSessionContext();
    m_authenticated = false;

    if (shouldReconnect)
    {
        updateStatus(QStringLiteral("实时通道已断开，正在重连"));
        m_reconnectTimer->start();
    }
    else
    {
        updateStatus(QStringLiteral("实时通道已断开"));
    }

    CHATCLIENT_LOG_INFO(kWsLogTag)
        << "实时通道已断开，close_code="
        << static_cast<int>(m_socket->closeCode())
        << " close_text="
        << closeCodeText(m_socket->closeCode())
        << " close_reason="
        << m_socket->closeReason()
        << " will_reconnect=" << shouldReconnect;
}

void ChatWsClient::handleAboutToClose()
{
    CHATCLIENT_LOG_INFO(kWsLogTag)
        << "实时通道即将关闭，close_code="
        << static_cast<int>(m_socket->closeCode())
        << " close_text="
        << closeCodeText(m_socket->closeCode())
        << " close_reason="
        << m_socket->closeReason();
}

void ChatWsClient::handleSocketStateChanged(
    QAbstractSocket::SocketState state)
{
    CHATCLIENT_LOG_DEBUG(kWsLogTag)
        << "实时通道状态变更，state=" << socketStateText(state);
}

void ChatWsClient::handleSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    CHATCLIENT_LOG_WARN(kWsLogTag)
        << "实时通道套接字错误，error=" << m_socket->errorString();
    if (!m_authenticated)
    {
        updateStatus(QStringLiteral("实时通道连接失败"));
    }
}

void ChatWsClient::sendAuth()
{
    chatclient::dto::ws::WsAuthRequestDto authRequest;
    authRequest.accessToken = m_accessToken;
    authRequest.deviceId = m_deviceId;
    authRequest.deviceSessionId = m_deviceSessionId;
    authRequest.clientVersion = m_clientVersion;

    sendEnvelope(QStringLiteral("ws.auth"),
                 createRequestId(QStringLiteral("auth")),
                 chatclient::dto::ws::toJsonObject(authRequest));
}

void ChatWsClient::sendEnvelope(const QString &type,
                                const QString &requestId,
                                const QJsonObject &payload)
{
    if (m_socket->state() != QAbstractSocket::ConnectedState)
    {
        return;
    }

    chatclient::dto::ws::WsEnvelopeDto envelope;
    envelope.version = 1;
    envelope.type = type;
    envelope.requestId = requestId;
    envelope.tsMs = nowEpochMs();
    envelope.payload = payload;

    const QByteArray bytes =
        QJsonDocument(chatclient::dto::ws::toJsonObject(envelope))
            .toJson(QJsonDocument::Compact);
    m_socket->sendTextMessage(QString::fromUtf8(bytes));
}

bool ChatWsClient::hasSessionContext() const
{
    return !m_accessToken.isEmpty() && !m_deviceId.isEmpty() &&
           !m_deviceSessionId.isEmpty();
}

QString ChatWsClient::createRequestId(const QString &action)
{
    return QStringLiteral("req_client_ws_%1_%2")
        .arg(action, QUuid::createUuid().toString(QUuid::WithoutBraces));
}

qint64 ChatWsClient::nowEpochMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

void ChatWsClient::updateStatus(const QString &statusText)
{
    emit statusChanged(statusText);
}

}  // namespace chatclient::ws
