#include "chatwindow.h"

#include "api/user_api_client.h"
#include "config/appconfig.h"
#include "log/app_logger.h"
#include "model/messagemodel.h"
#include "model/messagemodelregistry.h"
#include "qt_widget/addfrienddialog.h"
#include "view/messagelistview.h"

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QImage>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kConversationIdRole = Qt::UserRole + 1;
constexpr int kConversationTitleRole = Qt::UserRole + 2;
constexpr int kConversationMetaRole = Qt::UserRole + 3;
constexpr int kFriendNameRole = Qt::UserRole + 4;
constexpr int kFriendMetaRole = Qt::UserRole + 5;
constexpr int kFriendHintRole = Qt::UserRole + 6;

QString loadChatStyleSheet()
{
    QFile file(QStringLiteral(":/chatwindow.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QListWidgetItem *createRichListItem(const QString &title,
                                    const QString &subtitle,
                                    QListWidget *parent)
{
    auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(title, subtitle),
                                     parent);
    item->setSizeHint(QSize(0, 64));
    return item;
}

QPixmap createRoundedAvatarPixmap(const QImage &image, const QSize &size)
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

}  // namespace

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
{
    const auto &config = chatclient::config::AppConfig::instance();
    m_userApiClient = new chatclient::api::UserApiClient(this);

    // 聊天主界面当前改成三段式结构：
    // 1) 左侧导航栏负责“消息 / 好友”模式切换；
    // 2) 中间栏负责展示对应模式下的列表；
    // 3) 右侧负责当前选中项的详细内容。
    setObjectName(QStringLiteral("chatWindow"));
    setWindowTitle(config.chatWindowTitle());
    resize(1280, 800);

    auto *rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(18, 18, 18, 18);
    rootLayout->setSpacing(14);

    rootLayout->addWidget(createNavigationRail(), 0);
    rootLayout->addWidget(createMiddlePanel(), 0);

    m_contentStack = new QStackedWidget(this);
    m_contentStack->setObjectName(QStringLiteral("contentStack"));
    m_contentStack->addWidget(createMessageContentPage());
    m_contentStack->addWidget(createFriendContentPage());
    rootLayout->addWidget(m_contentStack, 1);

    setStyleSheet(loadChatStyleSheet());

    // 示例消息数据仍然在本地准备，方便当前继续联调 UI。
    m_messageModelRegistry->addTextMessage(QStringLiteral("product_discussion"),
                                           QStringLiteral("李华"),
                                           QStringLiteral("首页改版需求已经更新到共享文档。"),
                                           QStringLiteral("09:12"),
                                           false);
    m_messageModelRegistry->addTextMessage(QStringLiteral("product_discussion"),
                                           QStringLiteral("我"),
                                           QStringLiteral("收到，我今天把聊天页骨架一起整理出来。"),
                                           QStringLiteral("09:14"),
                                           true);
    m_messageModelRegistry->addTextMessage(QStringLiteral("backend_sync"),
                                           QStringLiteral("后端同学"),
                                           QStringLiteral("登录、登出接口都已经通了，可以开始接客户端。"),
                                           QStringLiteral("10:06"),
                                           false);
    m_messageModelRegistry->addTextMessage(QStringLiteral("backend_sync"),
                                           QStringLiteral("我"),
                                           QStringLiteral("好的，今天先把消息 / 好友两栏骨架切出来。"),
                                           QStringLiteral("10:08"),
                                           true);
    m_messageModelRegistry->addTextMessage(QStringLiteral("design_review"),
                                           QStringLiteral("设计师"),
                                           QStringLiteral("导航栏方案建议固定成消息和好友两个主入口。"),
                                           QStringLiteral("11:20"),
                                           false);

    switchSection(SidebarSection::kMessages);
    handleSessionSelectionChanged();
    handleFriendSelectionChanged();
}

