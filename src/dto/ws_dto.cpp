#include "dto/ws_dto.h"

// WS DTO 只负责“协议层” JSON 与结构体互转。
// 业务层 route 的真正含义不在这里处理，而是交给 ChatWsClient 和
// ConversationManager 继续分发。
namespace chatclient::dto::ws {

QJsonObject toJsonObject(const WsEnvelopeDto &envelope)
{
    QJsonObject json;
    json.insert(QStringLiteral("version"), envelope.version);
    json.insert(QStringLiteral("type"), envelope.type);
    json.insert(QStringLiteral("request_id"), envelope.requestId);
    json.insert(QStringLiteral("ts_ms"), envelope.tsMs);
    json.insert(QStringLiteral("payload"), envelope.payload);
    return json;
}

QJsonObject toJsonObject(const WsAuthRequestDto &request)
{
    QJsonObject json;
    json.insert(QStringLiteral("access_token"), request.accessToken);
    json.insert(QStringLiteral("device_id"), request.deviceId);
    json.insert(QStringLiteral("device_session_id"), request.deviceSessionId);
    if (!request.clientVersion.trimmed().isEmpty())
    {
        json.insert(QStringLiteral("client_version"), request.clientVersion);
    }
    return json;
}

bool parseWsEnvelope(const QJsonObject &json,
                     WsEnvelopeDto *out,
                     QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    const auto versionValue = json.value(QStringLiteral("version"));
    if (!versionValue.isDouble())
    {
        return fail(QStringLiteral("field 'version' must be an integer"));
    }

    const auto typeValue = json.value(QStringLiteral("type"));
    if (!typeValue.isString())
    {
        return fail(QStringLiteral("field 'type' must be a string"));
    }

    const auto requestIdValue = json.value(QStringLiteral("request_id"));
    if (!requestIdValue.isString())
    {
        return fail(QStringLiteral("field 'request_id' must be a string"));
    }

    const auto tsMsValue = json.value(QStringLiteral("ts_ms"));
    if (!tsMsValue.isDouble())
    {
        return fail(QStringLiteral("field 'ts_ms' must be an integer"));
    }

    const auto payloadValue = json.value(QStringLiteral("payload"));
    if (!payloadValue.isObject())
    {
        return fail(QStringLiteral("field 'payload' must be an object"));
    }

    if (out)
    {
        out->version = versionValue.toInteger();
        out->type = typeValue.toString().trimmed();
        out->requestId = requestIdValue.toString().trimmed();
        out->tsMs = tsMsValue.toInteger();
        out->payload = payloadValue.toObject();
    }

    return true;
}

bool parseWsAuthOkPayload(const QJsonObject &json,
                          WsAuthOkDto *out,
                          QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    const auto userIdValue = json.value(QStringLiteral("user_id"));
    if (!userIdValue.isString())
    {
        return fail(QStringLiteral("field 'user_id' must be a string"));
    }

    const auto deviceSessionValue =
        json.value(QStringLiteral("device_session_id"));
    if (!deviceSessionValue.isString())
    {
        return fail(QStringLiteral("field 'device_session_id' must be a string"));
    }

    if (out)
    {
        out->userId = userIdValue.toString().trimmed();
        out->deviceSessionId = deviceSessionValue.toString().trimmed();
    }

    return true;
}

bool parseWsErrorPayload(const QJsonObject &json,
                         WsErrorDto *out,
                         QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    const auto codeValue = json.value(QStringLiteral("code"));
    if (!codeValue.isDouble())
    {
        return fail(QStringLiteral("field 'code' must be an integer"));
    }

    const auto messageValue = json.value(QStringLiteral("message"));
    if (!messageValue.isString())
    {
        return fail(QStringLiteral("field 'message' must be a string"));
    }

    if (out)
    {
        out->code = codeValue.toInt();
        out->message = messageValue.toString().trimmed();
    }

    return true;
}

bool parseWsAckPayload(const QJsonObject &json,
                       WsAckEventDto *out,
                       QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    const auto routeValue = json.value(QStringLiteral("route"));
    if (!routeValue.isString())
    {
        return fail(QStringLiteral("field 'route' must be a string"));
    }

    const auto okValue = json.value(QStringLiteral("ok"));
    if (!okValue.isBool())
    {
        return fail(QStringLiteral("field 'ok' must be a boolean"));
    }

    const auto codeValue = json.value(QStringLiteral("code"));
    if (!codeValue.isDouble())
    {
        return fail(QStringLiteral("field 'code' must be an integer"));
    }

    const auto messageValue = json.value(QStringLiteral("message"));
    if (!messageValue.isString())
    {
        return fail(QStringLiteral("field 'message' must be a string"));
    }

    const auto dataValue = json.value(QStringLiteral("data"));
    if (!dataValue.isObject())
    {
        return fail(QStringLiteral("field 'data' must be an object"));
    }

    if (out)
    {
        out->route = routeValue.toString().trimmed();
        out->ok = okValue.toBool();
        out->code = codeValue.toInt();
        out->message = messageValue.toString().trimmed();
        out->data = dataValue.toObject();
    }

    return true;
}

bool parseWsNewPayload(const QJsonObject &json,
                       WsNewEventDto *out,
                       QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &message) {
        if (errorMessage)
        {
            *errorMessage = message;
        }
        return false;
    };

    const auto routeValue = json.value(QStringLiteral("route"));
    if (!routeValue.isString())
    {
        return fail(QStringLiteral("field 'route' must be a string"));
    }

    const auto dataValue = json.value(QStringLiteral("data"));
    if (!dataValue.isObject())
    {
        return fail(QStringLiteral("field 'data' must be an object"));
    }

    if (out)
    {
        out->route = routeValue.toString().trimmed();
        out->data = dataValue.toObject();
    }

    return true;
}

}  // namespace chatclient::dto::ws
