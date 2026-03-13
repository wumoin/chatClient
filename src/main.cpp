#include <QApplication>
#include <QMessageBox>

#include "config/appconfig.h"
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

    app.setApplicationDisplayName(
        chatclient::config::AppConfig::instance().displayName());

    LoginWindow login;
    login.show();

    // 进入主事件循环，直到应用退出。
    return app.exec();
}