void ChatWindow::setCurrentUserProfile(const QString &displayName,
                                       const QString &statusText,
                                       const QString &userId,
                                       const QString &avatarStorageKey)
{
    m_currentProfileUserId = userId.trimmed();
    m_currentProfileAvatarStorageKey = avatarStorageKey.trimmed();
    updateProfileAvatar(displayName);

    if (m_profileNameLabel)
    {
        m_profileNameLabel->setText(displayName);
    }

    if (m_profileStatusLabel)
    {
        m_profileStatusLabel->setText(statusText);
    }

    if (!m_userApiClient || m_currentProfileUserId.isEmpty() ||
        m_currentProfileAvatarStorageKey.isEmpty())
    {
        return;
    }

    const QString expectedUserId = m_currentProfileUserId;
    m_userApiClient->downloadUserAvatar(
        expectedUserId,
        [this, expectedUserId](const QByteArray &data) {
            if (expectedUserId != m_currentProfileUserId)
            {
                return;
            }

            QImage image;
            if (!image.loadFromData(data))
            {
                CHATCLIENT_LOG_WARN("chat.window")
                    << "failed to decode avatar image for user_id="
                    << expectedUserId;
                return;
            }

            updateProfileAvatarImage(image);
        },
        [expectedUserId](const chatclient::dto::user::ApiErrorDto &error) {
            CHATCLIENT_LOG_WARN("chat.window")
                << "failed to download avatar user_id="
                << expectedUserId
                << " request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;
        });
}

void ChatWindow::setSessionActionSubmitting(const bool submitting,
                                            const bool quitting)
{
    if (m_switchAccountNavButton)
    {
        m_switchAccountNavButton->setEnabled(!submitting);
        m_switchAccountNavButton->setText(
            submitting && !quitting ? QStringLiteral("切换中...")
                                    : QStringLiteral("切换账号"));
    }

    if (m_signOutNavButton)
    {
        m_signOutNavButton->setEnabled(!submitting);
        m_signOutNavButton->setText(
            submitting && quitting ? QStringLiteral("登出中...")
                                   : QStringLiteral("登出"));
    }

    if (m_profileStatusLabel && submitting)
    {
        m_profileStatusLabel->setText(quitting ? QStringLiteral("正在退出程序")
                                               : QStringLiteral("正在切换账号"));
    }
}

void ChatWindow::allowWindowClose()
{
    m_allowClose = true;
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    if (m_allowClose)
    {
        QWidget::closeEvent(event);
        return;
    }

    emit signOutRequested();
    event->ignore();
}

void ChatWindow::updateProfileAvatar(const QString &displayName)
{
    if (!m_navAvatarLabel)
    {
        return;
    }

    const QString trimmedName = displayName.trimmed();
    if (trimmedName.isEmpty())
    {
        m_navAvatarLabel->setPixmap(QPixmap());
        m_navAvatarLabel->setText(QStringLiteral("访客"));
        m_navAvatarLabel->setToolTip(QString());
        return;
    }

    // 左上角头像空间固定，因此默认头像使用用户名文本的前两个字符。
    // 完整名字通过 tooltip 保留，避免导航栏顶部被长用户名撑坏。
    const QString avatarText =
        trimmedName.size() <= 2 ? trimmedName : trimmedName.left(2);
    m_navAvatarLabel->setPixmap(QPixmap());
    m_navAvatarLabel->setText(avatarText);
    m_navAvatarLabel->setToolTip(trimmedName);
}

void ChatWindow::updateProfileAvatarImage(const QImage &image)
{
    if (!m_navAvatarLabel)
    {
        return;
    }

    const QPixmap roundedPixmap =
        createRoundedAvatarPixmap(image, m_navAvatarLabel->size());
    if (roundedPixmap.isNull())
    {
        return;
    }

    m_navAvatarLabel->setText(QString());
    m_navAvatarLabel->setPixmap(roundedPixmap);
}

