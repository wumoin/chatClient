#pragma once

#include "dto/auth_dto.h"

#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QWidget;
class QCloseEvent;
class QMouseEvent;
class QPaintEvent;
class ChatWindow;

namespace chatclient::service {
class AuthService;
}

// 登录窗口：使用 QWidget 作为顶层容器，负责组织登录界面的控件与布局。
class LoginWindow : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief 表单状态提示类型。
     */
    enum class StatusTone
    {
        kInfo,
        kSuccess,
        kError,
    };

    /**
     * @brief 构造登录窗口并初始化登录/注册页面。
     * @param parent 父级 QWidget，可为空。
     */
    explicit LoginWindow(QWidget *parent = nullptr);

private:
    /**
     * @brief 绘制窗口背景与圆角边框。
     * @param event 绘制事件对象。
     */
    void paintEvent(QPaintEvent *event) override;
    /**
     * @brief 在应用退出时执行最后的登出清理。
     * @param event 关闭事件对象。
     */
    void closeEvent(QCloseEvent *event) override;
    /**
     * @brief 处理鼠标按下事件并记录拖拽起点。
     * @param event 鼠标事件对象。
     */
    void mousePressEvent(QMouseEvent *event) override;
    /**
     * @brief 处理鼠标移动事件并在拖拽时移动窗口。
     * @param event 鼠标事件对象。
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    /**
     * @brief 处理鼠标释放事件并结束拖拽状态。
     * @param event 鼠标事件对象。
     */
    void mouseReleaseEvent(QMouseEvent *event) override;

    /**
     * @brief 创建自定义标题栏及窗口控制按钮。
     * @return 标题栏容器指针。
     */
    QWidget *createTitleBar();

    /**
     * @brief 切换到注册页面并刷新标题提示文案。
     */
    void showRegisterPage();
    /**
     * @brief 切换回登录页面并恢复标题提示文案。
     */
    void showLoginPage();

    /**
     * @brief 创建登录页控件树。
     * @return 登录页容器指针。
     */
    QWidget *createLoginPage();
    /**
     * @brief 创建注册页控件树。
     * @return 注册页容器指针。
     */
    QWidget *createRegisterPage();

    /**
     * @brief 提交登录表单。
     */
    void handleLoginSubmit();
    /**
     * @brief 处理登录成功结果。
     * @param session 当前保存到客户端的登录会话。
     */
    void handleLoginSucceeded(
        const chatclient::dto::auth::LoginSessionDto &session);
    /**
     * @brief 处理登录失败结果。
     * @param message 可直接展示给用户的错误提示。
     */
    void handleLoginFailed(const QString &message);

    /**
     * @brief 响应聊天页发起的切换账号动作。
     */
    void handleSwitchAccountRequested();
    /**
     * @brief 处理切换账号成功结果。
     */
    void handleSwitchAccountSucceeded();
    /**
     * @brief 处理切换账号失败结果。
     * @param message 可直接展示给用户的错误提示。
     */
    void handleSwitchAccountFailed(const QString &message);

    /**
     * @brief 响应聊天页发起的“登出并退出程序”动作。
     */
    void handleSignOutRequested();

    /**
     * @brief 提交注册表单。
     */
    void handleRegisterSubmit();
    /**
     * @brief 处理注册成功结果。
     * @param user 服务端返回的新用户信息。
     */
    void handleRegisterSucceeded(
        const chatclient::dto::auth::RegisterUserDto &user);
    /**
     * @brief 处理注册失败结果。
     * @param message 可直接展示给用户的错误提示。
     */
    void handleRegisterFailed(const QString &message);
    /**
     * @brief 统一切换登录页控件的忙碌状态。
     * @param submitting true 表示登录请求提交中；false 表示恢复可编辑。
     */
    void setLoginSubmitting(bool submitting);
    /**
     * @brief 设置登录页状态提示文案和颜色。
     * @param message 需要展示的状态文本；为空时隐藏提示。
     * @param tone 当前提示语气，例如信息、成功或错误。
     */
    void setLoginStatusMessage(const QString &message, StatusTone tone);
    /**
     * @brief 统一切换注册页控件的忙碌状态。
     * @param submitting true 表示注册请求提交中；false 表示恢复可编辑。
     */
    void setRegisterSubmitting(bool submitting);
    /**
     * @brief 设置注册页状态提示文案和颜色。
     * @param message 需要展示的状态文本；为空时隐藏提示。
     * @param tone 当前提示语气，例如信息、成功或错误。
     */
    void setRegisterStatusMessage(const QString &message, StatusTone tone);

    /**
     * @brief 打开聊天窗口并把当前登录用户信息灌入界面。
     * @param session 当前登录会话。
     */
    void openChatWindow(const chatclient::dto::auth::LoginSessionDto &session);

    /**
     * @brief 在程序退出前同步向服务端发送登出请求。
     * @param showConfirmation true 表示先弹确认框；false 表示静默执行。
     * @param dialogParent 确认框的父窗口，可为空。
     * @return true 表示允许继续退出；false 表示用户取消退出。
     */
    bool performApplicationExitLogout(bool showConfirmation,
                                      QWidget *dialogParent = nullptr);

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
    // 记住我：当前仅保留界面入口，后续再接更细的记住策略。
    QCheckBox *m_rememberCheck = nullptr;
    // 自动登录：当前仅保留界面入口，后续再接启动恢复策略。
    QCheckBox *m_autoLoginCheck = nullptr;
    // 登录按钮：触发真实登录请求。
    QPushButton *m_loginButton = nullptr;
    // 注册按钮：跳转注册流程。
    QPushButton *m_registerButton = nullptr;
    // 登录页状态提示：显示“登录中 / 成功 / 失败”等反馈。
    QLabel *m_loginStatusLabel = nullptr;

    // 注册页输入框：账号。
    QLineEdit *m_registerAccountEdit = nullptr;
    // 注册页输入框：昵称。
    QLineEdit *m_registerNicknameEdit = nullptr;
    // 注册页输入框：密码。
    QLineEdit *m_registerPasswordEdit = nullptr;
    // 注册页输入框：确认密码。
    QLineEdit *m_registerConfirmEdit = nullptr;
    // 注册页状态提示：显示“注册中 / 成功 / 失败”等反馈。
    QLabel *m_registerStatusLabel = nullptr;
    // 注册页主按钮：提交注册。
    QPushButton *m_registerSubmitButton = nullptr;
    // 注册页返回按钮：回到登录页。
    QPushButton *m_backToLoginButton = nullptr;
    // 认证业务服务：负责注册 / 登录请求校验与 HTTP 调用。
    chatclient::service::AuthService *m_authService = nullptr;
    // 聊天窗口：登录成功后展示。
    ChatWindow *m_chatWindow = nullptr;

    // 拖拽状态与偏移。
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_applicationShutdownInProgress = false;
};
