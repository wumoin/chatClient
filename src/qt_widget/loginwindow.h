#pragma once

#include <QWidget>
#include "chatwindow.h"

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QWidget;
class QMouseEvent;
class QPaintEvent;

// 登录窗口：使用 QWidget 作为顶层容器，负责组织登录界面的控件与布局。
class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    // 构造函数：完成界面搭建与基础样式设置。
    explicit LoginWindow(QWidget *parent = nullptr);

private:
    // 绘制自定义窗口边框与圆角背景。
    void paintEvent(QPaintEvent *event) override;
    // 处理窗口拖拽移动。
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    // 初始化自定义标题栏（拖拽区 + 窗口按钮）。
    QWidget *createTitleBar();

    // 登录页切换到注册页。
    void showRegisterPage();
    // 注册页切换回登录页。
    void showLoginPage();

    // 创建登录页控件并返回页面容器。
    QWidget *createLoginPage();
    // 创建注册页控件并返回页面容器。
    QWidget *createRegisterPage();

    // 标题与副标题：用于提示当前所处流程。
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    // 页面容器：在登录页与注册页之间切换。
    QStackedWidget *m_stack = nullptr;
    // 顶部标题栏（用于拖拽与放置窗口按钮）。
    QWidget *m_titleBar = nullptr;

    // 账号输入框：支持手机号/邮箱/用户名等文本。
    QLineEdit *m_accountEdit = nullptr;
    // 密码输入框：采用 Password 回显模式。
    QLineEdit *m_passwordEdit = nullptr;
    // 记住我：保存登录状态的开关（逻辑待接入）。
    QCheckBox *m_rememberCheck = nullptr;
    // 自动登录：下次启动自动登录的开关（逻辑待接入）。
    QCheckBox *m_autoLoginCheck = nullptr;
    // 登录按钮：触发登录动作（信号/槽待接入）。
    QPushButton *m_loginButton = nullptr;
    // 注册按钮：跳转注册流程（信号/槽待接入）。
    QPushButton *m_registerButton = nullptr;

    // 注册页输入框：账号。
    QLineEdit *m_registerAccountEdit = nullptr;
    // 注册页输入框：密码。
    QLineEdit *m_registerPasswordEdit = nullptr;
    // 注册页输入框：确认密码。
    QLineEdit *m_registerConfirmEdit = nullptr;
    // 注册页主按钮：提交注册。
    QPushButton *m_registerSubmitButton = nullptr;
    // 注册页返回按钮：回到登录页。
    QPushButton *m_backToLoginButton = nullptr;
    // 登录成功后打开聊天窗口。
    ChatWindow *window = nullptr;

    // 拖拽状态与偏移。
    bool m_dragging = false;
    QPoint m_dragOffset;
};