QWidget *ChatWindow::createNavigationRail()
{
    auto *rail = new QFrame(this);
    rail->setObjectName(QStringLiteral("navigationRail"));
    rail->setFixedWidth(108);

    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(12, 14, 12, 14);
    railLayout->setSpacing(12);

    m_navAvatarLabel = new QLabel(QStringLiteral("访客"), rail);
    m_navAvatarLabel->setObjectName(QStringLiteral("navBrandBadge"));
    m_navAvatarLabel->setAlignment(Qt::AlignCenter);
    m_navAvatarLabel->setFixedSize(52, 52);

    m_messagesNavButton = new QPushButton(QStringLiteral("消息"), rail);
    m_messagesNavButton->setObjectName(QStringLiteral("navButton"));
    m_messagesNavButton->setCheckable(true);

    m_friendsNavButton = new QPushButton(QStringLiteral("好友"), rail);
    m_friendsNavButton->setObjectName(QStringLiteral("navButton"));
    m_friendsNavButton->setCheckable(true);

    m_switchAccountNavButton = new QPushButton(QStringLiteral("切换账号"), rail);
    m_switchAccountNavButton->setObjectName(QStringLiteral("navSecondaryButton"));

    m_signOutNavButton = new QPushButton(QStringLiteral("登出"), rail);
    m_signOutNavButton->setObjectName(QStringLiteral("navSecondaryButton"));

    auto *navHint = new QLabel(QStringLiteral("主导航"), rail);
    navHint->setObjectName(QStringLiteral("navHintLabel"));

    auto *profileCard = new QFrame(rail);
    profileCard->setObjectName(QStringLiteral("navProfileCard"));
    auto *profileLayout = new QVBoxLayout(profileCard);
    profileLayout->setContentsMargins(10, 10, 10, 10);
    profileLayout->setSpacing(3);

    m_profileNameLabel = new QLabel(QStringLiteral("未登录"), profileCard);
    m_profileNameLabel->setObjectName(QStringLiteral("profileName"));
    m_profileStatusLabel = new QLabel(QStringLiteral("等待连接"), profileCard);
    m_profileStatusLabel->setObjectName(QStringLiteral("profileStatus"));

    profileLayout->addWidget(m_profileNameLabel);
    profileLayout->addWidget(m_profileStatusLabel);

    railLayout->addWidget(m_navAvatarLabel, 0, Qt::AlignHCenter);
    railLayout->addWidget(navHint, 0, Qt::AlignHCenter);
    railLayout->addWidget(m_messagesNavButton);
    railLayout->addWidget(m_friendsNavButton);
    railLayout->addStretch(1);
    railLayout->addWidget(m_switchAccountNavButton);
    railLayout->addWidget(m_signOutNavButton);
    railLayout->addWidget(profileCard);

    connect(m_messagesNavButton, &QPushButton::clicked, this, [this]() {
        switchSection(SidebarSection::kMessages);
    });
    connect(m_friendsNavButton, &QPushButton::clicked, this, [this]() {
        switchSection(SidebarSection::kFriends);
    });
    connect(m_switchAccountNavButton, &QPushButton::clicked, this, [this]() {
        emit switchAccountRequested();
    });
    connect(m_signOutNavButton, &QPushButton::clicked, this, [this]() {
        emit signOutRequested();
    });

    return rail;
}

QWidget *ChatWindow::createMiddlePanel()
{
    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("listPanel"));
    panel->setFixedWidth(340);

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    m_middleStack = new QStackedWidget(panel);
    m_middleStack->setObjectName(QStringLiteral("middleStack"));
    m_middleStack->addWidget(createMessagesPage());
    m_middleStack->addWidget(createFriendsPage());

    panelLayout->addWidget(m_middleStack);
    return panel;
}

QWidget *ChatWindow::createMessagesPage()
{
    auto *page = new QWidget(m_middleStack);
    page->setObjectName(QStringLiteral("listPage"));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("消息"), page);
    titleLabel->setObjectName(QStringLiteral("panelHeaderTitle"));
    auto *subtitleLabel =
        new QLabel(QStringLiteral("最近会话与未读消息"), page);
    subtitleLabel->setObjectName(QStringLiteral("panelHeaderSubtitle"));

    m_messageSearchEdit = new QLineEdit(page);
    m_messageSearchEdit->setObjectName(QStringLiteral("panelSearch"));
    m_messageSearchEdit->setPlaceholderText(QStringLiteral("搜索会话"));

    m_sessionList = new QListWidget(page);
    m_sessionList->setObjectName(QStringLiteral("entityList"));
    m_sessionList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *productDiscussion = createRichListItem(
        QStringLiteral("产品讨论组"),
        QStringLiteral("首页需求已更新"),
        m_sessionList);
    productDiscussion->setData(kConversationIdRole, QStringLiteral("product_discussion"));
    productDiscussion->setData(kConversationTitleRole, QStringLiteral("产品讨论组"));
    productDiscussion->setData(kConversationMetaRole,
                               QStringLiteral("3 人在线 · 需求评审中"));

    auto *backendSync = createRichListItem(
        QStringLiteral("后端联调"),
        QStringLiteral("登录和登出接口都已经打通"),
        m_sessionList);
    backendSync->setData(kConversationIdRole, QStringLiteral("backend_sync"));
    backendSync->setData(kConversationTitleRole, QStringLiteral("后端联调"));
    backendSync->setData(kConversationMetaRole,
                         QStringLiteral("接口联调 · 设备会话验证"));

    auto *designReview = createRichListItem(
        QStringLiteral("设计评审"),
        QStringLiteral("左导航和中间栏切换方案待确认"),
        m_sessionList);
    designReview->setData(kConversationIdRole, QStringLiteral("design_review"));
    designReview->setData(kConversationTitleRole, QStringLiteral("设计评审"));
    designReview->setData(kConversationMetaRole,
                          QStringLiteral("视觉方案 · 等待反馈"));

    m_sessionList->setCurrentRow(0);
    connect(m_sessionList,
            &QListWidget::currentItemChanged,
            this,
            [this](QListWidgetItem *, QListWidgetItem *) {
                handleSessionSelectionChanged();
            });

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addWidget(m_messageSearchEdit);
    layout->addWidget(m_sessionList, 1);

    return page;
}

