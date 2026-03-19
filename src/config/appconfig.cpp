#include "config/appconfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
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

const QString &AppConfig::configFilePath() const
{
    return configFilePath_;
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

QUrl AppConfig::logoutUrl() const
{
    return httpBaseUrl_.resolved(QUrl(logoutPath_));
}

QUrl AppConfig::avatarTempUploadUrl() const
{
    return httpBaseUrl_.resolved(QUrl(avatarTempUploadPath_));
}

QUrl AppConfig::userAvatarUrl(const QString &userId) const
{
    QString resolvedPath = userAvatarPathTemplate_;
    resolvedPath.replace(QStringLiteral("{user_id}"), userId);
    return httpBaseUrl_.resolved(QUrl(resolvedPath));
}

const QUrl &AppConfig::webSocketUrl() const
{
    return webSocketUrl_;
}

QString AppConfig::httpBaseUrlText() const
{
    return httpBaseUrl_.toString();
}

const QString &AppConfig::logAppName() const
{
    return logAppName_;
}

const QString &AppConfig::logMinimumLevel() const
{
    return logMinimumLevel_;
}

bool AppConfig::isConsoleLogEnabled() const
{
    return logEnableConsole_;
}

bool AppConfig::isFileLogEnabled() const
{
    return logEnableFile_;
}

bool AppConfig::displayLocalTimeInLog() const
{
    return logDisplayLocalTime_;
}

const QString &AppConfig::logDirectory() const
{
    return logDirectory_;
}

const QString &AppConfig::logFileName() const
{
    return logFileName_;
}

QString AppConfig::resolvedLogFilePath() const
{
    if (!logEnableFile_)
    {
        return QString();
    }

    const QFileInfo configFileInfo(configFilePath_);
    const QDir configDir = configFileInfo.dir();

    QFileInfo configuredFileInfo(logFileName_);
    if (configuredFileInfo.isAbsolute())
    {
        return QDir::cleanPath(configuredFileInfo.absoluteFilePath());
    }

    const QString directoryPath = logDirectory_.trimmed().isEmpty()
                                      ? configDir.absolutePath()
                                      : configDir.absoluteFilePath(logDirectory_);
    return QDir::cleanPath(QDir(directoryPath).absoluteFilePath(logFileName_));
}

bool AppConfig::load(QString *errorMessage)
{
    configFilePath_ = defaultConfigPath();
    QFile file(configFilePath_);
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
    const QJsonValue logValue = root.value(QStringLiteral("log"));
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

    logAppName_ = displayName_;

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
        !readRequiredString(httpValue.toObject(),
                            QStringLiteral("logout_path"),
                            &logoutPath_,
                            errorMessage) ||
        !readRequiredString(httpValue.toObject(),
                            QStringLiteral("avatar_temp_upload_path"),
                            &avatarTempUploadPath_,
                            errorMessage) ||
        !readRequiredString(httpValue.toObject(),
                            QStringLiteral("user_avatar_path_template"),
                            &userAvatarPathTemplate_,
                            errorMessage) ||
        !readRequiredString(wsValue.toObject(),
                            QStringLiteral("url"),
                            &webSocketUrlText,
                            errorMessage))
    {
        return false;
    }

    logAppName_ = displayName_;

    if (logValue.isObject())
    {
        const QJsonObject logObject = logValue.toObject();
        QString configuredLogLevel = logMinimumLevel_;

        if (!readOptionalString(logObject,
                                QStringLiteral("app_name"),
                                &logAppName_,
                                errorMessage) ||
            !readOptionalString(logObject,
                                QStringLiteral("minimum_level"),
                                &configuredLogLevel,
                                errorMessage) ||
            !readOptionalBool(logObject,
                              QStringLiteral("enable_console"),
                              &logEnableConsole_,
                              errorMessage) ||
            !readOptionalBool(logObject,
                              QStringLiteral("enable_file"),
                              &logEnableFile_,
                              errorMessage) ||
            !readOptionalBool(logObject,
                              QStringLiteral("display_local_time"),
                              &logDisplayLocalTime_,
                              errorMessage) ||
            !readOptionalString(logObject,
                                QStringLiteral("directory"),
                                &logDirectory_,
                                errorMessage) ||
            !readOptionalString(logObject,
                                QStringLiteral("file_name"),
                                &logFileName_,
                                errorMessage))
        {
            return false;
        }

        logMinimumLevel_ = configuredLogLevel.trimmed().toUpper();
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

    if (!isSupportedLogLevel(logMinimumLevel_))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("log.minimum_level 不受支持：%1")
                    .arg(logMinimumLevel_);
        }
        return false;
    }

    if (!registerPath_.startsWith(QLatin1Char('/')) ||
        !loginPath_.startsWith(QLatin1Char('/')) ||
        !logoutPath_.startsWith(QLatin1Char('/')) ||
        !avatarTempUploadPath_.startsWith(QLatin1Char('/')) ||
        !userAvatarPathTemplate_.startsWith(QLatin1Char('/')))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral(
                    "register_path、login_path、logout_path、avatar_temp_upload_path 和 user_avatar_path_template 必须以 / 开头");
        }
        return false;
    }

    if (!userAvatarPathTemplate_.contains(QStringLiteral("{user_id}")))
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral(
                    "user_avatar_path_template 必须包含 {user_id} 占位符");
        }
        return false;
    }

    if (logAppName_.trimmed().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("log.app_name 不能为空");
        }
        return false;
    }

    if (logEnableFile_ && logFileName_.trimmed().isEmpty())
    {
        if (errorMessage)
        {
            *errorMessage = QStringLiteral("log.file_name 不能为空");
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

bool AppConfig::readOptionalString(const QJsonObject &object,
                                   const QString &key,
                                   QString *out,
                                   QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull())
    {
        return true;
    }

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

bool AppConfig::readOptionalBool(const QJsonObject &object,
                                 const QString &key,
                                 bool *out,
                                 QString *errorMessage)
{
    const QJsonValue value = object.value(key);
    if (value.isUndefined() || value.isNull())
    {
        return true;
    }

    if (!value.isBool())
    {
        if (errorMessage)
        {
            *errorMessage =
                QStringLiteral("客户端配置字段 %1 必须是布尔值").arg(key);
        }
        return false;
    }

    if (out)
    {
        *out = value.toBool();
    }
    return true;
}

bool AppConfig::isSupportedLogLevel(const QString &levelText)
{
    return levelText == QStringLiteral("DEBUG") ||
           levelText == QStringLiteral("INFO") ||
           levelText == QStringLiteral("WARN") ||
           levelText == QStringLiteral("WARNING") ||
           levelText == QStringLiteral("ERROR") ||
           levelText == QStringLiteral("CRITICAL") ||
           levelText == QStringLiteral("FATAL");
}

}  // namespace chatclient::config
