#include <QApplication>

#include "chatwindow.h"
#include "loginwindow.h"

int main(int argc, char *argv[])
{
    // Qt 应用入口：负责初始化事件循环与应用级资源。
    QApplication app(argc, argv);

    LoginWindow login;
    login.show();

    // 进入主事件循环，直到应用退出。
    return app.exec();
}