QWidget *ChatWindow::createFriendsPage()
{
    auto *page = new QWidget(m_middleStack);
    page->setObjectName(QStringLiteral("listPage"));

    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("好友"), page);
    titleLabel->setObjectName(QStringLiteral("panelHeaderTitle"));
    auto *subtitleLabel =
        new QLabel(QStringLiteral("联系人、好友申请与添加入口"), page);
    subtitleLabel->setObjectName(QStringLiteral("panelHeaderSubtitle"));

    m_friendSearchEdit = new QLineEdit(page);
    m_friendSearchEdit->setObjectName(QStringLiteral("panelSearch"));
    m_friendSearchEdit->setPlaceholderText(QStringLiteral("搜索好友"));

    m_addFriendButton = new QPushButton(QStringLiteral("添加好友"), page);
    m_addFriendButton->setObjectName(QStringLiteral("panelPrimaryButton"));
    connect(m_addFriendButton, &QPushButton::clicked, this, &ChatWindow::showAddFriendDialog);

    m_friendList = new QListWidget(page);
    m_friendList->setObjectName(QStringLiteral("entityList"));
    m_friendList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *lihua = createRichListItem(QStringLiteral("李华"),
                                     QStringLiteral("产品经理 · 已添加"),
                                     m_friendList);
    lihua->setData(kFriendNameRole, QStringLiteral("李华"));
    lihua->setData(kFriendMetaRole, QStringLiteral("产品经理 · 上次活跃于 10:42"));
    lihua->setData(kFriendHintRole,
                   QStringLiteral("当前只是好友模式骨架，后续这里会接好友资料与发起会话能力。"));

    auto *zhouning = createRichListItem(QStringLiteral("周宁"),
                                        QStringLiteral("前端开发 · 已添加"),
                                        m_friendList);
    zhouning->setData(kFriendNameRole, QStringLiteral("周宁"));
    zhouning->setData(kFriendMetaRole, QStringLiteral("前端开发 · 项目协作中"));
    zhouning->setData(kFriendHintRole,
                      QStringLiteral("后续这里可以补备注名、共同群组和发消息入口。"));

    auto *newRequests = createRichListItem(QStringLiteral("新的朋友"),
                                           QStringLiteral("1 条待处理申请"),
                                           m_friendList);
    newRequests->setData(kFriendNameRole, QStringLiteral("新的朋友"));
    newRequests->setData(kFriendMetaRole, QStringLiteral("待处理申请 · 需要后续接真实接口"));
    newRequests->setData(kFriendHintRole,
                         QStringLiteral("这里后续会接好友申请列表、同意 / 拒绝等操作。"));

    m_friendList->setCurrentRow(0);
    connect(m_friendList,
            &QListWidget::currentItemChanged,
            this,
            [this](QListWidgetItem *, QListWidgetItem *) {
                handleFriendSelectionChanged();
            });

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addWidget(m_friendSearchEdit);
    layout->addWidget(m_addFriendButton);
    layout->addWidget(m_friendList, 1);

    return page;
}

