#include "chatwindow.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QTextEdit>
#include <QVBoxLayout>

// 从 Qt 资源系统读取聊天窗口专用样式。
// 约定：ChatWindow 只读取 chatwindow.qss，避免与登录窗口样式耦合。
// 若读取失败，返回空字符串，界面会退化为 Qt 默认样式，但不影响程序运行。
static QString loadChatStyleSheet()
{
    QFile file(QStringLiteral(":/chatwindow.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
{
    // 用于 QSS 顶层选择器 QWidget#chatWindow。
    setObjectName(QStringLiteral("chatWindow"));
    setWindowTitle(QStringLiteral("chatClient 聊天"));
    resize(1180, 760);

    // 根布局：左右两栏。
    // 左侧固定宽度用于会话导航，右侧自适应用于聊天主体。
    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(20, 20, 20, 20);
    rootLayout->setSpacing(16);

    // stretch = 0: 左侧保持固定宽度
    // stretch = 1: 右侧占据剩余空间
    rootLayout->addWidget(createSidebar(), 0);
    rootLayout->addWidget(createChatPanel(), 1);

    // 在所有子控件创建完后统一加载样式，确保选择器可以完整命中。
    setStyleSheet(loadChatStyleSheet());
}

QWidget *ChatWindow::createSidebar()
{
    // 侧边栏容器：品牌色背景 + 圆角卡片。
    auto *sidebar = new QFrame(this);
    sidebar->setObjectName(QStringLiteral("sidebarPanel"));
    sidebar->setFixedWidth(320);

    // 纵向区域：品牌 -> 搜索 -> 会话列表(可伸展) -> 个人信息。
    auto *sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(18, 18, 18, 18);
    sidebarLayout->setSpacing(14);

    // 顶部品牌文字。
    auto *brandLabel = new QLabel(QStringLiteral("CHATCLIENT"), sidebar);
    brandLabel->setObjectName(QStringLiteral("brandLabel"));

    // 会话搜索框（仅 UI 展示，未接过滤逻辑）。
    m_searchEdit = new QLineEdit(sidebar);
    m_searchEdit->setObjectName(QStringLiteral("sessionSearch"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索会话或联系人"));

    // 会话列表（演示数据）：
    // 每个 item 使用两行文本，模拟“会话名 + 最后一条消息摘要”。
    m_sessionList = new QListWidget(sidebar);
    m_sessionList->setObjectName(QStringLiteral("sessionList"));
    m_sessionList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessionList->addItem(QStringLiteral("产品讨论组\n最后消息: 需求文档已更新"));
    m_sessionList->addItem(QStringLiteral("李华\n最后消息: 10 分钟后开会"));
    m_sessionList->addItem(QStringLiteral("后端联调\n最后消息: /api/chat 已联通"));
    m_sessionList->addItem(QStringLiteral("设计评审\n最后消息: 新版视觉已上传"));
    m_sessionList->setCurrentRow(0);

    // 底部用户信息卡片。
    auto *profileCard = new QFrame(sidebar);
    profileCard->setObjectName(QStringLiteral("profileCard"));
    auto *profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(12, 12, 12, 12);
    profileLayout->setSpacing(4);

    auto *nameLabel = new QLabel(QStringLiteral("Wumo"), profileCard);
    nameLabel->setObjectName(QStringLiteral("profileName"));
    auto *roleLabel = new QLabel(QStringLiteral("在线"), profileCard);
    roleLabel->setObjectName(QStringLiteral("profileStatus"));

    profileLayout->addWidget(nameLabel);
    profileLayout->addWidget(roleLabel);

    // 列表区域设置 stretch=1，让其占据侧边栏的主要高度。
    sidebarLayout->addWidget(brandLabel);
    sidebarLayout->addWidget(m_searchEdit);
    sidebarLayout->addWidget(m_sessionList, 1);
    sidebarLayout->addWidget(profileCard);

    return sidebar;
}

QWidget *ChatWindow::createChatPanel()
{
    // 右侧聊天主体卡片。
    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("chatPanel"));

    // 垂直结构：header -> message area(可伸展) -> composer。
    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(20, 18, 20, 18);
    panelLayout->setSpacing(14);

    // 顶栏：会话标题 + 在线状态 + 功能按钮。
    auto *header = new QFrame(panel);
    header->setObjectName(QStringLiteral("chatHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    auto *conversationTitle = new QLabel(QStringLiteral("产品讨论组"), header);
    conversationTitle->setObjectName(QStringLiteral("conversationTitle"));
    auto *conversationMeta = new QLabel(QStringLiteral("14 人在线"), header);
    conversationMeta->setObjectName(QStringLiteral("conversationMeta"));
    auto *titleBlock = new QWidget(header);
    auto *titleLayout = new QVBoxLayout(titleBlock);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);
    titleLayout->addWidget(conversationTitle);
    titleLayout->addWidget(conversationMeta);

    // 顶栏右侧按钮（仅展示按钮样式与布局，不绑定业务槽函数）。
    auto *voiceBtn = new QPushButton(QStringLiteral("语音"), header);
    voiceBtn->setObjectName(QStringLiteral("headerGhostButton"));
    auto *videoBtn = new QPushButton(QStringLiteral("视频"), header);
    videoBtn->setObjectName(QStringLiteral("headerGhostButton"));

    headerLayout->addWidget(titleBlock, 1);
    headerLayout->addWidget(voiceBtn);
    headerLayout->addWidget(videoBtn);

    // 消息滚动区域：
    // 使用 QScrollArea 包裹消息容器，消息较多时支持垂直滚动。
    auto *scrollArea = new QScrollArea(panel);
    scrollArea->setObjectName(QStringLiteral("messageScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 真实项目中，这里通常会替换为自定义 message list + model。
    // 当前版本用垂直布局模拟消息流，便于快速验证视觉效果。
    m_messageContainer = new QWidget(scrollArea);
    m_messageContainer->setObjectName(QStringLiteral("messageContainer"));
    m_messageLayout = new QVBoxLayout(m_messageContainer);
    m_messageLayout->setContentsMargins(4, 4, 4, 4);
    m_messageLayout->setSpacing(10);
    m_messageLayout->addWidget(createMessageRow(QStringLiteral("李华"),
                                                QStringLiteral("大家看下这版首页布局，今天定稿。"),
                                                QStringLiteral("09:12"),
                                                false));
    m_messageLayout->addWidget(createMessageRow(QStringLiteral("我"),
                                                QStringLiteral("收到，我这边会把组件拆分同步到前端仓库。"),
                                                QStringLiteral("09:14"),
                                                true));
    m_messageLayout->addWidget(createMessageRow(QStringLiteral("产品经理"),
                                                QStringLiteral("别忘了把移动端间距规范也补上。"),
                                                QStringLiteral("09:15"),
                                                false));
    // 末尾弹性空间：让消息默认靠上显示，避免少量消息时垂直居中。
    m_messageLayout->addStretch(1);
    scrollArea->setWidget(m_messageContainer);

    // 底部输入区：输入框 + 操作按钮。
    auto *composer = new QFrame(panel);
    composer->setObjectName(QStringLiteral("composerPanel"));
    auto *composerLayout = new QVBoxLayout(composer);
    composerLayout->setContentsMargins(12, 12, 12, 12);
    composerLayout->setSpacing(10);

    m_messageEditor = new QTextEdit(composer);
    m_messageEditor->setObjectName(QStringLiteral("messageEditor"));
    m_messageEditor->setPlaceholderText(QStringLiteral("输入消息，按 Enter 发送，Shift+Enter 换行"));
    m_messageEditor->setMinimumHeight(110);

    // 输入区按钮行：左侧辅助按钮，右侧主发送按钮。
    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);

    auto *emojiBtn = new QPushButton(QStringLiteral("表情"), composer);
    emojiBtn->setObjectName(QStringLiteral("composerGhostButton"));
    auto *fileBtn = new QPushButton(QStringLiteral("文件"), composer);
    fileBtn->setObjectName(QStringLiteral("composerGhostButton"));
    auto *sendBtn = new QPushButton(QStringLiteral("发送"), composer);
    sendBtn->setObjectName(QStringLiteral("sendButton"));

    actionRow->addWidget(emojiBtn);
    actionRow->addWidget(fileBtn);
    actionRow->addStretch(1);
    actionRow->addWidget(sendBtn);

    composerLayout->addWidget(m_messageEditor);
    composerLayout->addLayout(actionRow);

    // 消息滚动区域设置 stretch=1，占用聊天主体大部分空间。
    panelLayout->addWidget(header);
    panelLayout->addWidget(scrollArea, 1);
    panelLayout->addWidget(composer);

    return panel;
}

QWidget *ChatWindow::createMessageRow(const QString &author,
                                      const QString &text,
                                      const QString &timeText,
                                      bool fromSelf)
{
    // 每条消息 = 外层行容器 + 内层气泡容器。
    // 外层行负责左右对齐，内层气泡负责文字排版。
    auto *row = new QWidget(m_messageContainer);
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    // 根据消息归属绑定不同 objectName，由 QSS 区分颜色与边框。
    auto *bubble = new QFrame(row);
    bubble->setObjectName(fromSelf ? QStringLiteral("messageBubbleSelf") : QStringLiteral("messageBubblePeer"));
    auto *bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout->setContentsMargins(12, 10, 12, 10);
    bubbleLayout->setSpacing(6);

    auto *authorLabel = new QLabel(author, bubble);
    authorLabel->setObjectName(QStringLiteral("messageAuthor"));
    if (fromSelf) {
        authorLabel->setAlignment(Qt::AlignRight);
    }

    auto *messageLabel = new QLabel(text, bubble);
    messageLabel->setObjectName(QStringLiteral("messageText"));
    messageLabel->setWordWrap(true);

    auto *timeLabel = new QLabel(timeText, bubble);
    timeLabel->setObjectName(QStringLiteral("messageTime"));
    if (fromSelf) {
        timeLabel->setAlignment(Qt::AlignRight);
    }

    bubbleLayout->addWidget(authorLabel);
    bubbleLayout->addWidget(messageLabel);
    bubbleLayout->addWidget(timeLabel);

    // 利用 stretch 将“我方消息”推到右侧，“对方消息”保持左侧。
    if (fromSelf) {
        rowLayout->addStretch(1);
        rowLayout->addWidget(bubble, 0, Qt::AlignRight);
    } else {
        rowLayout->addWidget(bubble, 0, Qt::AlignLeft);
        rowLayout->addStretch(1);
    }

    return row;
}
