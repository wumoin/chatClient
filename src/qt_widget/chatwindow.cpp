#include "chatwindow.h"

#include "api/conversation_api_client.h"
#include "api/user_api_client.h"
#include "config/appconfig.h"
#include "dto/conversation_dto.h"
#include "log/app_logger.h"
#include "model/messagemodel.h"
#include "model/messagemodelregistry.h"
#include "qt_widget/addfrienddialog.h"
#include "service/auth_service.h"
#include "service/friend_service.h"
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
constexpr int kFriendUserIdRole = Qt::UserRole + 7;
constexpr int kFriendAvatarStorageKeyRole = Qt::UserRole + 8;

QString localizeCreateConversationError(
    const chatclient::dto::conversation::ApiErrorDto &error)
{
    switch (error.errorCode)
    {
    case 40102:
        return QStringLiteral("当前登录状态已失效，请重新登录");
    case 40300:
        return QStringLiteral("当前无法和该用户建立私聊");
    case 40400:
        return QStringLiteral("目标会话或目标用户不存在");
    case 40001:
        return QStringLiteral("发起私聊请求参数不正确");
    default:
        break;
    }

    if (error.message == QStringLiteral("peer user not found"))
    {
        return QStringLiteral("目标用户不存在");
    }

    if (error.message == QStringLiteral("peer user is not your friend"))
    {
        return QStringLiteral("当前只能和好友建立私聊");
    }

    if (error.message == QStringLiteral("invalid access token"))
    {
        return QStringLiteral("当前登录状态已失效，请重新登录");
    }

    return QStringLiteral("建立私聊失败，请稍后重试");
}

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
    m_conversationApiClient = new chatclient::api::ConversationApiClient(this);
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