QWidget *ChatWindow::createMessageContentPage()
{
    const auto &config = chatclient::config::AppConfig::instance();

    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("contentPanel"));

    auto *panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(20, 18, 20, 18);
    panelLayout->setSpacing(14);

    auto *header = new QFrame(panel);
    header->setObjectName(QStringLiteral("contentHeader"));
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(10);

    m_conversationTitleLabel = new QLabel(QStringLiteral("产品讨论组"), header);
    m_conversationTitleLabel->setObjectName(QStringLiteral("contentTitle"));
    m_conversationMetaLabel =
        new QLabel(QStringLiteral("3 人在线 · 需求评审中"), header);
    m_conversationMetaLabel->setObjectName(QStringLiteral("contentMeta"));

    auto *titleBlock = new QWidget(header);
    auto *titleLayout = new QVBoxLayout(titleBlock);
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(2);
    titleLayout->addWidget(m_conversationTitleLabel);
    titleLayout->addWidget(m_conversationMetaLabel);

    auto *voiceBtn = new QPushButton(QStringLiteral("语音"), header);
    voiceBtn->setObjectName(QStringLiteral("headerGhostButton"));
    auto *videoBtn = new QPushButton(QStringLiteral("视频"), header);
    videoBtn->setObjectName(QStringLiteral("headerGhostButton"));

    headerLayout->addWidget(titleBlock, 1);
    headerLayout->addWidget(voiceBtn);
    headerLayout->addWidget(videoBtn);

    m_messageListView = new MessageListView(panel);
    m_messageListView->setObjectName(QStringLiteral("messageListView"));
    m_messageModelRegistry = new MessageModelRegistry(this);
    m_messageListView->setMessageModel(
        m_messageModelRegistry->ensureModel(QStringLiteral("product_discussion")));

    auto *composer = new QFrame(panel);
    composer->setObjectName(QStringLiteral("composerPanel"));
    auto *composerLayout = new QVBoxLayout(composer);
    composerLayout->setContentsMargins(12, 12, 12, 12);
    composerLayout->setSpacing(10);

    auto *composerHint = new QLabel(
        QStringLiteral("当前服务地址：%1").arg(config.httpBaseUrlText()),
        composer);
    composerHint->setObjectName(QStringLiteral("composerHintLabel"));

    m_messageEditor = new QTextEdit(composer);
    m_messageEditor->setObjectName(QStringLiteral("messageEditor"));
    m_messageEditor->setPlaceholderText(QStringLiteral("输入消息，按 Enter 发送，Shift+Enter 换行"));
    m_messageEditor->setMinimumHeight(110);

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

    composerLayout->addWidget(composerHint);
    composerLayout->addWidget(m_messageEditor);
    composerLayout->addLayout(actionRow);

    connect(sendBtn, &QPushButton::clicked, this, &ChatWindow::handleSendMessage);
    auto *returnShortcut = new QShortcut(QKeySequence(Qt::Key_Return), m_messageEditor);
    returnShortcut->setContext(Qt::WidgetShortcut);
    connect(returnShortcut, &QShortcut::activated, this, &ChatWindow::handleSendMessage);
    auto *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), m_messageEditor);
    enterShortcut->setContext(Qt::WidgetShortcut);
    connect(enterShortcut, &QShortcut::activated, this, &ChatWindow::handleSendMessage);

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

QWidget *ChatWindow::createFriendContentPage()
{
    auto *panel = new QFrame(this);
    panel->setObjectName(QStringLiteral("contentPanel"));

    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    auto *heroCard = new QFrame(panel);
    heroCard->setObjectName(QStringLiteral("friendHeroCard"));
    auto *heroLayout = new QVBoxLayout(heroCard);
    heroLayout->setContentsMargins(18, 18, 18, 18);
    heroLayout->setSpacing(6);

    m_friendDetailTitleLabel = new QLabel(QStringLiteral("李华"), heroCard);
    m_friendDetailTitleLabel->setObjectName(QStringLiteral("friendDetailTitle"));
    m_friendDetailMetaLabel =
        new QLabel(QStringLiteral("产品经理 · 上次活跃于 10:42"), heroCard);
    m_friendDetailMetaLabel->setObjectName(QStringLiteral("friendDetailMeta"));
    m_friendDetailHintLabel =
        new QLabel(QStringLiteral("这里后续会接好友资料、申请处理和发起会话能力。"),
                   heroCard);
    m_friendDetailHintLabel->setObjectName(QStringLiteral("friendDetailHint"));
    m_friendDetailHintLabel->setWordWrap(true);

    heroLayout->addWidget(m_friendDetailTitleLabel);
    heroLayout->addWidget(m_friendDetailMetaLabel);
    heroLayout->addWidget(m_friendDetailHintLabel);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(10);

    auto *startChatButton = new QPushButton(QStringLiteral("发起会话"), panel);
    startChatButton->setObjectName(QStringLiteral("panelGhostButton"));
    auto *viewProfileButton = new QPushButton(QStringLiteral("查看资料"), panel);
    viewProfileButton->setObjectName(QStringLiteral("panelGhostButton"));

    actionRow->addWidget(startChatButton);
    actionRow->addWidget(viewProfileButton);
    actionRow->addStretch(1);

    auto *placeholderCard = new QFrame(panel);
    placeholderCard->setObjectName(QStringLiteral("friendPlaceholderCard"));
    auto *placeholderLayout = new QVBoxLayout(placeholderCard);
    placeholderLayout->setContentsMargins(18, 18, 18, 18);
    placeholderLayout->setSpacing(6);

    auto *placeholderTitle = new QLabel(QStringLiteral("好友模式"), placeholderCard);
    placeholderTitle->setObjectName(QStringLiteral("friendPlaceholderTitle"));
    auto *placeholderBody = new QLabel(
        QStringLiteral("当前已经完成左侧导航切换、中间栏切换和“添加好友”独立弹窗骨架。后续这里会继续接好友资料、申请列表和发消息入口。"),
        placeholderCard);
    placeholderBody->setObjectName(QStringLiteral("friendPlaceholderBody"));
    placeholderBody->setWordWrap(true);

    placeholderLayout->addWidget(placeholderTitle);
    placeholderLayout->addWidget(placeholderBody);

    layout->addWidget(heroCard);
    layout->addLayout(actionRow);
    layout->addWidget(placeholderCard);
    layout->addStretch(1);

    return panel;
}

