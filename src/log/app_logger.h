#pragma once

#include <QDebug>
#include <QString>
#include <QtGlobal>

namespace chatclient::log {

/**
 * @brief 客户端统一日志模块。
 *
 * 作用：
 * 1. 读取 `AppConfig` 中的日志配置；
 * 2. 接管 Qt 全局消息处理器，把 Qt 框架日志和业务日志走同一套输出；
 * 3. 统一控制台 / 文件输出格式，避免日志散落在不同地方各写一套。
 */
class AppLogger
{
public:
    /**
     * @brief 初始化统一日志模块。
     * @param errorMessage 初始化失败时写入的错误消息，可为空。
     * @return true 表示日志模块初始化成功；false 表示配置非法或日志文件无法打开。
     */
    static bool initialize(QString *errorMessage = nullptr);

    /**
     * @brief 判断日志模块是否已经初始化完成。
     * @return true 表示已初始化；false 表示尚未初始化。
     */
    static bool isInitialized();

    /**
     * @brief 返回当前实际使用的日志文件路径。
     * @return 启用文件日志时返回绝对路径；否则返回空字符串。
     */
    static QString logFilePath();
};

}  // namespace chatclient::log

#define CHATCLIENT_LOG_DEBUG(component) \
    qDebug().noquote().nospace() << "[" << component << "] "

#define CHATCLIENT_LOG_INFO(component) \
    qInfo().noquote().nospace() << "[" << component << "] "

#define CHATCLIENT_LOG_WARN(component) \
    qWarning().noquote().nospace() << "[" << component << "] "

#define CHATCLIENT_LOG_ERROR(component) \
    qCritical().noquote().nospace() << "[" << component << "] "
