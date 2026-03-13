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
        << "config loaded from "
        << chatclient::config::AppConfig::instance().configFilePath();
    if (chatclient::config::AppConfig::instance().isFileLogEnabled()) {
        CHATCLIENT_LOG_INFO("bootstrap")
            << "file log enabled at "
            << chatclient::log::AppLogger::logFilePath();
    } else {
        CHATCLIENT_LOG_INFO("bootstrap")
            << "file log disabled, console logging only";
    }
    CHATCLIENT_LOG_INFO("bootstrap")
        << "HTTP base URL "
        << chatclient::config::AppConfig::instance().httpBaseUrlText();
    CHATCLIENT_LOG_INFO("bootstrap")
        << "WebSocket URL "
        << chatclient::config::AppConfig::instance().webSocketUrl().toString();

    LoginWindow login;
    login.show();
    CHATCLIENT_LOG_INFO("bootstrap") << "login window shown";

    // 进入主事件循环，直到应用退出。
    return app.exec();
}