void ChatWindow::setAuthService(chatclient::service::AuthService *authService)
{
    m_authService = authService;

    if (m_friendService)
    {
        m_friendService->deleteLater();
        m_friendService = nullptr;
    }

    if (!m_authService)
    {
        return;
    }

    m_friendService = new chatclient::service::FriendService(m_authService, this);
    connect(m_friendService,
            &chatclient::service::FriendService::friendsStarted,
            this,
            [this]() {
                if (m_friendList)
                {
                    m_friendList->setEnabled(false);
                    m_friendList->clear();
                    auto *loadingItem = new QListWidgetItem(
                        QStringLiteral("正在加载好友列表..."),
                        m_friendList);
                    loadingItem->setFlags(Qt::NoItemFlags);
                }
                if (m_friendDetailTitleLabel)
                {
                    m_friendDetailTitleLabel->setText(QStringLiteral("好友"));
                }
                if (m_friendDetailMetaLabel)
                {
                    m_friendDetailMetaLabel->setText(QStringLiteral("同步中"));
                }
                if (m_friendDetailHintLabel)
                {
                    m_friendDetailHintLabel->setText(
                        QStringLiteral("正在从服务端刷新当前账号的好友列表。"));
                }
            });
    connect(m_friendService,
            &chatclient::service::FriendService::friendsSucceeded,
            this,
            [this](const chatclient::dto::friendship::FriendListItems &friends) {
                updateFriendList(friends, true);
            });
    connect(m_friendService,
            &chatclient::service::FriendService::friendsFailed,
            this,
            [this](const QString &message) {
                if (m_friendList)
                {
                    m_friendList->setEnabled(true);
                    m_friendList->clear();
                    auto *errorItem = new QListWidgetItem(message, m_friendList);
                    errorItem->setFlags(Qt::NoItemFlags);
                }
                m_currentSelectedFriendUserId.clear();
                if (m_friendDetailTitleLabel)
                {
                    m_friendDetailTitleLabel->setText(QStringLiteral("好友"));
                }
                if (m_friendDetailMetaLabel)
                {
                    m_friendDetailMetaLabel->setText(QStringLiteral("加载失败"));
                }
                if (m_friendDetailHintLabel)
                {
                    m_friendDetailHintLabel->setText(message);
                }
            });

    refreshFriendList(false);
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

    auto *placeholderItem =
        new QListWidgetItem(QStringLiteral("正在准备好友列表..."), m_friendList);
    placeholderItem->setFlags(Qt::NoItemFlags);
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
    auto *heroLayout = new QHBoxLayout(heroCard);
    heroLayout->setContentsMargins(18, 18, 18, 18);
    heroLayout->setSpacing(18);

    auto *textBlock = new QWidget(heroCard);
    auto *textLayout = new QVBoxLayout(textBlock);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(6);

    m_friendDetailTitleLabel = new QLabel(QStringLiteral("李华"), textBlock);
    m_friendDetailTitleLabel->setObjectName(QStringLiteral("friendDetailTitle"));
    m_friendDetailMetaLabel =
        new QLabel(QStringLiteral("产品经理 · 上次活跃于 10:42"), textBlock);
    m_friendDetailMetaLabel->setObjectName(QStringLiteral("friendDetailMeta"));
    m_friendDetailHintLabel =
        new QLabel(QStringLiteral("这里后续会接好友资料、申请处理和发起会话能力。"),
                   textBlock);
    m_friendDetailHintLabel->setObjectName(QStringLiteral("friendDetailHint"));
    m_friendDetailHintLabel->setWordWrap(true);

    textLayout->addWidget(m_friendDetailTitleLabel);
    textLayout->addWidget(m_friendDetailMetaLabel);
    textLayout->addWidget(m_friendDetailHintLabel);

    m_friendDetailAvatarLabel = new QLabel(QStringLiteral("李华"), heroCard);
    m_friendDetailAvatarLabel->setObjectName(
        QStringLiteral("friendDetailAvatar"));
    m_friendDetailAvatarLabel->setAlignment(Qt::AlignCenter);
    m_friendDetailAvatarLabel->setFixedSize(88, 88);

    heroLayout->addWidget(textBlock, 1);
    heroLayout->addWidget(m_friendDetailAvatarLabel, 0, Qt::AlignTop);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(10);

    m_startChatButton = new QPushButton(QStringLiteral("发起会话"), panel);
    m_startChatButton->setObjectName(QStringLiteral("panelGhostButton"));
    auto *viewProfileButton = new QPushButton(QStringLiteral("查看资料"), panel);
    viewProfileButton->setObjectName(QStringLiteral("panelGhostButton"));

    actionRow->addWidget(m_startChatButton);
    actionRow->addWidget(viewProfileButton);
    actionRow->addStretch(1);

    connect(m_startChatButton,
            &QPushButton::clicked,
            this,
            &ChatWindow::handleStartPrivateConversation);

    // auto *placeholderCard = new QFrame(panel);
    // placeholderCard->setObjectName(QStringLiteral("friendPlaceholderCard"));
    // auto *placeholderLayout = new QVBoxLayout(placeholderCard);
    // placeholderLayout->setContentsMargins(18, 18, 18, 18);
    // placeholderLayout->setSpacing(6);

    // auto *placeholderTitle = new QLabel(QStringLiteral("好友模式"), placeholderCard);
    // placeholderTitle->setObjectName(QStringLiteral("friendPlaceholderTitle"));
    // auto *placeholderBody = new QLabel(
    //     QStringLiteral("当前已经完成左侧导航切换、中间栏切换和“添加好友”独立弹窗骨架。后续这里会继续接好友资料、申请列表和发消息入口。"),
    //     placeholderCard);
    // placeholderBody->setObjectName(QStringLiteral("friendPlaceholderBody"));
    // placeholderBody->setWordWrap(true);

    //placeholderLayout->addWidget(placeholderTitle);
    //placeholderLayout->addWidget(placeholderBody);

    layout->addWidget(heroCard);
    layout->addLayout(actionRow);
    //layout->addWidget(placeholderCard);
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
        refreshFriendList(true);
        handleFriendSelectionChanged();
    }
}

