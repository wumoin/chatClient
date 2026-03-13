#include "config/appconfig.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace chatclient::config {
namespace {

AppConfig g_appConfig;
bool g_initialized = false;

}  // namespace

bool AppConfig::initialize(QString *errorMessage)
{
    if (g_initialized)
    {
        return true;
    }

    AppConfig loadedConfig;
    if (!loadedConfig.load(errorMessage))
    {
        return false;
    }

    g_appConfig = loadedConfig;
    g_initialized = true;
    return true;
}

const AppConfig &AppConfig::instance()
{
    return g_appConfig;
}

const QString &AppConfig::displayName() const
{
    return displayName_;
}

const QString &AppConfig::loginWindowTitle() const
{
    return loginWindowTitle_;
}

const QString &AppConfig::chatWindowTitle() const
{
    return chatWindowTitle_;
}

const QUrl &AppConfig::httpBaseUrl() const
{
    return httpBaseUrl_;
}

QUrl AppConfig::registerUrl() const
{
    return httpBaseUrl_.resolved(QUrl(registerPath_));
}

QUrl AppConfig::loginUrl() const
{
    return httpBaseUrl_.resolved(QUrl(loginPath_));
}

const QUrl &AppConfig::webSocketUrl() const
{
    return webSocketUrl_;
}

QString AppConfig::httpBaseUrlText() const
{
    return httpBaseUrl_.toString();
}

bool AppConfig::load(QString *errorMessage)
{
    QFile file(defaultConfigPath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("无法打开客户端配置文件：%1").arg(file.fileName());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("客户端配置文件不是合法 JSON：%1")
                                .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject root = document.object();
    const QJsonValue appValue = root.value(QStringLiteral("app"));
    const QJsonValue servicesValue = root.value(QStringLiteral("services"));
    if (!appValue.isObject() || !servicesValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("客户端配置文件缺少 app 或 services 对象");
        }
        return false;
    }

    const QJsonObject appObject = appValue.toObject();
    const QJsonObject servicesObject = servicesValue.toObject();
    const QJsonValue httpValue = servicesObject.value(QStringLiteral("http"));
    const QJsonValue wsValue = servicesObject.value(QStringLiteral("ws"));
    if (!httpValue.isObject() || !wsValue.isObject())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("客户端配置文件缺少 http 或 ws 服务配置");
        }
        return false;
    }

    QString httpBaseUrlText;
    QString webSocketUrlText;

    if (!readRequiredString(appObject,
                            QStringLiteral("display_name"),
                            &displayName_,
                            errorMessage) ||
        !readRequiredString(appObject,
                            QStringLiteral("login_window_title"),
                            &loginWindowTitle_,
                            errorMessage) ||
        !readRequiredString(appObject,
                            QStringLiteral("chat_window_title"),
                            &chatWindowTitle_,
                            errorMessage) ||
        !readRequiredString(httpValue.toObject(),
                            QStringLiteral("base_url"),
                            &httpBaseUrlText,
                            errorMessage) ||
        !readRequiredString(httpValue.toObject(),
                            QStringLiteral("register_path"),
                            &registerPath_,
                            errorMessage) ||
        !readRequiredString(httpValue.toObject(),
                            QStringLiteral("login_path"),
                            &loginPath_,
                            errorMessage) ||
        !readRequiredString(wsValue.toObject(),
                            QStringLiteral("url"),
                            &webSocketUrlText,
                            errorMessage))
    {
        return false;
    }

    httpBaseUrl_ = QUrl(httpBaseUrlText);
    webSocketUrl_ = QUrl(webSocketUrlText);

    if (!httpBaseUrl_.isValid() || httpBaseUrl_.scheme().isEmpty() ||
        httpBaseUrl_.host().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("http.base_url 不是合法的服务地址：%1")
                    .arg(httpBaseUrlText);
        }
        return false;
    }

    if (!webSocketUrl_.isValid() || webSocketUrl_.scheme().isEmpty() ||
        webSocketUrl_.host().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("ws.url 不是合法的 WebSocket 地址：%1")
                    .arg(webSocketUrlText);
        }
        return false;
    }

    if (!registerPath_.startsWith(QLatin1Char('/')) ||
        !loginPath_.startsWith(QLatin1Char('/')))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("register_path 和 login_path 必须以 / 开头");
        }
        return false;
    }

    return true;
}

QString AppConfig::defaultConfigPath()
{
    return QString::fromUtf8(CHATCLIENT_DEFAULT_CONFIG_PATH);
}

bool AppConfig::readRequiredString(const QJsonObject &object,
                                   const QString &key,
                                   QString *out,
                                   QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (!value.isString())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("客户端配置字段 %1 必须是字符串").arg(key);
        }
        return false;
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("客户端配置字段 %1 不能为空").arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = text;
    }
    return true;
}

}  // namespace chatclient::config
