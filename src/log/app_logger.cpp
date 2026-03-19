#include "log/app_logger.h"

#include "config/appconfig.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>

#include <cstdlib>
#include <cstdio>

namespace chatclient::log {
namespace {

/**
 * @brief 日志运行时状态。
 */
struct LogState
{
    bool initialized = false;
    bool enableConsole = true;
    bool enableFile = true;
    bool displayLocalTime = true;
    int minimumSeverity = 1;
    QString appName = QStringLiteral("chatClient");
    QString logFilePath;
    QFile file;
    QMutex mutex;
};

LogState g_logState;

/**
 * @brief 把 Qt 日志类型映射成统一的严重级别顺序。
 * @param type Qt 日志类型。
 * @return 数值越大表示级别越高。
 */
int severityRank(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return 0;
    case QtInfoMsg:
        return 1;
    case QtWarningMsg:
        return 2;
    case QtCriticalMsg:
        return 3;
    case QtFatalMsg:
        return 4;
    }

    return 1;
}

/**
 * @brief 把日志级别文本转换为统一严重级别顺序。
 * @param levelText 配置中的最小日志级别文本。
 * @return 数值越大表示级别越高。
 */
int parseMinimumSeverity(const QString &levelText)
{
    const QString normalized = levelText.trimmed().toUpper();
    if (normalized == QStringLiteral("DEBUG") ||
        normalized == QStringLiteral("调试"))
    {
        return severityRank(QtDebugMsg);
    }
    if (normalized == QStringLiteral("INFO") ||
        normalized == QStringLiteral("信息"))
    {
        return severityRank(QtInfoMsg);
    }
    if (normalized == QStringLiteral("WARN") ||
        normalized == QStringLiteral("WARNING") ||
        normalized == QStringLiteral("警告"))
    {
        return severityRank(QtWarningMsg);
    }
    if (normalized == QStringLiteral("ERROR") ||
        normalized == QStringLiteral("CRITICAL") ||
        normalized == QStringLiteral("错误"))
    {
        return severityRank(QtCriticalMsg);
    }
    if (normalized == QStringLiteral("FATAL") ||
        normalized == QStringLiteral("致命"))
    {
        return severityRank(QtFatalMsg);
    }

    return severityRank(QtInfoMsg);
}

/**
 * @brief 把 Qt 日志类型转换为统一级别文本。
 * @param type Qt 日志类型。
 * @return 统一输出格式中的级别文本。
 */
QString levelText(QtMsgType type)
{
    switch (type)
    {
    case QtDebugMsg:
        return QStringLiteral("调试");
    case QtInfoMsg:
        return QStringLiteral("信息");
    case QtWarningMsg:
        return QStringLiteral("警告");
    case QtCriticalMsg:
        return QStringLiteral("错误");
    case QtFatalMsg:
        return QStringLiteral("致命");
    }

    return QStringLiteral("信息");
}

/**
 * @brief 判断当前日志是否应该输出。
 * @param type Qt 日志类型。
 * @return true 表示达到最小日志级别；false 表示应被过滤。
 */
bool shouldLog(QtMsgType type)
{
    return severityRank(type) >= g_logState.minimumSeverity;
}

/**
 * @brief 生成统一格式的日志文本。
 * @param type Qt 日志类型。
 * @param context Qt 提供的日志上下文。
 * @param message 原始日志消息。
 * @return 单行格式化后的日志文本。
 */
QString formatLine(QtMsgType type,
                   const QMessageLogContext &context,
                   const QString &message)
{
    const QDateTime now = g_logState.displayLocalTime
                              ? QDateTime::currentDateTime()
                              : QDateTime::currentDateTimeUtc();
    const QString timestamp =
        now.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));

    QString line =
        QStringLiteral("%1 %2 [%3] %4")
            .arg(timestamp, levelText(type), g_logState.appName, message);

    if (context.file != nullptr && context.line > 0)
    {
        line += QStringLiteral(" (%1:%2)")
                    .arg(QString::fromUtf8(context.file))
                    .arg(context.line);
    }

    return line;
}

/**
 * @brief 实际处理 Qt 全局日志消息。
 * @param type Qt 日志类型。
 * @param context Qt 日志上下文。
 * @param message 原始日志消息。
 */
void messageHandler(QtMsgType type,
                    const QMessageLogContext &context,
                    const QString &message)
{
    if (!shouldLog(type))
    {
        return;
    }

    const QString line = formatLine(type, context, message);
    const QByteArray bytes = line.toUtf8();

    QMutexLocker locker(&g_logState.mutex);

    if (g_logState.enableConsole)
    {
        FILE *stream = (type == QtWarningMsg || type == QtCriticalMsg ||
                        type == QtFatalMsg)
                           ? stderr
                           : stdout;
        std::fputs(bytes.constData(), stream);
        std::fputc('\n', stream);
        std::fflush(stream);
    }

    if (g_logState.enableFile && g_logState.file.isOpen())
    {
        g_logState.file.write(bytes);
        g_logState.file.write("\n");
        g_logState.file.flush();
    }

    if (type == QtFatalMsg)
    {
        std::abort();
    }
}

}  // namespace

bool AppLogger::initialize(QString *errorMessage)
{
    if (g_logState.initialized)
    {
        return true;
    }

    const auto &config = chatclient::config::AppConfig::instance();

    g_logState.appName = config.logAppName();
    g_logState.enableConsole = config.isConsoleLogEnabled();
    g_logState.enableFile = config.isFileLogEnabled();
    g_logState.displayLocalTime = config.displayLocalTimeInLog();
    g_logState.minimumSeverity = parseMinimumSeverity(config.logMinimumLevel());
    g_logState.logFilePath = config.resolvedLogFilePath();

    if (g_logState.enableFile)
    {
        const QFileInfo fileInfo(g_logState.logFilePath);
        QDir directory = fileInfo.dir();
        if (!directory.exists() && !directory.mkpath(QStringLiteral(".")))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法创建日志目录：%1")
                                    .arg(directory.absolutePath());
            }
            return false;
        }

        g_logState.file.setFileName(g_logState.logFilePath);
        if (!g_logState.file.open(QIODevice::WriteOnly |
                                  QIODevice::Append |
                                  QIODevice::Text))
        {
            if (errorMessage)
            {
                *errorMessage = QStringLiteral("无法打开日志文件：%1")
                                    .arg(g_logState.logFilePath);
            }
            return false;
        }
    }

    qInstallMessageHandler(messageHandler);
    g_logState.initialized = true;
    return true;
}

bool AppLogger::isInitialized()
{
    return g_logState.initialized;
}

QString AppLogger::logFilePath()
{
    return g_logState.logFilePath;
}

}  // namespace chatclient::log
