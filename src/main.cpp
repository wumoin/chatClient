#include <QApplication>

#include "loginwindow.h"

int main(int argc, char *argv[])
{
    // Qt 应用入口：负责初始化事件循环与应用级资源。
    QApplication app(argc, argv);

    // 创建并显示登录窗口。
    LoginWindow window;
    window.show();

    // 进入主事件循环，直到应用退出。
    return app.exec();
}
