#include <QApplication>
#include <QMessageBox>

#include "config/appconfig.h"
#include "log/app_logger.h"
#include "loginwindow.h"

int main(int argc, char *argv[])
{
    // Qt 应用入口：负责初始化事件循环与应用级资源。
    QApplication app(argc, argv);

    QString configError;
    if (!chatclient::config::AppConfig::initialize(&configError)) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("ChatClient"),
                              configError);
        return 1;
    }

    QString logError;
    if (!chatclient::log::AppLogger::initialize(&logError)) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("ChatClient"),
                              logError);
        return 1;
    }

    app.setApplicationDisplayName(
        chatclient::config::AppConfig::instance().displayName());

    CHATCLIENT_LOG_INFO("bootstrap")
        << "已加载配置文件："
        << chatclient::config::AppConfig::instance().configFilePath();
    if (chatclient::config::AppConfig::instance().isFileLogEnabled()) {
        CHATCLIENT_LOG_INFO("bootstrap")
            << "已启用文件日志，路径："
            << chatclient::log::AppLogger::logFilePath();
    } else {
        CHATCLIENT_LOG_INFO("bootstrap")
            << "未启用文件日志，仅输出到控制台";
    }
    CHATCLIENT_LOG_INFO("bootstrap")
        << "HTTP 基础地址："
        << chatclient::config::AppConfig::instance().httpBaseUrlText();
    CHATCLIENT_LOG_INFO("bootstrap")
        << "WebSocket 地址："
        << chatclient::config::AppConfig::instance().webSocketUrl().toString();

    LoginWindow login;
    login.show();
    CHATCLIENT_LOG_INFO("bootstrap") << "登录窗口已显示";

    // 进入主事件循环，直到应用退出。
    return app.exec();
}
