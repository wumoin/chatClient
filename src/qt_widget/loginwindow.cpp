#include "loginwindow.h"

#include "api/user_api_client.h"
#include "config/appconfig.h"
#include "log/app_logger.h"
#include "qt_widget/chatwindow.h"
#include "service/auth_service.h"
#include "service/user_error_localizer.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QFileDialog>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
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

// LoginWindow 承担客户端的登录 / 注册入口界面，并负责在成功认证后
// 打开 ChatWindow。这里还额外接入了注册头像上传，因此它既要处理
// 常规表单验证，也要处理少量图片预览和上传状态反馈。

static QString loadGlobalStyle()
{
    QFile file(QStringLiteral(":/loginwindow.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

static QPixmap createRoundedAvatarPixmap(const QImage &image, const QSize &size)
{
    if (image.isNull() || !size.isValid())
    {
        return QPixmap();
    }

    const QImage scaledImage = image.scaled(size,
                                            Qt::KeepAspectRatioByExpanding,
                                            Qt::SmoothTransformation);
    QPixmap roundedPixmap(size);
    roundedPixmap.fill(Qt::transparent);

    QPainter painter(&roundedPixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QPainterPath path;
    path.addEllipse(roundedPixmap.rect());
    painter.setClipPath(path);
    painter.drawImage(QRect(QPoint(0, 0), size), scaledImage);
    return roundedPixmap;
}

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    const auto &config = chatclient::config::AppConfig::instance();
    m_authService = new chatclient::service::AuthService(this);
    m_userApiClient = new chatclient::api::UserApiClient(this);

    // 窗口基础设置：标题与固定尺寸，保证布局稳定。
    setWindowTitle(config.loginWindowTitle());
    setFixedSize(420, 600);
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
    resetRegisterAvatarState();

    // 显式指定键盘 Tab 焦点顺序，避免新增头像设置区后仍按旧的控件创建顺序跳转。
    QWidget::setTabOrder(m_accountEdit, m_passwordEdit);
    QWidget::setTabOrder(m_passwordEdit, m_rememberCheck);
    QWidget::setTabOrder(m_rememberCheck, m_autoLoginCheck);
    QWidget::setTabOrder(m_autoLoginCheck, m_loginButton);
    QWidget::setTabOrder(m_loginButton, m_registerButton);

    QWidget::setTabOrder(m_registerAvatarSelectButton, m_registerAccountEdit);
    QWidget::setTabOrder(m_registerAccountEdit, m_registerNicknameEdit);
    QWidget::setTabOrder(m_registerNicknameEdit, m_registerPasswordEdit);
    QWidget::setTabOrder(m_registerPasswordEdit, m_registerConfirmEdit);
    QWidget::setTabOrder(m_registerConfirmEdit, m_registerSubmitButton);
    QWidget::setTabOrder(m_registerSubmitButton, m_backToLoginButton);

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

    // 当前登录和注册都已经接入真实认证 service。
    // 窗口层只负责收集表单、切换提交态和响应最终结果。
    connect(m_authService,
            &chatclient::service::AuthService::loginStarted,
            this,
            [this]() {
                setLoginSubmitting(true);
                setLoginStatusMessage(QStringLiteral("正在提交登录请求..."),
                                      StatusTone::kInfo);
            });
    connect(m_authService,
            &chatclient::service::AuthService::loginSucceeded,
            this,
            &LoginWindow::handleLoginSucceeded);
    connect(m_authService,
            &chatclient::service::AuthService::loginFailed,
            this,
            &LoginWindow::handleLoginFailed);
    connect(m_authService,
            &chatclient::service::AuthService::logoutStarted,
            this,
            [this]() {
                if (m_chatWindow) {
                    m_chatWindow->setSessionActionSubmitting(true, false);
                }
            });
    connect(m_authService,
            &chatclient::service::AuthService::logoutSucceeded,
            this,
            &LoginWindow::handleSwitchAccountSucceeded);
    connect(m_authService,
            &chatclient::service::AuthService::logoutFailed,
            this,
            &LoginWindow::handleSwitchAccountFailed);
    connect(m_authService,
            &chatclient::service::AuthService::registerStarted,
            this,
            [this]() {
                setRegisterSubmitting(true);
                setRegisterStatusMessage(QStringLiteral("正在提交注册请求..."),
                                         StatusTone::kInfo);
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

void LoginWindow::closeEvent(QCloseEvent *event)
{
    if (m_applicationShutdownInProgress)
    {
        QWidget::closeEvent(event);
        return;
    }

    if (m_authService && m_authService->hasActiveSession())
    {
        performApplicationExitLogout(false, this);
    }

    QWidget::closeEvent(event);
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
    CHATCLIENT_LOG_INFO("login.window") << "切换到注册页";
    m_stack->setCurrentIndex(1);
    m_titleLabel->setText(QStringLiteral("创建账号"));
    m_subtitleLabel->setText(QStringLiteral("填写信息以注册新账号"));
    setLoginSubmitting(false);
    setLoginStatusMessage(QString(), StatusTone::kInfo);
    setRegisterStatusMessage(QString(), StatusTone::kInfo);
}

void LoginWindow::showLoginPage()
{
    // 切换回登录页面，同时恢复标题文本。
    if (!m_stack) {
        return;
    }
    CHATCLIENT_LOG_INFO("login.window") << "切换到登录页";
    m_stack->setCurrentIndex(0);
    m_titleLabel->setText(
        chatclient::config::AppConfig::instance().displayName());
    m_subtitleLabel->setText(QStringLiteral("欢迎回来，请登录"));
    setLoginSubmitting(false);
    setLoginStatusMessage(QString(), StatusTone::kInfo);
    setRegisterSubmitting(false);
    setRegisterStatusMessage(QString(), StatusTone::kInfo);
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

    // 状态提示区：统一展示本地校验、服务端校验和网络错误。
    m_loginStatusLabel = new QLabel(card);
    m_loginStatusLabel->setObjectName(QStringLiteral("authStatusLabel"));
    m_loginStatusLabel->setWordWrap(true);
    m_loginStatusLabel->setVisible(false);

    
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
    cardLayout->addWidget(m_loginStatusLabel);
    cardLayout->addLayout(helperLayout);

    // 点击“登录”时走真实认证链路。
    connect(m_loginButton,
            &QPushButton::clicked,
            this,
            &LoginWindow::handleLoginSubmit);
    connect(m_passwordEdit,
            &QLineEdit::returnPressed,
            this,
            &LoginWindow::handleLoginSubmit);
    connect(m_accountEdit,
            &QLineEdit::returnPressed,
            this,
            &LoginWindow::handleLoginSubmit);

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

    // 头像设置区：用户选择图片后会立即调用临时头像上传接口。
    auto *avatarLayout = new QHBoxLayout();
    avatarLayout->setSpacing(12);

    m_registerAvatarPreviewLabel = new QLabel(QStringLiteral("头像"), card);
    m_registerAvatarPreviewLabel->setObjectName(QStringLiteral("avatarPickerPreview"));
    m_registerAvatarPreviewLabel->setAlignment(Qt::AlignCenter);
    m_registerAvatarPreviewLabel->setFixedSize(72, 72);

    auto *avatarMetaLayout = new QVBoxLayout();
    avatarMetaLayout->setSpacing(6);
    m_registerAvatarSelectButton = new QPushButton(QStringLiteral("设置头像"), card);
    m_registerAvatarSelectButton->setObjectName(QStringLiteral("avatarPickerButton"));
    m_registerAvatarHintLabel = new QLabel(QStringLiteral("可选。选择图片后会立即上传临时头像"), card);
    m_registerAvatarHintLabel->setObjectName(QStringLiteral("avatarHintLabel"));
    m_registerAvatarHintLabel->setWordWrap(true);
    avatarMetaLayout->addWidget(m_registerAvatarSelectButton, 0, Qt::AlignLeft);
    avatarMetaLayout->addWidget(m_registerAvatarHintLabel);
    avatarMetaLayout->addStretch(1);

    avatarLayout->addWidget(m_registerAvatarPreviewLabel);
    avatarLayout->addLayout(avatarMetaLayout, 1);

    // 密码输入区。
    m_registerPasswordEdit = new QLineEdit(card);
    m_registerPasswordEdit->setPlaceholderText(QStringLiteral("设置登录密码"));
    m_registerPasswordEdit->setEchoMode(QLineEdit::Password);

    // 确认密码输入区。
    m_registerConfirmEdit = new QLineEdit(card);
    m_registerConfirmEdit->setPlaceholderText(QStringLiteral("再次输入密码"));
    m_registerConfirmEdit->setEchoMode(QLineEdit::Password);

    // 注册提交按钮：用于提交注册信息。
    m_registerSubmitButton = new QPushButton(QStringLiteral("注册"), card);
    m_registerSubmitButton->setMinimumHeight(40);
    m_registerSubmitButton->setProperty("variant", QStringLiteral("primary"));

    // 状态提示区：统一展示本地校验、服务端校验和网络错误。
    m_registerStatusLabel = new QLabel(card);
    m_registerStatusLabel->setObjectName(QStringLiteral("authStatusLabel"));
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
    // 头像设置放在第一项，和当前注册需求一致，同时避免中段信息过密。
    cardLayout->addLayout(avatarLayout);
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
    connect(m_registerAvatarSelectButton,
            &QPushButton::clicked,
            this,
            &LoginWindow::handleRegisterAvatarSelect);

    // 点击“返回登录”时切换回登录页。
    connect(m_backToLoginButton, &QPushButton::clicked, this, &LoginWindow::showLoginPage);

    return card;
}

void LoginWindow::handleLoginSubmit()
{
    if (!m_authService) {
        return;
    }

    // 窗口层的职责很薄：收集表单 -> 调 AuthService -> 只在同步校验失败时就地提示。
    // 真正的网络请求和登录态持久化都已经在 AuthService 里收口。
    CHATCLIENT_LOG_INFO("login.window")
        << "点击登录，account="
        << (m_accountEdit ? m_accountEdit->text() : QString());

    QString errorMessage;
    if (!m_authService->loginUser(
            m_accountEdit ? m_accountEdit->text() : QString(),
            m_passwordEdit ? m_passwordEdit->text() : QString(),
            &errorMessage)) {
        CHATCLIENT_LOG_WARN("login.window")
            << "登录提交被拒绝，message="
            << errorMessage;
        setLoginStatusMessage(errorMessage, StatusTone::kError);
        return;
    }
}

void LoginWindow::handleLoginSucceeded(
    const chatclient::dto::auth::LoginSessionDto &session)
{
    CHATCLIENT_LOG_INFO("login.window")
        << "登录成功，user_id="
        << session.user.userId
        << " device_session_id="
        << session.deviceSessionId;
    setLoginSubmitting(false);
    setLoginStatusMessage(QStringLiteral("登录成功，正在进入聊天页..."),
                          StatusTone::kSuccess);
    openChatWindow(session);
}

void LoginWindow::handleLoginFailed(const QString &message)
{
    CHATCLIENT_LOG_WARN("login.window")
        << "登录失败，message="
        << message;
    setLoginSubmitting(false);
    setLoginStatusMessage(message, StatusTone::kError);
}

void LoginWindow::handleSwitchAccountRequested()
{
    if (!m_authService || m_authService->isLoggingOut()) {
        return;
    }

    // “切换账号”不是简单关窗口，而是一次完整登出：
    // 先向用户确认，再调用 AuthService 让服务端会话失效，最后回到登录页。
    const auto answer = QMessageBox::question(
        m_chatWindow != nullptr ? static_cast<QWidget *>(m_chatWindow)
                                : static_cast<QWidget *>(this),
        QStringLiteral("切换账号"),
        QStringLiteral("确定要退出当前账号并切换到登录页吗？"));
    if (answer != QMessageBox::Yes) {
        return;
    }

    QString errorMessage;
    if (!m_authService->logoutUser(&errorMessage)) {
        if (m_chatWindow) {
            m_chatWindow->setSessionActionSubmitting(false, false);
        }
        QMessageBox::warning(
            m_chatWindow != nullptr ? static_cast<QWidget *>(m_chatWindow)
                                    : static_cast<QWidget *>(this),
            QStringLiteral("切换账号"),
            errorMessage.isEmpty()
                ? QStringLiteral("切换账号失败，请稍后重试。")
                : errorMessage);
    }
}

void LoginWindow::handleSwitchAccountSucceeded()
{
    CHATCLIENT_LOG_INFO("login.window") << "切换账号成功";

    if (m_chatWindow) {
        m_chatWindow->setSessionActionSubmitting(false, false);
        m_chatWindow->setCurrentUserProfile(QStringLiteral("访客"),
                                            QStringLiteral("等待连接"));
        m_chatWindow->hide();
    }

    showLoginPage();
    setLoginStatusMessage(QStringLiteral("已退出当前账号，请重新登录"),
                          StatusTone::kSuccess);
    show();
    raise();
    activateWindow();
}

void LoginWindow::handleSwitchAccountFailed(const QString &message)
{
    CHATCLIENT_LOG_WARN("login.window")
        << "切换账号失败，message="
        << message;

    if (m_chatWindow) {
        if (m_authService && m_authService->hasActiveSession()) {
            const auto &session = m_authService->currentSession();
            const QString displayName = session.user.nickname.isEmpty()
                                            ? session.account
                                            : session.user.nickname;
            const QString statusText =
                QStringLiteral("已登录 · %1").arg(session.account);
            m_chatWindow->setCurrentUserProfile(displayName,
                                                statusText,
                                                session.user.userId,
                                                session.user.avatarUrl);
        }
        m_chatWindow->setSessionActionSubmitting(false, false);
        QMessageBox::warning(m_chatWindow,
                             QStringLiteral("切换账号"),
                             message.isEmpty()
                                 ? QStringLiteral("切换账号失败，请稍后重试。")
                                 : message);
    }
}

void LoginWindow::handleSignOutRequested()
{
    if (!performApplicationExitLogout(true,
                                      m_chatWindow != nullptr
                                          ? static_cast<QWidget *>(m_chatWindow)
                                          : static_cast<QWidget *>(this)))
    {
        return;
    }

    if (m_chatWindow) {
        m_chatWindow->allowWindowClose();
        m_chatWindow->close();
    }
    close();
}

void LoginWindow::handleRegisterAvatarSelect()
{
    if (!m_userApiClient || m_registerAvatarUploading)
    {
        return;
    }

    // 注册头像的体验是“选中即上传”：
    // - 先本地预览
    // - 再调用临时头像上传接口
    // - 注册提交时只带 avatar_upload_key
    //
    // 这样注册接口仍然是轻量表单，不需要在注册时再次上传二进制文件。
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择头像"),
        QString(),
        QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (filePath.isEmpty())
    {
        return;
    }

    const QImage image(filePath);
    if (image.isNull())
    {
        setRegisterStatusMessage(QStringLiteral("所选文件不是可识别的图片"),
                                 StatusTone::kError);
        return;
    }

    setRegisterAvatarPreview(image);
    setRegisterAvatarUploading(true);
    setRegisterStatusMessage(QStringLiteral("正在上传头像..."), StatusTone::kInfo);

    m_userApiClient->uploadTemporaryAvatar(
        filePath,
        [this](const chatclient::dto::user::TemporaryAvatarUploadResponseDto &response) {
            m_registerAvatarUploadKey = response.avatarUploadKey;
            setRegisterAvatarUploading(false);
            if (m_registerAvatarHintLabel)
            {
                m_registerAvatarHintLabel->setText(QStringLiteral("头像已上传，注册时会一并使用"));
                m_registerAvatarHintLabel->setProperty("statusTone",
                                                       QStringLiteral("success"));
                m_registerAvatarHintLabel->style()->unpolish(m_registerAvatarHintLabel);
                m_registerAvatarHintLabel->style()->polish(m_registerAvatarHintLabel);
            }

            CHATCLIENT_LOG_INFO("login.window")
                << "临时头像上传成功，request_id="
                << response.requestId
                << " avatar_upload_key="
                << response.avatarUploadKey;
            // 上传成功后不额外弹成功框，避免打断注册流程；提示留在状态区和头像 hint 里即可。
            setRegisterStatusMessage(QString(), StatusTone::kInfo);
        },
        [this](const chatclient::dto::user::ApiErrorDto &error) {
            setRegisterAvatarUploading(false);
            m_registerAvatarUploadKey.clear();
            if (m_registerAvatarSelectButton)
            {
                m_registerAvatarSelectButton->setText(QStringLiteral("重新上传头像"));
            }

            const QString localizedMessage =
                chatclient::service::localizeUserError(error);
            const QString message = localizedMessage.isEmpty()
                                        ? QStringLiteral("头像上传失败，请重新选择图片后再试")
                                        : localizedMessage;
            if (m_registerAvatarHintLabel)
            {
                m_registerAvatarHintLabel->setText(message);
                m_registerAvatarHintLabel->setProperty("statusTone",
                                                       QStringLiteral("error"));
                m_registerAvatarHintLabel->style()->unpolish(m_registerAvatarHintLabel);
                m_registerAvatarHintLabel->style()->polish(m_registerAvatarHintLabel);
            }

            CHATCLIENT_LOG_WARN("login.window")
                << "临时头像上传失败，request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
            setRegisterStatusMessage(message, StatusTone::kError);
        });
}

void LoginWindow::handleRegisterSubmit()
{
    if (!m_authService) {
        return;
    }

    CHATCLIENT_LOG_INFO("login.window")
        << "点击注册，account="
        << (m_registerAccountEdit ? m_registerAccountEdit->text() : QString());

    if (m_registerAvatarUploading)
    {
        // 注册请求必须等头像上传先稳定下来，否则服务端拿不到可引用的 avatar_upload_key。
        setRegisterStatusMessage(QStringLiteral("头像仍在上传，请稍候"),
                                 StatusTone::kInfo);
        return;
    }

    QString errorMessage;
    if (!m_authService->registerUser(m_registerAccountEdit ? m_registerAccountEdit->text() : QString(),
                                     m_registerNicknameEdit ? m_registerNicknameEdit->text() : QString(),
                                     m_registerPasswordEdit ? m_registerPasswordEdit->text() : QString(),
                                     m_registerConfirmEdit ? m_registerConfirmEdit->text() : QString(),
                                     m_registerAvatarUploadKey,
                                     &errorMessage)) {
        CHATCLIENT_LOG_WARN("login.window")
            << "注册提交被拒绝，message="
            << errorMessage;
        setRegisterStatusMessage(errorMessage, StatusTone::kError);
        return;
    }
}

void LoginWindow::handleRegisterSucceeded(
    const chatclient::dto::auth::RegisterUserDto &user)
{
    CHATCLIENT_LOG_INFO("login.window")
        << "注册成功，user_id="
        << user.userId
        << " account="
        << user.account;
    setRegisterSubmitting(false);
    setRegisterStatusMessage(QStringLiteral("注册成功，请返回登录页继续。"),
                             StatusTone::kSuccess);

    // 注册成功后做一次表单和头像状态重置，避免下次打开注册页时还残留上一次输入。
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
    resetRegisterAvatarState();

    showLoginPage();
    QMessageBox::information(
        this,
        QStringLiteral("注册成功"),
        QStringLiteral("账号 %1 已创建成功，请直接在登录页继续登录。")
            .arg(user.account));
}

void LoginWindow::handleRegisterFailed(const QString &message)
{
    CHATCLIENT_LOG_WARN("login.window")
        << "注册失败，message="
        << message;
    setRegisterSubmitting(false);
    setRegisterStatusMessage(message, StatusTone::kError);
}

void LoginWindow::setLoginSubmitting(bool submitting)
{
    if (m_accountEdit) {
        m_accountEdit->setEnabled(!submitting);
    }
    if (m_passwordEdit) {
        m_passwordEdit->setEnabled(!submitting);
    }
    if (m_rememberCheck) {
        m_rememberCheck->setEnabled(!submitting);
    }
    if (m_autoLoginCheck) {
        m_autoLoginCheck->setEnabled(!submitting);
    }
    if (m_loginButton) {
        m_loginButton->setEnabled(!submitting);
    }
    if (m_registerButton) {
        m_registerButton->setEnabled(!submitting);
    }
}

void LoginWindow::setLoginStatusMessage(const QString &message,
                                        StatusTone tone)
{
    if (!m_loginStatusLabel) {
        return;
    }

    if (message.isEmpty()) {
        m_loginStatusLabel->clear();
        m_loginStatusLabel->setVisible(false);
        return;
    }

    QString toneText = QStringLiteral("info");
    if (tone == StatusTone::kSuccess) {
        toneText = QStringLiteral("success");
    } else if (tone == StatusTone::kError) {
        toneText = QStringLiteral("error");
    }

    m_loginStatusLabel->setProperty("statusTone", toneText);
    m_loginStatusLabel->setText(message);
    m_loginStatusLabel->setVisible(true);
    m_loginStatusLabel->style()->unpolish(m_loginStatusLabel);
    m_loginStatusLabel->style()->polish(m_loginStatusLabel);
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
    if (m_registerAvatarSelectButton) {
        m_registerAvatarSelectButton->setEnabled(!submitting && !m_registerAvatarUploading);
    }
    if (m_registerSubmitButton) {
        m_registerSubmitButton->setEnabled(!submitting && !m_registerAvatarUploading);
    }
    if (m_backToLoginButton) {
        m_backToLoginButton->setEnabled(!submitting && !m_registerAvatarUploading);
    }
}

void LoginWindow::setRegisterStatusMessage(const QString &message,
                                           StatusTone tone)
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
    if (tone == StatusTone::kSuccess) {
        toneText = QStringLiteral("success");
    } else if (tone == StatusTone::kError) {
        toneText = QStringLiteral("error");
    }

    m_registerStatusLabel->setProperty("statusTone", toneText);
    m_registerStatusLabel->setText(message);
    m_registerStatusLabel->setVisible(true);
    m_registerStatusLabel->style()->unpolish(m_registerStatusLabel);
    m_registerStatusLabel->style()->polish(m_registerStatusLabel);
}

void LoginWindow::setRegisterAvatarUploading(const bool uploading)
{
    m_registerAvatarUploading = uploading;

    // 头像上传和注册提交存在强依赖，所以这里要同时联动：
    // - “设置头像”按钮文本
    // - 注册按钮可用性
    // - 返回登录按钮可用性
    // - hint 文案
    if (m_registerAvatarSelectButton)
    {
        m_registerAvatarSelectButton->setEnabled(!uploading);
        m_registerAvatarSelectButton->setText(
            uploading ? QStringLiteral("上传中...")
                      : (m_registerAvatarUploadKey.isEmpty()
                             ? QStringLiteral("设置头像")
                             : QStringLiteral("更换头像")));
    }

    if (m_registerSubmitButton)
    {
        m_registerSubmitButton->setEnabled(!uploading && !m_authService->isRegistering());
    }

    if (m_backToLoginButton)
    {
        m_backToLoginButton->setEnabled(!uploading && !m_authService->isRegistering());
    }

    if (m_registerAvatarHintLabel && uploading)
    {
        m_registerAvatarHintLabel->setText(QStringLiteral("头像上传中，请稍候"));
        m_registerAvatarHintLabel->setProperty("statusTone",
                                               QStringLiteral("info"));
        m_registerAvatarHintLabel->style()->unpolish(m_registerAvatarHintLabel);
        m_registerAvatarHintLabel->style()->polish(m_registerAvatarHintLabel);
    }
}

void LoginWindow::resetRegisterAvatarState()
{
    // 这个 helper 用来把注册头像区域恢复到完全初始态。
    // 注册成功、切回登录页或上传失败重试等场景都会复用它。
    m_registerAvatarUploading = false;
    m_registerAvatarUploadKey.clear();

    if (m_registerAvatarPreviewLabel)
    {
        m_registerAvatarPreviewLabel->clear();
        m_registerAvatarPreviewLabel->setPixmap(QPixmap());
        m_registerAvatarPreviewLabel->setText(QStringLiteral("头像"));
        m_registerAvatarPreviewLabel->setToolTip(QString());
    }

    if (m_registerAvatarHintLabel)
    {
        m_registerAvatarHintLabel->setText(
            QStringLiteral("可选。选择图片后会立即上传临时头像"));
        m_registerAvatarHintLabel->setProperty("statusTone",
                                               QStringLiteral("info"));
        m_registerAvatarHintLabel->style()->unpolish(m_registerAvatarHintLabel);
        m_registerAvatarHintLabel->style()->polish(m_registerAvatarHintLabel);
    }

    if (m_registerAvatarSelectButton)
    {
        m_registerAvatarSelectButton->setEnabled(true);
        m_registerAvatarSelectButton->setText(QStringLiteral("设置头像"));
    }

    if (m_registerSubmitButton)
    {
        m_registerSubmitButton->setEnabled(true);
    }

    if (m_backToLoginButton)
    {
        m_backToLoginButton->setEnabled(true);
    }
}

void LoginWindow::setRegisterAvatarPreview(const QImage &image)
{
    if (!m_registerAvatarPreviewLabel)
    {
        return;
    }

    const QSize previewSize = m_registerAvatarPreviewLabel->size();
    const QPixmap roundedPixmap =
        createRoundedAvatarPixmap(image, previewSize);
    if (!roundedPixmap.isNull())
    {
        m_registerAvatarPreviewLabel->setText(QString());
        m_registerAvatarPreviewLabel->setPixmap(roundedPixmap);
    }
}

void LoginWindow::openChatWindow(
    const chatclient::dto::auth::LoginSessionDto &session)
{
    if (!m_chatWindow) {
        // ChatWindow 采用延迟创建：只有真正登录成功时才实例化，
        // 这样程序启动阶段不会额外建立聊天页和 WS 相关对象。
        m_chatWindow = new ChatWindow();
        m_chatWindow->setAuthService(m_authService);
        connect(m_chatWindow,
                &ChatWindow::switchAccountRequested,
                this,
                &LoginWindow::handleSwitchAccountRequested);
        connect(m_chatWindow,
                &ChatWindow::signOutRequested,
                this,
                &LoginWindow::handleSignOutRequested);
    }

    // 登录成功后，LoginWindow 只做一次“把认证结果同步到聊天页并切过去”。
    const QString displayName = session.user.nickname.isEmpty()
                                    ? session.account
                                    : session.user.nickname;
    const QString statusText = QStringLiteral("已登录 · %1").arg(session.account);

    m_chatWindow->setCurrentUserProfile(displayName,
                                        statusText,
                                        session.user.userId,
                                        session.user.avatarUrl);
    m_chatWindow->setSessionActionSubmitting(false, false);
    m_chatWindow->show();
    m_chatWindow->raise();
    m_chatWindow->activateWindow();
    hide();
}

bool LoginWindow::performApplicationExitLogout(bool showConfirmation,
                                               QWidget *dialogParent)
{
    if (m_applicationShutdownInProgress)
    {
        return true;
    }

    if (showConfirmation)
    {
        // 从聊天页主动登出时走确认；应用关闭时可以关闭确认，保持退出流程顺滑。
        const auto answer = QMessageBox::question(
            dialogParent != nullptr ? dialogParent : this,
            QStringLiteral("登出"),
            QStringLiteral("确定要登出当前账号并退出程序吗？"));
        if (answer != QMessageBox::Yes)
        {
            return false;
        }
    }

    m_applicationShutdownInProgress = true;

    // 应用退出时优先尝试一次阻塞式远端登出，让服务端及时回收 device_session。
    // 如果失败，也仍然允许本地继续退出，不把用户困在窗口里。
    if (m_chatWindow)
    {
        m_chatWindow->setSessionActionSubmitting(true, true);
    }

    QString errorMessage;
    const bool remoteLoggedOut =
        !m_authService || m_authService->logoutUserBlocking(&errorMessage);

    if (!remoteLoggedOut)
    {
        CHATCLIENT_LOG_WARN("login.window")
            << "程序退出时的登出流程已结束，仅执行了本地清理，message="
            << errorMessage;
    }

    return true;
}
