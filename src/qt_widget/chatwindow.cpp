#include "chatwindow.h"

#include "delegate/messagedelegate.h"
#include "model/messagemodel.h"

#include <QAbstractItemView>
#include <QClipboard>
#include <QFile>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QTextEdit>
#include <QTime>
#include <QTimer>
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

    // 消息区域：
    // 使用 QListView + model + delegate，避免为每条消息创建 QWidget。
    m_messageListView = new QListView(panel);
    m_messageListView->setObjectName(QStringLiteral("messageListView"));
    m_messageListView->setFrameShape(QFrame::NoFrame);
    m_messageListView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 聊天气泡高度不固定，按像素滚动比按 item 滚动更平滑。
    m_messageListView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 为“复制当前消息”保留单选能力（视觉由 delegate 决定，不额外高亮整行背景）。
    m_messageListView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_messageListView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 各行高度由 delegate::sizeHint 动态计算，不能开启 uniformItemSizes。
    m_messageListView->setUniformItemSizes(false);
    // 点击后允许获得焦点，这样 Ctrl+C 可以作用于当前消息。
    m_messageListView->setFocusPolicy(Qt::ClickFocus);
    m_messageListView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_messageModel = new MessageModel(m_messageListView);
    m_messageListView->setModel(m_messageModel);
    // 委托负责气泡样式与布局，QListView 本身只管复用和滚动。
    m_messageListView->setItemDelegate(new MessageDelegate(m_messageListView));
    connect(m_messageListView, &QListView::customContextMenuRequested, this, &ChatWindow::showMessageContextMenu);
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, m_messageListView);
    copyShortcut->setContext(Qt::WidgetShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &ChatWindow::copyCurrentMessageText);

    m_messageModel->addTextMessage(QStringLiteral("李华"),
                                   QStringLiteral("大家看下这版首页布局，今天定稿。"),
                                   QStringLiteral("09:12"),
                                   false);
    m_messageModel->addTextMessage(QStringLiteral("我"),
                                   QStringLiteral("收到，我这边会把组件拆分同步到前端仓库。"),
                                   QStringLiteral("09:14"),
                                   true);
    m_messageModel->addTextMessage(QStringLiteral("产品经理"),
                                   QStringLiteral("别忘了把移动端间距规范也补上。"),
                                   QStringLiteral("09:15"),
                                   false);

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

    // 发送行为：
    // 点击发送或按 Enter 提交消息；Shift+Enter 仍走 QTextEdit 默认换行逻辑。
    connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::handleSendMessage);
    // 这里使用 QShortcut 只拦截“单独 Enter”，不会覆盖 Shift+Enter 的默认换行。
    auto *returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), m_messageEditor);
    returnShortcut->setContext(Qt::WidgetShortcut);
    connect(returnShortcut, &QShortcut::activated, this, &ChatWindow::handleSendMessage);
    auto *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), m_messageEditor);
    enterShortcut->setContext(Qt::WidgetShortcut);
    connect(enterShortcut, &QShortcut::activated, this, &ChatWindow::handleSendMessage);

    // 消息列表设置 stretch=1，占用聊天主体大部分空间。
    panelLayout->addWidget(header);
    panelLayout->addWidget(m_messageListView, 1);
    panelLayout->addWidget(composer);

    QTimer::singleShot(0, this, [this]() {
        if (m_messageListView) {
            m_messageListView->scrollToBottom();
        }
    });

    return panel;
}

void ChatWindow::appendMessage(const QString &author, const QString &text, bool fromSelf)
{
    if (!m_messageModel || !m_messageListView) {
        return;
    }

    // 时间在 UI 层统一格式化，后续接入服务端时间时可在此改成时间戳转换。
    m_messageModel->addTextMessage(author,
                                   text,
                                   QTime::currentTime().toString(QStringLiteral("HH:mm")),
                                   fromSelf);
    m_messageListView->scrollToBottom();
}

void ChatWindow::handleSendMessage()
{
    if (!m_messageEditor) {
        return;
    }

    const QString text = m_messageEditor->toPlainText().trimmed();
    // 过滤纯空白输入，避免插入空气泡。
    if (text.isEmpty()) {
        return;
    }

    appendMessage(QStringLiteral("我"), text, true);
    m_messageEditor->clear();
}

void ChatWindow::copyMessageText(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    QString text = index.data(MessageModel::TextRole).toString();
    // 非文本消息可能没有纯 text 字段，此时回退复制展示文案（如 [图片消息]/[文件] xxx）。
    if (text.isEmpty()) {
        text = index.data(Qt::DisplayRole).toString();
    }
    if (text.isEmpty()) {
        return;
    }

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
}

void ChatWindow::copyCurrentMessageText()
{
    if (!m_messageListView) {
        return;
    }
    copyMessageText(m_messageListView->currentIndex());
}

void ChatWindow::showMessageContextMenu(const QPoint &pos)
{
    if (!m_messageListView) {
        return;
    }

    const QModelIndex index = m_messageListView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    m_messageListView->setCurrentIndex(index);
    QMenu menu(m_messageListView);
    QAction *copyAction = menu.addAction(QStringLiteral("复制消息"));

    const QAction *chosen = menu.exec(m_messageListView->viewport()->mapToGlobal(pos));
    if (chosen == copyAction) {
        copyMessageText(index);
    }
}