void ChatWindow::handleStartPrivateConversation()
{
    if (!m_startChatButton || !m_friendDetailHintLabel)
    {
        return;
    }

    if (!m_authService || !m_authService->hasActiveSession())
    {
        m_friendDetailHintLabel->setText(
            QStringLiteral("当前登录状态不可用，请重新登录后再发起私聊。"));
        return;
    }

    const QString peerUserId = m_currentSelectedFriendUserId.trimmed();
    if (peerUserId.isEmpty())
    {
        m_friendDetailHintLabel->setText(
            QStringLiteral("请先在好友列表中选择一个好友。"));
        return;
    }

    if (!m_conversationApiClient)
    {
        m_friendDetailHintLabel->setText(
            QStringLiteral("会话接口客户端尚未初始化。"));
        return;
    }

    const QString peerName =
        m_friendDetailTitleLabel ? m_friendDetailTitleLabel->text().trimmed()
                                 : QString();
    chatclient::dto::conversation::CreatePrivateConversationRequestDto request;
    request.peerUserId = peerUserId;

    m_startChatButton->setEnabled(false);
    m_startChatButton->setText(QStringLiteral("创建中..."));
    m_friendDetailHintLabel->setText(
        QStringLiteral("正在为你和 %1 创建或复用私聊会话。")
            .arg(peerName.isEmpty() ? QStringLiteral("该好友") : peerName));

    const QString accessToken =
        m_authService->currentSession().accessToken.trimmed();
    m_conversationApiClient->createPrivateConversation(
        accessToken,
        request,
        [this, peerName](
            const chatclient::dto::conversation::
                CreatePrivateConversationResponseDto &response) {
            if (m_startChatButton)
            {
                m_startChatButton->setEnabled(true);
                m_startChatButton->setText(QStringLiteral("发起会话"));
            }

            m_currentConversationId = response.conversation.conversationId;

            CHATCLIENT_LOG_INFO("chat.window")
                << "private conversation created request_id="
                << response.requestId
                << " conversation_id="
                << response.conversation.conversationId
                << " peer_user_id="
                << response.conversation.peerUser.userId;

            if (m_friendDetailHintLabel)
            {
                m_friendDetailHintLabel->setText(
                    QStringLiteral("已为你和 %1 建立私聊，会话 ID：%2。后续接入真实会话列表后会在消息页展示。")
                        .arg(peerName.isEmpty()
                                 ? response.conversation.peerUser.nickname
                                 : peerName,
                             response.conversation.conversationId));
            }
        },
        [this](const chatclient::dto::conversation::ApiErrorDto &error) {
            if (m_startChatButton)
            {
                m_startChatButton->setEnabled(true);
                m_startChatButton->setText(QStringLiteral("发起会话"));
            }

            CHATCLIENT_LOG_WARN("chat.window")
                << "failed to create private conversation request_id="
                << error.requestId
                << " http_status="
                << error.httpStatus
                << " error_code="
                << error.errorCode
                << " message="
                << error.message;

            if (m_friendDetailHintLabel)
            {
                m_friendDetailHintLabel->setText(
                    localizeCreateConversationError(error));
            }
        });
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
        m_currentSelectedFriendUserId.clear();
        updateFriendDetailAvatar(QStringLiteral("好友"));
        if (m_friendDetailTitleLabel) {
            m_friendDetailTitleLabel->setText(QStringLiteral("好友详情"));
        }
        if (m_friendDetailMetaLabel) {
            m_friendDetailMetaLabel->setText(QStringLiteral("请选择一个好友"));
        }
        if (m_friendDetailHintLabel) {
            m_friendDetailHintLabel->setText(
                QStringLiteral("这里会展示当前选中好友的资料摘要，以及后续的发消息和查看资料入口。"));
        }
        return;
    }

    m_currentSelectedFriendUserId =
        item->data(kFriendUserIdRole).toString();
    const QString friendName = item->data(kFriendNameRole).toString();
    const QString friendAvatarStorageKey =
        item->data(kFriendAvatarStorageKeyRole).toString().trimmed();
    updateFriendDetailAvatar(friendName);
    if (m_friendDetailTitleLabel) {
        m_friendDetailTitleLabel->setText(friendName);
    }
    if (m_friendDetailMetaLabel) {
        m_friendDetailMetaLabel->setText(
            item->data(kFriendMetaRole).toString());
    }
    if (m_friendDetailHintLabel) {
        m_friendDetailHintLabel->setText(
            item->data(kFriendHintRole).toString());
    }

    if (!m_userApiClient || m_currentSelectedFriendUserId.isEmpty() ||
        friendAvatarStorageKey.isEmpty())
    {
        return;
    }

    const QString expectedUserId = m_currentSelectedFriendUserId;
    m_userApiClient->downloadUserAvatar(
        expectedUserId,
        [this, expectedUserId](const QByteArray &data) {
            if (expectedUserId != m_currentSelectedFriendUserId)
            {
                return;
            }

            QImage image;
            if (!image.loadFromData(data))
            {
                CHATCLIENT_LOG_WARN("chat.window")
                    << "failed to decode friend avatar image for user_id="
                    << expectedUserId;
                return;
            }

            updateFriendDetailAvatarImage(image);
        },
        [expectedUserId](const chatclient::dto::user::ApiErrorDto &error) {
            CHATCLIENT_LOG_WARN("chat.window")
                << "failed to download friend avatar user_id="
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

void ChatWindow::showAddFriendDialog()
{
    AddFriendDialog dialog(m_authService, this);
    dialog.exec();
    refreshFriendList(true);
}

void ChatWindow::refreshFriendList(const bool keepSelection)
{
    if (!m_friendService) {
        return;
    }

    if (!keepSelection) {
        m_currentSelectedFriendUserId.clear();
    }

    QString errorMessage;
    if (!m_friendService->fetchFriends(&errorMessage) &&
        !errorMessage.trimmed().isEmpty() && m_friendList)
    {
        m_friendList->setEnabled(true);
        m_friendList->clear();
        auto *errorItem = new QListWidgetItem(errorMessage, m_friendList);
        errorItem->setFlags(Qt::NoItemFlags);
    }
}

void ChatWindow::updateFriendList(
    const chatclient::dto::friendship::FriendListItems &friends,
    bool keepSelection)
{
    if (!m_friendList) {
        return;
    }

    const QString previousUserId =
        keepSelection ? m_currentSelectedFriendUserId : QString();

    m_friendList->setEnabled(true);
    m_friendList->clear();

    if (friends.isEmpty())
    {
        auto *emptyItem = new QListWidgetItem(
            QStringLiteral("当前还没有好友，先去添加一个吧"),
            m_friendList);
        emptyItem->setFlags(Qt::NoItemFlags);
        m_currentSelectedFriendUserId.clear();
        handleFriendSelectionChanged();
        return;
    }

    int targetRow = -1;
    for (int index = 0; index < friends.size(); ++index)
    {
        const auto &friendItem = friends.at(index);
        auto *item = createRichListItem(
            friendItem.user.nickname,
            QStringLiteral("账号：%1").arg(friendItem.user.account),
            m_friendList);
        item->setData(kFriendUserIdRole, friendItem.user.userId);
        item->setData(kFriendNameRole, friendItem.user.nickname);
        item->setData(kFriendAvatarStorageKeyRole, friendItem.user.avatarUrl);
        item->setData(
            kFriendMetaRole,
            QStringLiteral("账号：%1 · 用户 ID：%2")
                .arg(friendItem.user.account, friendItem.user.userId));
        item->setData(
            kFriendHintRole,
            QStringLiteral("当前已接入真实好友列表；后续这里可以继续补发消息、备注名和共同群组。"));

        if (!previousUserId.isEmpty() &&
            previousUserId == friendItem.user.userId)
        {
            targetRow = index;
        }
    }

    if (targetRow < 0) {
        targetRow = 0;
    }

    m_friendList->setCurrentRow(targetRow);
    handleFriendSelectionChanged();
}

void ChatWindow::updateFriendDetailAvatar(const QString &displayName)
{
    if (!m_friendDetailAvatarLabel)
    {
        return;
    }

    const QString trimmedName = displayName.trimmed();
    const QString fallbackText = trimmedName.isEmpty()
                                     ? QStringLiteral("好友")
                                     : trimmedName.left(2);
    m_friendDetailAvatarLabel->setPixmap(QPixmap());
    m_friendDetailAvatarLabel->setText(fallbackText);
    m_friendDetailAvatarLabel->setToolTip(trimmedName);
}

void ChatWindow::updateFriendDetailAvatarImage(const QImage &image)
{
    if (!m_friendDetailAvatarLabel)
    {
        return;
    }

    const QPixmap avatarPixmap =
        createRoundedAvatarPixmap(image, m_friendDetailAvatarLabel->size());
    if (avatarPixmap.isNull())
    {
        return;
    }

    m_friendDetailAvatarLabel->setPixmap(avatarPixmap);
    m_friendDetailAvatarLabel->setText(QString());
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
