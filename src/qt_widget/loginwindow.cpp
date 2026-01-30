#include "loginwindow.h"

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

LoginWindow::LoginWindow(QWidget *parent)
    : QWidget(parent)
{
    // 窗口基础设置：标题与固定尺寸，保证布局稳定。
    setWindowTitle(QStringLiteral("chatClient 登录"));
    setFixedSize(420, 500);

    // 根布局：垂直排布整体结构（标题、页面切换区）。
    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(32, 28, 32, 28);
    rootLayout->setSpacing(18);

    // 标题与副标题：在登录/注册之间切换时更新文本。
    m_titleLabel = new QLabel(QStringLiteral("ChatClient"), this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setStyleSheet(QStringLiteral("font-size: 26px; font-weight: 600;"));

    m_subtitleLabel = new QLabel(QStringLiteral("欢迎回来，请登录"), this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setStyleSheet(QStringLiteral("color: #6f7782;"));

    // 页面容器：使用栈式布局在登录页/注册页之间切换。
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createLoginPage());
    m_stack->addWidget(createRegisterPage());
    m_stack->setCurrentIndex(0);

    // 组装整体布局结构。
    rootLayout->addWidget(m_titleLabel);
    rootLayout->addWidget(m_subtitleLabel);
    rootLayout->addWidget(m_stack);
    rootLayout->addStretch(1);
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
}

void LoginWindow::showLoginPage()
{
    // 切换回登录页面，同时恢复标题文本。
    if (!m_stack) {
        return;
    }
    m_stack->setCurrentIndex(0);
    m_titleLabel->setText(QStringLiteral("ChatClient"));
    m_subtitleLabel->setText(QStringLiteral("欢迎回来，请登录"));
}

QWidget *LoginWindow::createLoginPage()
{
    // 登录页容器：卡片式布局承载登录表单。
    auto *card = new QFrame(m_stack);
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: #ffffff; border: 1px solid #e6e6e6; border-radius: 12px; }"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(14);

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
    m_loginButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f6feb; color: #ffffff; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: #1a5ed0; }"
        "QPushButton:pressed { background: #144aa6; }"));

    // 辅助操作区：忘记密码提示 + 注册入口。
    auto *helperLayout = new QHBoxLayout();
    auto *forgotLabel = new QLabel(QStringLiteral("忘记密码？"), card);
    forgotLabel->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    m_registerButton = new QPushButton(QStringLiteral("注册新账号"), card);
    m_registerButton->setFlat(true);
    m_registerButton->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    helperLayout->addWidget(forgotLabel);
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
    card->setStyleSheet(QStringLiteral(
        "QFrame { background: #ffffff; border: 1px solid #e6e6e6; border-radius: 12px; }"));
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(24, 24, 24, 24);
    cardLayout->setSpacing(12);

    // 注册账号输入区。
    m_registerAccountEdit = new QLineEdit(card);
    m_registerAccountEdit->setPlaceholderText(QStringLiteral("用户名"));


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
    m_registerSubmitButton->setStyleSheet(QStringLiteral(
        "QPushButton { background: #1f6feb; color: #ffffff; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: #1a5ed0; }"
        "QPushButton:pressed { background: #144aa6; }"));

    // 返回登录入口：已注册用户可快速切回登录页。
    auto *backLayout = new QHBoxLayout();
    auto *hintLabel = new QLabel(QStringLiteral("已有账号？"), card);
    hintLabel->setStyleSheet(QStringLiteral("color: #6f7782;"));
    m_backToLoginButton = new QPushButton(QStringLiteral("返回登录"), card);
    m_backToLoginButton->setFlat(true);
    m_backToLoginButton->setStyleSheet(QStringLiteral("color: #1f6feb;"));
    backLayout->addWidget(hintLabel);
    backLayout->addWidget(m_backToLoginButton);
    backLayout->addStretch(1);

    // 将注册控件按顺序加入卡片布局。
    cardLayout->addWidget(m_registerAccountEdit);
    cardLayout->addWidget(m_registerPasswordEdit);
    cardLayout->addWidget(m_registerConfirmEdit);
    cardLayout->addWidget(m_registerSubmitButton);
    cardLayout->addLayout(backLayout);

    // 点击“返回登录”时切换回登录页。
    connect(m_backToLoginButton, &QPushButton::clicked, this, &LoginWindow::showLoginPage);

    return card;
}
