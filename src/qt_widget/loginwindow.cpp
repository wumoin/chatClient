#include "loginwindow.h"

#include "config/appconfig.h"
#include "service/auth_service.h"

#include <QCheckBox>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#endif


static QString loadGlobalStyle()
{
    QFile file(QStringLiteral(":/loginwindow.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    const auto &config = chatclient::config::AppConfig::instance();
    m_authService = new chatclient::service::AuthService(this);

    // 窗口基础设置：标题与固定尺寸，保证布局稳定。
    setWindowTitle(config.loginWindowTitle());
    setFixedSize(420, 520);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, true);

    // 根布局：标题栏贴顶，其余内容放到内部容器里设置边距。
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // 应用全局 QSS：统一输入框与勾选框风格，跨平台一致。
    setStyleSheet(loadGlobalStyle());

    // 自定义标题栏：用于窗口拖拽与窗口按钮。
    m_titleBar = createTitleBar();

    // 标题与副标题：在登录/注册之间切换时更新文本。
    m_titleLabel = new QLabel(config.displayName(), this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName(QStringLiteral("titleLabel"));

    m_subtitleLabel = new QLabel(QStringLiteral("欢迎回来，请登录"), this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setObjectName(QStringLiteral("subtitleLabel"));

    // 页面容器：使用栈式布局在登录页/注册页之间切换。
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createLoginPage());
    m_stack->addWidget(createRegisterPage());
    m_stack->setCurrentIndex(0);

    // 内容容器：给标题与表单区域提供内边距。
    auto *content = new QWidget(this);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(18, 12, 18, 18);
    contentLayout->setSpacing(12);

    // 组装整体布局结构。
    rootLayout->addWidget(m_titleBar);
    contentLayout->addWidget(m_titleLabel);
    contentLayout->addWidget(m_subtitleLabel);
    contentLayout->addWidget(m_stack);
    contentLayout->addStretch(1);
    rootLayout->addWidget(content);

    // 当前先把客户端注册功能接入真实 HTTP 接口。
    // 登录功能尚未实现，因此登录页仍保留原型行为，注册页则通过 AuthService 走完整请求链路。
    connect(m_authService,
            &chatclient::service::AuthService::registerStarted,
            this,
            [this]() {
                setRegisterSubmitting(true);
                setRegisterStatusMessage(QStringLiteral("正在提交注册请求..."),
                                         RegisterStatusTone::kInfo);
            });
    connect(m_authService,
            &chatclient::service::AuthService::registerSucceeded,
            this,
            &LoginWindow::handleRegisterSucceeded);
    connect(m_authService,
            &chatclient::service::AuthService::registerFailed,
            this,
            &LoginWindow::handleRegisterFailed);
}

void LoginWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    // 绘制窗口主体：圆角背景 + 细边框，避免系统边框带来的粗糙感。
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF rect = this->rect().adjusted(1.0, 1.0, -1.0, -1.0);
    const qreal radius = 16.0;

    painter.setBrush(QColor(250, 251, 253));
    painter.setPen(QPen(QColor(220, 224, 230), 1.0));
    painter.drawRoundedRect(rect, radius, radius);
}

void LoginWindow::mousePressEvent(QMouseEvent *event)
{
    // 只允许在标题栏范围内拖拽窗口。
    if (event->button() == Qt::LeftButton && m_titleBar && m_titleBar->geometry().contains(event->pos())) {
        m_dragging = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void LoginWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragOffset);
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void LoginWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

QWidget *LoginWindow::createTitleBar()
{
    // 标题栏：左侧拖拽区域 + 右侧窗口按钮。
    auto *bar = new QWidget(this);
    bar->setObjectName(QStringLiteral("titleBar"));
    bar->setFixedHeight(36);

    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(6, 4, 6, 4);
    layout->setSpacing(6);


    auto *minimizeBtn = new QPushButton(QStringLiteral("—"), bar);
    minimizeBtn->setToolTip(QStringLiteral("最小化"));
    auto *closeBtn = new QPushButton(QStringLiteral("×"), bar);
    closeBtn->setToolTip(QStringLiteral("关闭"));

    layout->addStretch(1);
    layout->addWidget(minimizeBtn);
    layout->addWidget(closeBtn);

    connect(minimizeBtn, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::close);

    return bar;
}

void LoginWindow::showRegisterPage()
{
    // 切换到注册页面，同时更新标题文本以提示当前流程。
    if (!m_stack) {
        return;
    }
    m_stack->setCurrentIndex(1);
    m_titleLabel->setText(QStringLiteral("创建账号"));
    m_subtitleLabel->setText(QStringLiteral("填写信息以注册新账号"));
    setRegisterStatusMessage(QString(), RegisterStatusTone::kInfo);
}

void LoginWindow::showLoginPage()
{
    // 切换回登录页面，同时恢复标题文本。
    if (!m_stack) {
        return;
    }
    m_stack->setCurrentIndex(0);
    m_titleLabel->setText(
        chatclient::config::AppConfig::instance().displayName());
    m_subtitleLabel->setText(QStringLiteral("欢迎回来，请登录"));
    setRegisterSubmitting(false);
    setRegisterStatusMessage(QString(), RegisterStatusTone::kInfo);
}

QWidget *LoginWindow::createLoginPage()
{
    // 登录页容器：卡片式布局承载登录表单。
    auto *card = new QFrame(m_stack);
    card->setProperty("panel", QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 22, 22, 22);
    cardLayout->setSpacing(12);

    // 账号输入区：标签 + 输入框。
    m_accountEdit = new QLineEdit(card);
    m_accountEdit->setPlaceholderText(QStringLiteral("用户名"));

    // 密码输入区：标签 + 密码框（隐藏输入）。
    m_passwordEdit = new QLineEdit(card);
    m_passwordEdit->setPlaceholderText(QStringLiteral("请输入密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);

    // 选项区：记住我 / 自动登录。
    auto *optionsLayout = new QHBoxLayout();
    optionsLayout->setSpacing(12);
    m_rememberCheck = new QCheckBox(QStringLiteral("记住我"), card);
    m_autoLoginCheck = new QCheckBox(QStringLiteral("自动登录"), card);
    optionsLayout->addWidget(m_rememberCheck);
    optionsLayout->addWidget(m_autoLoginCheck);
    optionsLayout->addStretch(1);


    // 主操作按钮：登录。
    m_loginButton = new QPushButton(QStringLiteral("登录"), card);
    m_loginButton->setMinimumHeight(40);
    m_loginButton->setProperty("variant", QStringLiteral("primary"));

    connect(m_loginButton,
            &QPushButton::clicked,
            this,
            [this]() {
                QMessageBox::information(
                    this,
                    QStringLiteral("功能未完成"),
                    QStringLiteral("当前客户端已接入真实注册功能，登录功能仍在开发中。"));
            });

    
    // 辅助操作区：忘记密码提示 + 注册入口。
    auto *helperLayout = new QHBoxLayout();
    m_registerButton = new QPushButton(QStringLiteral("注册新账号"), card);
    m_registerButton->setFlat(true);
    m_registerButton->setProperty("variant", QStringLiteral("link"));
    helperLayout->addStretch(1);
    helperLayout->addWidget(m_registerButton);

    // 将表单控件按顺序加入卡片布局。
    cardLayout->addWidget(m_accountEdit);
    cardLayout->addWidget(m_passwordEdit);
    cardLayout->addLayout(optionsLayout);
    cardLayout->addWidget(m_loginButton);
    cardLayout->addLayout(helperLayout);

    // 点击“注册新账号”时切换到注册页。
    connect(m_registerButton, &QPushButton::clicked, this, &LoginWindow::showRegisterPage);

    return card;
}

QWidget *LoginWindow::createRegisterPage()
{
    // 注册页容器：卡片式布局承载注册表单。
    auto *card = new QFrame(m_stack);
    card->setProperty("panel", QStringLiteral("card"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 22, 22, 22);
    cardLayout->setSpacing(12);

    // 注册账号输入区。
    m_registerAccountEdit = new QLineEdit(card);
    m_registerAccountEdit->setPlaceholderText(QStringLiteral("用户名"));

    // 注册昵称输入区。
    m_registerNicknameEdit = new QLineEdit(card);
    m_registerNicknameEdit->setPlaceholderText(QStringLiteral("昵称"));

    // 密码输入区。
    m_registerPasswordEdit = new QLineEdit(card);
    m_registerPasswordEdit->setPlaceholderText(QStringLiteral("设置登录密码"));
    m_registerPasswordEdit->setEchoMode(QLineEdit::Password);

    // 确认密码输入区。
    m_registerConfirmEdit = new QLineEdit(card);
    m_registerConfirmEdit->setPlaceholderText(QStringLiteral("再次输入密码"));
    m_registerConfirmEdit->setEchoMode(QLineEdit::Password);

    // 注册提交按钮：用于提交注册信息（逻辑待接入）。
    m_registerSubmitButton = new QPushButton(QStringLiteral("注册"), card);
    m_registerSubmitButton->setMinimumHeight(40);
    m_registerSubmitButton->setProperty("variant", QStringLiteral("primary"));

    // 状态提示区：统一展示本地校验、服务端校验和网络错误。
    m_registerStatusLabel = new QLabel(card);
    m_registerStatusLabel->setObjectName(QStringLiteral("registerStatusLabel"));
    m_registerStatusLabel->setWordWrap(true);
    m_registerStatusLabel->setVisible(false);

    // 返回登录入口：已注册用户可快速切回登录页。
    auto *backLayout = new QHBoxLayout();
    auto *hintLabel = new QLabel(QStringLiteral("已有账号？"), card);
    hintLabel->setProperty("textRole", QStringLiteral("hint"));
    m_backToLoginButton = new QPushButton(QStringLiteral("返回登录"), card);
    m_backToLoginButton->setFlat(true);
    m_backToLoginButton->setProperty("variant", QStringLiteral("link"));
    backLayout->addStretch(1);
    backLayout->addWidget(hintLabel);
    backLayout->addWidget(m_backToLoginButton);

    // 将注册控件按顺序加入卡片布局。
    cardLayout->addWidget(m_registerAccountEdit);
    cardLayout->addWidget(m_registerNicknameEdit);
    cardLayout->addWidget(m_registerPasswordEdit);
    cardLayout->addWidget(m_registerConfirmEdit);
    cardLayout->addWidget(m_registerSubmitButton);
    cardLayout->addWidget(m_registerStatusLabel);
    cardLayout->addLayout(backLayout);

    // 提交注册时走 AuthService，不再停留在纯 UI 原型。
    connect(m_registerSubmitButton,
            &QPushButton::clicked,
            this,
            &LoginWindow::handleRegisterSubmit);
    connect(m_registerConfirmEdit,
            &QLineEdit::returnPressed,
            this,
            &LoginWindow::handleRegisterSubmit);

    // 点击“返回登录”时切换回登录页。
    connect(m_backToLoginButton, &QPushButton::clicked, this, &LoginWindow::showLoginPage);

    return card;
}

void LoginWindow::handleRegisterSubmit()
{
    if (!m_authService) {
        return;
    }

    QString errorMessage;
    if (!m_authService->registerUser(m_registerAccountEdit ? m_registerAccountEdit->text() : QString(),
                                     m_registerNicknameEdit ? m_registerNicknameEdit->text() : QString(),
                                     m_registerPasswordEdit ? m_registerPasswordEdit->text() : QString(),
                                     m_registerConfirmEdit ? m_registerConfirmEdit->text() : QString(),
                                     &errorMessage)) {
        setRegisterStatusMessage(errorMessage, RegisterStatusTone::kError);
        return;
    }
}

void LoginWindow::handleRegisterSucceeded(
    const chatclient::dto::auth::RegisterUserDto &user)
{
    setRegisterSubmitting(false);
    setRegisterStatusMessage(QStringLiteral("注册成功，请返回登录页继续。"),
                             RegisterStatusTone::kSuccess);

    if (m_accountEdit) {
        m_accountEdit->setText(user.account);
    }
    if (m_passwordEdit) {
        m_passwordEdit->clear();
    }
    if (m_registerAccountEdit) {
        m_registerAccountEdit->clear();
    }
    if (m_registerNicknameEdit) {
        m_registerNicknameEdit->clear();
    }
    if (m_registerPasswordEdit) {
        m_registerPasswordEdit->clear();
    }
    if (m_registerConfirmEdit) {
        m_registerConfirmEdit->clear();
    }

    showLoginPage();
    QMessageBox::information(
        this,
        QStringLiteral("注册成功"),
        QStringLiteral("账号 %1 已创建成功。当前客户端登录接口仍在开发中，请稍后使用登录功能。")
            .arg(user.account));
}

void LoginWindow::handleRegisterFailed(const QString &message)
{
    setRegisterSubmitting(false);
    setRegisterStatusMessage(message, RegisterStatusTone::kError);
}

void LoginWindow::setRegisterSubmitting(bool submitting)
{
    if (m_registerAccountEdit) {
        m_registerAccountEdit->setEnabled(!submitting);
    }
    if (m_registerNicknameEdit) {
        m_registerNicknameEdit->setEnabled(!submitting);
    }
    if (m_registerPasswordEdit) {
        m_registerPasswordEdit->setEnabled(!submitting);
    }
    if (m_registerConfirmEdit) {
        m_registerConfirmEdit->setEnabled(!submitting);
    }
    if (m_registerSubmitButton) {
        m_registerSubmitButton->setEnabled(!submitting);
    }
    if (m_backToLoginButton) {
        m_backToLoginButton->setEnabled(!submitting);
    }
}

void LoginWindow::setRegisterStatusMessage(const QString &message,
                                           RegisterStatusTone tone)
{
    if (!m_registerStatusLabel) {
        return;
    }

    if (message.isEmpty()) {
        m_registerStatusLabel->clear();
        m_registerStatusLabel->setVisible(false);
        return;
    }

    QString toneText = QStringLiteral("info");
    if (tone == RegisterStatusTone::kSuccess) {
        toneText = QStringLiteral("success");
    } else if (tone == RegisterStatusTone::kError) {
        toneText = QStringLiteral("error");
    }

    m_registerStatusLabel->setProperty("statusTone", toneText);
    m_registerStatusLabel->setText(message);
    m_registerStatusLabel->setVisible(true);
    m_registerStatusLabel->style()->unpolish(m_registerStatusLabel);
    m_registerStatusLabel->style()->polish(m_registerStatusLabel);
}