void ChatWindow::switchSection(const SidebarSection section)
{
    m_currentSection = section;

    const bool showMessages = section == SidebarSection::kMessages;
    if (m_messagesNavButton) {
        m_messagesNavButton->setChecked(showMessages);
    }
    if (m_friendsNavButton) {
        m_friendsNavButton->setChecked(!showMessages);
    }

    if (m_middleStack) {
        m_middleStack->setCurrentIndex(showMessages ? 0 : 1);
    }
    if (m_contentStack) {
        m_contentStack->setCurrentIndex(showMessages ? 0 : 1);
    }

    if (showMessages) {
        handleSessionSelectionChanged();
    } else {
        handleFriendSelectionChanged();
    }
}

void ChatWindow::handleSessionSelectionChanged()
{
    if (!m_sessionList) {
        return;
    }

    auto *item = m_sessionList->currentItem();
    if (item == nullptr) {
        return;
    }

    m_currentConversationId = item->data(kConversationIdRole).toString();
    if (m_currentConversationId.isEmpty()) {
        return;
    }

    if (m_conversationTitleLabel) {
        m_conversationTitleLabel->setText(
            item->data(kConversationTitleRole).toString());
    }
    if (m_conversationMetaLabel) {
        m_conversationMetaLabel->setText(
            item->data(kConversationMetaRole).toString());
    }

    if (m_messageModelRegistry && m_messageListView) {
        m_messageListView->setMessageModel(
            m_messageModelRegistry->ensureModel(m_currentConversationId));
        m_messageListView->scrollToBottom();
    }
}

void ChatWindow::handleFriendSelectionChanged()
{
    if (!m_friendList) {
        return;
    }

    auto *item = m_friendList->currentItem();
    if (item == nullptr) {
        return;
    }

    if (m_friendDetailTitleLabel) {
        m_friendDetailTitleLabel->setText(
            item->data(kFriendNameRole).toString());
    }
    if (m_friendDetailMetaLabel) {
        m_friendDetailMetaLabel->setText(
            item->data(kFriendMetaRole).toString());
    }
    if (m_friendDetailHintLabel) {
        m_friendDetailHintLabel->setText(
            item->data(kFriendHintRole).toString());
    }
}

void ChatWindow::showAddFriendDialog()
{
    AddFriendDialog dialog(this);
    dialog.exec();
}

void ChatWindow::appendMessage(const QString &author,
                               const QString &text,
                               const bool fromSelf)
{
    if (!m_messageListView || !m_messageModelRegistry || m_currentConversationId.isEmpty()) {
        return;
    }

    m_messageModelRegistry->addTextMessage(m_currentConversationId,
                                           author,
                                           text,
                                           QTime::currentTime().toString(QStringLiteral("HH:mm")),
                                           fromSelf);
    m_messageListView->scrollToBottom();
}

void ChatWindow::handleSendMessage()
{
    if (!m_messageEditor || m_currentSection != SidebarSection::kMessages) {
        return;
    }

    const QString text = m_messageEditor->toPlainText().trimmed();
    if (text.isEmpty()) {
        return;
    }

    appendMessage(QStringLiteral("我"), text, true);
    m_messageEditor->clear();
}
