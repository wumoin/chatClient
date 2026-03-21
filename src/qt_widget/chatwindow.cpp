#include "chatwindow.h"

#include "api/user_api_client.h"
#include "config/appconfig.h"
#include "dto/conversation_dto.h"
#include "log/app_logger.h"
#include "model/conversationlistmodel.h"
#include "model/messagemodel.h"
#include "qt_widget/addfrienddialog.h"
#include "service/auth_service.h"
#include "service/conversation_manager.h"
#include "service/friend_service.h"
#include "view/messagelistview.h"

#include <QDesktopServices>
#include <QAbstractItemView>
#include <QCloseEvent>
#include <QColor>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QTextCursor>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <optional>

namespace {

constexpr int kConversationIdRole = Qt::UserRole + 1;
constexpr int kConversationTitleRole = Qt::UserRole + 2;
constexpr int kConversationMetaRole = Qt::UserRole + 3;
constexpr int kFriendNameRole = Qt::UserRole + 4;
constexpr int kFriendMetaRole = Qt::UserRole + 5;
constexpr int kFriendHintRole = Qt::UserRole + 6;
constexpr int kFriendUserIdRole = Qt::UserRole + 7;
constexpr int kFriendAvatarStorageKeyRole = Qt::UserRole + 8;
constexpr int kConversationPeerUserIdRole = Qt::UserRole + 9;
constexpr int kConversationAvatarStorageKeyRole = Qt::UserRole + 10;
// 用于“当前没有真实会话选中”时挂一份空 MessageModel，避免右侧 view 解绑。
const auto kEmptyConversationId = "__empty_conversation__";

struct FileMessageUiContext
{
    MessageItem identity;
    QString fileName;
    QString localPath;
    QString remoteUrl;
    QString mimeType;
    qint64 sizeBytes = -1;
    MessageTransferState transferState = MessageTransferState::None;
};

QStringList defaultEmojiList()
{
    // 当前先提供一组聊天场景里最常用的 emoji。
    // 这里保持静态常量列表，避免每次点开表情面板都重新拼装一份数据。
    return {
        QStringLiteral("😀"),
        QStringLiteral("😁"),
        QStringLiteral("😂"),
        QStringLiteral("🤣"),
        QStringLiteral("😊"),
        QStringLiteral("😍"),
        QStringLiteral("😘"),
        QStringLiteral("🥳"),
        QStringLiteral("😎"),
        QStringLiteral("🤔"),
        QStringLiteral("😭"),
        QStringLiteral("😡"),
        QStringLiteral("👍"),
        QStringLiteral("👎"),
        QStringLiteral("👏"),
        QStringLiteral("🙏"),
        QStringLiteral("🎉"),
        QStringLiteral("❤️"),
        QStringLiteral("🔥"),
        QStringLiteral("✨"),
        QStringLiteral("🌹"),
        QStringLiteral("🍀"),
        QStringLiteral("☕"),
        QStringLiteral("🎁"),
    };
}

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

QPixmap createTextAvatarPixmap(const QString &displayName, const QSize &size);

QListWidgetItem *createRichListItem(const QString &title,
                                    const QString &subtitle,
                                    QListWidget *parent)
{
    // 好友中间栏当前仍是轻量过渡实现：直接把标题 / 副标题压进 QListWidgetItem。
    auto *item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(title, subtitle),
                                     parent);
    item->setSizeHint(QSize(0, 64));
    return item;
}

QListWidgetItem *createConversationListItem(const QString &title,
                                            const QString &preview,
                                            const qint64 unreadCount,
                                            QListWidget *parent)
{
    // 当前中间栏还在使用 QListWidget，因此每条会话摘要会在这里投影成一个独立的 item widget：
    // 左侧头像，中间标题+最后一条消息预览，右侧未读数角标。
    auto *item = new QListWidgetItem(parent);
    item->setSizeHint(QSize(0, 72));

    auto *container = new QWidget(parent);
    container->setObjectName(QStringLiteral("conversationListItemWidget"));
    container->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(10);

    auto *avatarLabel = new QLabel(container);
    avatarLabel->setObjectName(QStringLiteral("conversationAvatarLabel"));
    avatarLabel->setFixedSize(40, 40);
    avatarLabel->setPixmap(createTextAvatarPixmap(title, QSize(40, 40)));

    auto *textBlock = new QWidget(container);
    auto *textLayout = new QVBoxLayout(textBlock);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    auto *titleLabel = new QLabel(title.trimmed(), textBlock);
    titleLabel->setObjectName(QStringLiteral("conversationItemTitleLabel"));

    int pos = preview.indexOf('\n');
    QString firstLine = (pos == -1) ? preview : preview.left(pos);
    auto *previewLabel = new QLabel(
        preview.trimmed().isEmpty() ? QStringLiteral("暂无历史消息")
                                    : firstLine.trimmed(),
        textBlock);
    previewLabel->setObjectName(QStringLiteral("conversationItemPreviewLabel"));
    previewLabel->setWordWrap(false);

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(previewLabel);

    auto *badgeLabel = new QLabel(container);
    badgeLabel->setObjectName(QStringLiteral("conversationUnreadBadge"));
    badgeLabel->setAlignment(Qt::AlignCenter);
    if (unreadCount > 0)
    {
        badgeLabel->setText(unreadCount > 99 ? QStringLiteral("99+")
                                             : QString::number(unreadCount));
    }
    else
    {
        badgeLabel->hide();
    }

    layout->addWidget(avatarLabel, 0, Qt::AlignVCenter);
    layout->addWidget(textBlock, 1);
    layout->addWidget(badgeLabel, 0, Qt::AlignVCenter);

    parent->setItemWidget(item, container);
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

QString avatarText(const QString &displayName)
{
    const QString trimmedName = displayName.trimmed();
    if (trimmedName.isEmpty()) {
        return QStringLiteral("会话");
    }

    return trimmedName.size() <= 2 ? trimmedName : trimmedName.left(2);
}

QPixmap createTextAvatarPixmap(const QString &displayName, const QSize &size)
{
    if (!size.isValid()) {
        return QPixmap();
    }

    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0xD9, 0xE7, 0xFF));
    painter.drawEllipse(pixmap.rect());

    QFont font = painter.font();
    font.setBold(true);
    font.setPixelSize(qMax(14, size.height() / 3));
    painter.setFont(font);
    painter.setPen(QColor(0x21, 0x4A, 0x7A));
    painter.drawText(pixmap.rect(), Qt::AlignCenter, avatarText(displayName));
    return pixmap;
}

std::optional<FileMessageUiContext> fileMessageUiContextFromIndex(
    const QModelIndex &index)
{
    if (!index.isValid() ||
        index.data(MessageModel::MessageTypeRole).toInt() !=
            static_cast<int>(MessageType::File))
    {
        return std::nullopt;
    }

    FileMessageUiContext context;
    context.identity.messageId =
        index.data(MessageModel::MessageIdRole).toString().trimmed();
    context.identity.clientMessageId =
        index.data(MessageModel::ClientMessageIdRole).toString().trimmed();
    context.identity.seq =
        index.data(MessageModel::SeqRole).toLongLong();
    context.fileName =
        index.data(MessageModel::FileNameRole).toString().trimmed();
    context.localPath =
        index.data(MessageModel::FileLocalPathRole).toString().trimmed();
    context.remoteUrl =
        index.data(MessageModel::FileRemoteUrlRole).toString().trimmed();
    context.mimeType =
        index.data(MessageModel::FileMimeTypeRole).toString().trimmed();
    context.sizeBytes =
        index.data(MessageModel::FileSizeBytesRole).toLongLong();
    context.transferState = static_cast<MessageTransferState>(
        index.data(MessageModel::TransferStateRole).toInt());

    return context;
}

}  // namespace

ChatWindow::ChatWindow(QWidget *parent)
    : QWidget(parent)
{
    const auto &config = chatclient::config::AppConfig::instance();
    m_conversationManager = new chatclient::service::ConversationManager(this);
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

    connect(m_conversationManager,
            &chatclient::service::ConversationManager::realtimeStatusChanged,
            this,
            &ChatWindow::updateRealtimeStatus);
    connect(m_conversationManager,
            &chatclient::service::ConversationManager::realtimeAuthenticated,
            this,
            [this](const QString &userId, const QString &deviceSessionId) {
                CHATCLIENT_LOG_INFO("chat.window")
                    << "实时通道认证成功，user_id="
                    << userId
                    << " device_session_id="
                    << deviceSessionId;
            });
    connect(m_conversationManager,
            &chatclient::service::ConversationManager::realtimeAuthenticationFailed,
            this,
            [this](const QString &message) {
                CHATCLIENT_LOG_WARN("chat.window")
                    << "实时通道认证失败，message="
                    << message;
                if (m_friendDetailHintLabel &&
                    m_currentSection == SidebarSection::kMessages)
                {
                    m_friendDetailHintLabel->setText(message);
                }
            });
    connect(m_conversationManager,
            &chatclient::service::ConversationManager::conversationBootstrapStarted,
            this,
            [this]() { showSessionPlaceholder(QStringLiteral("正在同步会话列表...")); });
    connect(m_conversationManager,
            &chatclient::service::ConversationManager::conversationListUpdated,
            this,
            [this]() { updateSessionListFromManager(true); },
            Qt::QueuedConnection);
    connect(m_conversationManager,
            &chatclient::service::ConversationManager::conversationBootstrapFailed,
            this,
            [this](const QString &message) { showSessionPlaceholder(message); });
    connect(m_conversationManager,
            &chatclient::service::ConversationManager::realtimeNewEventReceived,
            this,
            [this](const QString &route, const QJsonObject &) {
                if (route == QStringLiteral("friend.request.accepted"))
                {
                    CHATCLIENT_LOG_INFO("chat.window")
                        << "收到好友申请通过实时事件，开始刷新好友列表";
                    refreshFriendList(true);
                    return;
                }

                if (route == QStringLiteral("friend.request.new") ||
                    route == QStringLiteral("friend.request.rejected") ||
                    route == QStringLiteral("conversation.created"))
                {
                    CHATCLIENT_LOG_INFO("chat.window")
                        << "收到实时业务事件，route=" << route;
                }
            });

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

    // 当前用户资料一旦可用，就可以尝试做两件事：
    // 1. 建立实时通道；
    // 2. 触发会话 bootstrap。
    // 这两个动作都依赖已经拿到有效登录态和当前用户身份。
    connectRealtimeChannel();
    if (m_conversationManager)
    {
        m_conversationManager->initializeConversationDataIfNeeded();
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
                    << "解析当前用户头像图片失败，user_id="
                    << expectedUserId;
                return;
            }

            updateProfileAvatarImage(image);
        },
        [expectedUserId](const chatclient::dto::user::ApiErrorDto &error) {
            CHATCLIENT_LOG_WARN("chat.window")
                << "下载当前用户头像失败，user_id="
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
    if (m_conversationManager)
    {
        m_conversationManager->setAuthService(m_authService);
    }

    if (m_friendService)
    {
        m_friendService->deleteLater();
        m_friendService = nullptr;
    }

    if (!m_authService)
    {
        if (m_conversationManager)
        {
            m_conversationManager->disconnectRealtimeChannel();
        }
        return;
    }

    // FriendService 跟当前登录态强绑定：
    // 切账号时直接重建一份，避免继续持有旧 access token / 旧用户上下文。
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
    connectRealtimeChannel();
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
    // 中间栏只负责“列表模式切换”：
    // 消息模式显示会话列表，好友模式显示好友列表。
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

    // 会话中间栏当前仍是 QWidget 投影层：
    // ConversationManager 内部维护真正的 ConversationListModel，这里只负责展示。
    m_sessionList = new QListWidget(page);
    m_sessionList->setObjectName(QStringLiteral("entityList"));
    m_sessionList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_sessionList->setIconSize(QSize(40, 40));
    showSessionPlaceholder(QStringLiteral("正在准备会话列表..."));
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

    // 好友中间栏直接展示 FriendService 最近一次拉回来的好友快照。
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

    // 右侧消息页由三部分组成：
    // 1. 顶部会话头部
    // 2. 中间消息列表
    // 3. 底部输入区
    // 这些控件后续会被切换会话、WS 事件和发送状态反复修改，因此统一存成 ChatWindow 成员。
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

    m_conversationVoiceButton = new QPushButton(QStringLiteral("语音"), header);
    m_conversationVoiceButton->setObjectName(
        QStringLiteral("headerGhostButton"));
    m_conversationVideoButton = new QPushButton(QStringLiteral("视频"), header);
    m_conversationVideoButton->setObjectName(
        QStringLiteral("headerGhostButton"));

    headerLayout->addWidget(titleBlock, 1);
    headerLayout->addWidget(m_conversationVoiceButton);
    headerLayout->addWidget(m_conversationVideoButton);

    m_messageListView = new MessageListView(panel);
    m_messageListView->setObjectName(QStringLiteral("messageListView"));
    // 初始先绑定一份“空会话”模型，等左侧真实会话选中后再切到对应 conversation_id。
    m_messageListView->setMessageModel(
        m_conversationManager->ensureMessageModel(
            QString::fromLatin1(kEmptyConversationId)));

    auto *composer = new QFrame(panel);
    composer->setObjectName(QStringLiteral("composerPanel"));
    auto *composerLayout = new QVBoxLayout(composer);
    composerLayout->setContentsMargins(12, 12, 12, 12);
    composerLayout->setSpacing(10);

    m_messageComposerHintLabel = new QLabel(
        QStringLiteral("当前服务地址：%1").arg(config.httpBaseUrlText()),
        composer);
    m_messageComposerHintLabel->setObjectName(
        QStringLiteral("composerHintLabel"));

    m_messageEditor = new QTextEdit(composer);
    m_messageEditor->setObjectName(QStringLiteral("messageEditor"));
    m_messageEditor->setPlaceholderText(QStringLiteral("输入消息，按 Enter 发送，Shift+Enter 换行"));
    m_messageEditor->setMinimumHeight(110);
    m_messageEditor->installEventFilter(this);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);

    m_messageEmojiButton = new QPushButton(QStringLiteral("表情"), composer);
    m_messageEmojiButton->setObjectName(QStringLiteral("composerGhostButton"));
    m_messageFileButton = new QPushButton(QStringLiteral("附件"), composer);
    m_messageFileButton->setObjectName(QStringLiteral("composerGhostButton"));
    m_messageSendButton = new QPushButton(QStringLiteral("发送"), composer);
    m_messageSendButton->setObjectName(QStringLiteral("sendButton"));

    actionRow->addWidget(m_messageEmojiButton);
    actionRow->addWidget(m_messageFileButton);
    actionRow->addStretch(1);
    actionRow->addWidget(m_messageSendButton);

    composerLayout->addWidget(m_messageComposerHintLabel);
    composerLayout->addWidget(m_messageEditor);
    composerLayout->addLayout(actionRow);

    connect(m_messageSendButton,
            &QPushButton::clicked,
            this,
            &ChatWindow::handleSendMessage);
    connect(m_messageFileButton,
            &QPushButton::clicked,
            this,
            &ChatWindow::handleSendAttachment);
    connect(m_messageEmojiButton,
            &QPushButton::clicked,
            this,
            &ChatWindow::toggleEmojiPicker);
    connect(m_messageListView,
            &MessageListView::fileMessageActivated,
            this,
            &ChatWindow::handleFileMessageActivated);
    connect(m_messageListView,
            &MessageListView::fileMessageDownloadRequested,
            this,
            &ChatWindow::handleFileMessageDownloadRequested);
    connect(m_messageListView,
            &MessageListView::fileMessageDownloadToRequested,
            this,
            &ChatWindow::handleFileMessageDownloadToRequested);
    connect(m_messageListView,
            &MessageListView::fileMessageOpenRequested,
            this,
            &ChatWindow::handleFileMessageOpenRequested);
    connect(m_messageListView,
            &MessageListView::fileMessageOpenFolderRequested,
            this,
            &ChatWindow::handleFileMessageOpenFolderRequested);

    setConversationHeaderText(QStringLiteral("产品讨论组"),
                              QStringLiteral("3 人在线 · 需求评审中"));
    setConversationComposerHintText(
        QStringLiteral("当前服务地址：%1").arg(config.httpBaseUrlText()));

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

bool ChatWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_messageEditor && event &&
        event->type() == QEvent::KeyPress)
    {
        // 输入框里采用聊天类应用常见约定：
        // Enter 发送；Shift+Enter 保留为换行。
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        const int key = keyEvent->key();
        const Qt::KeyboardModifiers modifiers = keyEvent->modifiers();
        const bool isPlainEnter =
            (key == Qt::Key_Return || key == Qt::Key_Enter) &&
            !modifiers.testFlag(Qt::ShiftModifier) &&
            !modifiers.testFlag(Qt::ControlModifier) &&
            !modifiers.testFlag(Qt::AltModifier) &&
            !modifiers.testFlag(Qt::MetaModifier);
        if (isPlainEnter)
        {
            handleSendMessage();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
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

    layout->addWidget(heroCard);
    layout->addLayout(actionRow);
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

    // 切模式时立即刷新当前页数据：
    // - 消息页依赖 ConversationManager 当前快照；
    // - 好友页依赖 FriendService 当前快照 / 拉取结果。
    if (showMessages) {
        updateSessionListFromManager(true);
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

    if (!m_conversationManager)
    {
        m_friendDetailHintLabel->setText(
            QStringLiteral("会话接口客户端尚未初始化。"));
        return;
    }

    const QString peerName =
        m_friendDetailTitleLabel ? m_friendDetailTitleLabel->text().trimmed()
                                 : QString();

    m_startChatButton->setEnabled(false);
    m_startChatButton->setText(QStringLiteral("创建中..."));
    m_friendDetailHintLabel->setText(
        QStringLiteral("正在为你和 %1 创建或复用私聊会话。")
            .arg(peerName.isEmpty() ? QStringLiteral("该好友") : peerName));

    // “发起会话”按钮本身不直接操作会话列表。
    // 真正的建会话、复用旧会话、写回本地列表都由 ConversationManager 决定。
    m_conversationManager->createPrivateConversation(
        peerUserId,
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
                << "私聊会话创建成功，request_id="
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
                << "创建私聊会话失败，request_id="
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

void ChatWindow::connectRealtimeChannel()
{
    if (!m_conversationManager)
    {
        return;
    }

    if (!m_authService || !m_authService->hasActiveSession())
    {
        m_conversationManager->disconnectRealtimeChannel();
        return;
    }

    m_conversationManager->connectRealtimeChannel();
}

void ChatWindow::updateRealtimeStatus(const QString &statusText)
{
    if (m_profileStatusLabel && !statusText.trimmed().isEmpty())
    {
        m_profileStatusLabel->setText(statusText);
    }
}

void ChatWindow::showSessionPlaceholder(const QString &message)
{
    if (!m_sessionList) {
        return;
    }

    // 占位态不仅更新中间栏文案，还要把右侧聊天区切回空模型，
    // 避免仍然显示上一条会话的消息内容。
    m_sessionList->clear();
    auto *item = new QListWidgetItem(message, m_sessionList);
    item->setFlags(Qt::NoItemFlags);

    m_currentConversationId.clear();
    setConversationHeaderText(QStringLiteral("消息"),
                              QStringLiteral("等待会话同步"));
    if (m_messageListView && m_conversationManager) {
        m_messageListView->setMessageModel(
            m_conversationManager->ensureMessageModel(
                QString::fromLatin1(kEmptyConversationId)));
    }
}

void ChatWindow::updateSessionListFromManager(bool keepSelection)
{
    if (!m_sessionList || !m_conversationManager) {
        return;
    }

    // 这里只消费 ConversationManager 当前已经准备好的快照，
    // 自己不额外触发 HTTP 拉取，保持 UI 层只做投影。
    const QString previousConversationId =
        keepSelection ? m_currentConversationId : QString();
    auto *conversationModel = m_conversationManager->conversationListModel();
    if (!conversationModel || conversationModel->rowCount() <= 0) {
        showSessionPlaceholder(QStringLiteral("当前还没有会话"));
        return;
    }

    // 目前中间栏仍然使用 QListWidget，因此这里会把 ConversationListModel
    // 的会话摘要快照重新投影成列表项 widget。
    m_sessionList->clear();

    const auto applyConversationAvatar = [](QWidget *itemWidget,
                                            const QPixmap &avatarPixmap) {
        if (!itemWidget || avatarPixmap.isNull()) {
            return;
        }

        auto *avatarLabel = itemWidget->findChild<QLabel *>(
            QStringLiteral("conversationAvatarLabel"));
        if (!avatarLabel) {
            return;
        }

        avatarLabel->setPixmap(avatarPixmap);
    };

    int targetRow = -1;
    for (int row = 0; row < conversationModel->rowCount(); ++row)
    {
        // 先从 ConversationListModel 读取标准摘要字段，再把 UI 当前直接会用到的关键字段
        // 缓存在 QListWidgetItem 的 role 上，后续切换会话时就不需要反查 model。
        const QModelIndex index = conversationModel->index(row, 0);
        const QString conversationId =
            index.data(
                     chatclient::model::ConversationListModel::ConversationIdRole)
                .toString();
        const QString title =
            index.data(chatclient::model::ConversationListModel::TitleRole)
                .toString();
        const QString peerUserId =
            index.data(
                     chatclient::model::ConversationListModel::PeerUserIdRole)
                .toString();
        const QString avatarStorageKey =
            index.data(
                     chatclient::model::ConversationListModel::AvatarUrlRole)
                .toString();
        const QString preview =
            index.data(
                     chatclient::model::ConversationListModel::LastMessagePreviewRole)
                .toString();
        const qint64 unreadCount =
            index.data(
                     chatclient::model::ConversationListModel::UnreadCountRole)
                .toLongLong();

        auto *item = createConversationListItem(title,
                                                preview,
                                                unreadCount,
                                                m_sessionList);
        item->setData(kConversationIdRole, conversationId);
        item->setData(kConversationTitleRole, title);
        item->setData(kConversationPeerUserIdRole, peerUserId);
        item->setData(kConversationAvatarStorageKeyRole, avatarStorageKey);
        item->setData(
            kConversationMetaRole,
            unreadCount > 0
                ? QStringLiteral("%1 条未读").arg(unreadCount)
                : QStringLiteral("历史消息已同步"));

        if (const QPixmap cachedAvatar =
                m_conversationAvatarCache.value(peerUserId);
            !cachedAvatar.isNull())
        {
            applyConversationAvatar(m_sessionList->itemWidget(item),
                                    cachedAvatar);
        }

        if (m_userApiClient && !peerUserId.isEmpty() &&
            !avatarStorageKey.trimmed().isEmpty() &&
            !m_conversationAvatarCache.contains(peerUserId))
        {
            // 会话列表头像采用简单的内存缓存，以 peer_user_id 为 key，
            // 避免每次列表重绘都重复发起头像下载。
            m_userApiClient->downloadUserAvatar(
                peerUserId,
                [this, peerUserId, applyConversationAvatar](const QByteArray &data) {
                    if (!m_sessionList) {
                        return;
                    }

                    QImage image;
                    if (!image.loadFromData(data))
                    {
                        CHATCLIENT_LOG_WARN("chat.window")
                            << "解析会话头像图片失败，peer_user_id="
                            << peerUserId;
                        return;
                    }

                    const QPixmap avatarPixmap =
                        createRoundedAvatarPixmap(image, QSize(40, 40));
                    if (avatarPixmap.isNull()) {
                        return;
                    }

                    m_conversationAvatarCache.insert(peerUserId, avatarPixmap);

                    for (int itemIndex = 0;
                         itemIndex < m_sessionList->count();
                         ++itemIndex)
                    {
                        QListWidgetItem *currentItem =
                            m_sessionList->item(itemIndex);
                        if (!currentItem) {
                            continue;
                        }

                        if (currentItem->data(kConversationPeerUserIdRole)
                                .toString() != peerUserId)
                        {
                            continue;
                        }

                        applyConversationAvatar(
                            m_sessionList->itemWidget(currentItem),
                            avatarPixmap);
                    }
                },
                [conversationId](const chatclient::dto::user::ApiErrorDto &error) {
                    CHATCLIENT_LOG_WARN("chat.window")
                        << "下载会话头像失败，conversation_id="
                        << conversationId
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

        if (!previousConversationId.isEmpty() &&
            previousConversationId == conversationId)
        {
            targetRow = row;
        }
    }

    if (targetRow < 0) {
        targetRow = 0;
    }

    m_sessionList->setCurrentRow(targetRow);
}

void ChatWindow::handleSessionSelectionChanged()
{
    if (!m_sessionList) {
        return;
    }

    auto *item = m_sessionList->currentItem();
    if (item == nullptr) {
        m_currentConversationId.clear();
        setConversationHeaderText(QStringLiteral("消息"),
                                  QStringLiteral("请选择一个会话"));
        if (m_messageListView && m_conversationManager) {
            m_messageListView->setMessageModel(
                m_conversationManager->ensureMessageModel(
                    QString::fromLatin1(kEmptyConversationId)));
        }
        return;
    }

    // 切换会话时当前不再重新发 HTTP：
    // 1. 记录当前 conversation_id
    // 2. 本地清掉该会话未读
    // 3. 切换右侧绑定的 MessageModel
    m_currentConversationId = item->data(kConversationIdRole).toString();
    if (m_currentConversationId.isEmpty()) {
        return;
    }

    if (m_conversationManager) {
        // 当前实现里的“已读”仍是客户端局部状态，只影响中间栏未读角标展示。
        // 是否同步服务端 last_read_seq，需要后续补正式读状态链路。
        m_conversationManager->markConversationReadLocally(
            m_currentConversationId);
    }

    setConversationHeaderText(item->data(kConversationTitleRole).toString(),
                              item->data(kConversationMetaRole).toString());

    if (m_conversationManager && m_messageListView) {
        m_messageListView->setMessageModel(
            m_conversationManager->ensureMessageModel(m_currentConversationId));
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

    // 好友详情区当前没有独立 model，直接读取 QListWidgetItem 上缓存的字段即可。
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
                    << "解析好友头像图片失败，user_id="
                    << expectedUserId;
                return;
            }

            updateFriendDetailAvatarImage(image);
        },
        [expectedUserId](const chatclient::dto::user::ApiErrorDto &error) {
            CHATCLIENT_LOG_WARN("chat.window")
                << "下载好友头像失败，user_id="
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
    // 添加好友弹窗关闭后，统一刷新一次好友列表，
    // 这样无论弹窗里发生了搜索、发送申请还是审批，都能把中间栏拉回最新快照。
    AddFriendDialog dialog(m_authService, m_conversationManager, this);
    dialog.exec();
    refreshFriendList(true);
}

void ChatWindow::refreshFriendList(const bool keepSelection)
{
    if (!m_friendService) {
        return;
    }

    // 好友页目前走“整表刷新”思路，而不是局部增量 patch。
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
        // 把右侧详情页会直接用到的字段提前缓存进 item role，
        // 这样切换选中项时不需要再向 service 反查一次 DTO。
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
            QStringLiteral(""));

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

void ChatWindow::setConversationHeaderText(const QString &title,
                                           const QString &meta)
{
    if (m_conversationTitleLabel) {
        m_conversationTitleLabel->setText(title);
    }
    if (m_conversationMetaLabel) {
        m_conversationMetaLabel->setText(meta);
    }
}

void ChatWindow::setConversationComposerHintText(const QString &text)
{
    if (m_messageComposerHintLabel) {
        m_messageComposerHintLabel->setText(text);
    }
}

void ChatWindow::handleSendMessage()
{
    if (!m_messageEditor || !m_conversationManager ||
        m_currentSection != SidebarSection::kMessages) {
        return;
    }

    const QString text = m_messageEditor->toPlainText().trimmed();
    if (text.isEmpty() || m_currentConversationId.trimmed().isEmpty()) {
        return;
    }

    // 当前发送按钮直接走实时通道。
    // 若 WS 当前不可用，这里不会自动退回到 HTTP send-text 接口。
    if (!m_conversationManager->sendTextMessage(m_currentConversationId, text)) {
        CHATCLIENT_LOG_WARN("chat.window")
            << "通过实时通道发送文本消息失败，conversation_id="
            << m_currentConversationId;
        return;
    }

    m_messageEditor->clear();
}

void ChatWindow::handleSendAttachment()
{
    if (!m_conversationManager)
    {
        setConversationComposerHintText(
            QStringLiteral("附件发送入口尚未初始化。"));
        return;
    }

    if (m_currentConversationId.trimmed().isEmpty())
    {
        setConversationComposerHintText(
            QStringLiteral("请先在左侧选择一个会话，再发送附件。"));
        return;
    }

    // 附件入口默认先给“所有文件”，避免界面层面的筛选器让用户误以为
    // 只能发送少数几种扩展名。图片筛选和常见文档筛选仍然保留，方便手动切换。
    //
    // 真正决定走图片消息还是文件消息的，不是这里的 filter，而是下面基于
    // QImageReader 的内容识别结果：
    // - 能被识别为图片：走 message.send_image
    // - 其它任意本地文件：走 message.send_file
    const QStringList localPaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择附件"),
        QString(),
        QStringLiteral(
            "所有文件 (*);;图片文件 (*.png *.jpg *.jpeg *.webp *.bmp *.gif);;常见文档与压缩文件 (*.pdf *.zip *.rar *.7z *.txt *.md *.doc *.docx *.xls *.xlsx *.ppt *.pptx)"));
    if (localPaths.isEmpty())
    {
        return;
    }

    int startedCount = 0;
    int imageCount = 0;
    int fileCount = 0;
    for (const QString &localPath : localPaths)
    {
        // 这里用“能否被图片解码器读取”来判断是否发送成图片消息。
        // 这样即使用户是从“所有文件”筛选里选出来的真实图片，也仍然会被
        // 自动分流到图片消息链路；反过来，所有无法识别为图片的文件都会统一
        // 走普通文件消息链路，不依赖扩展名白名单。
        QImageReader imageReader(localPath);
        imageReader.setAutoTransform(true);
        const bool isImageAttachment = imageReader.canRead();

        const bool started = isImageAttachment
                                 ? m_conversationManager->sendImageMessage(
                                       m_currentConversationId,
                                       localPath)
                                 : m_conversationManager->sendFileMessage(
                                       m_currentConversationId,
                                       localPath);
        if (started)
        {
            ++startedCount;
            if (isImageAttachment)
            {
                ++imageCount;
            }
            else
            {
                ++fileCount;
            }
        }
    }

    if (startedCount <= 0)
    {
        setConversationComposerHintText(
            QStringLiteral("附件发送启动失败，请确认文件可读且实时通道可用。"));
        return;
    }

    if (startedCount == 1)
    {
        setConversationComposerHintText(
            imageCount == 1
                ? QStringLiteral("图片已加入发送队列，正在上传。")
                : QStringLiteral("文件已加入发送队列，正在上传。"));
    }
    else
    {
        QString hintText =
            QStringLiteral("已将 %1 个附件加入发送队列").arg(startedCount);
        if (imageCount > 0 && fileCount > 0)
        {
            hintText += QStringLiteral("（其中 %1 张图片、%2 个文件）")
                            .arg(imageCount)
                            .arg(fileCount);
        }
        else if (imageCount > 0)
        {
            hintText += QStringLiteral("（%1 张图片）").arg(imageCount);
        }
        else if (fileCount > 0)
        {
            hintText += QStringLiteral("（%1 个文件）").arg(fileCount);
        }
        hintText += QStringLiteral("，正在并发上传。");
        setConversationComposerHintText(hintText);
    }

    if (m_messageListView)
    {
        m_messageListView->scrollToBottom();
    }
}

bool ChatWindow::requestFileMessageDownload(const QModelIndex &index,
                                            const QString &targetPath)
{
    if (!m_conversationManager)
    {
        setConversationComposerHintText(QStringLiteral("文件下载入口尚未初始化。"));
        return false;
    }

    if (m_currentConversationId.trimmed().isEmpty())
    {
        setConversationComposerHintText(
            QStringLiteral("请先选择一个会话，再操作文件消息。"));
        return false;
    }

    const auto context = fileMessageUiContextFromIndex(index);
    if (!context.has_value())
    {
        return false;
    }

    chatclient::service::ConversationManager::FileDownloadRequest request;
    request.conversationId = m_currentConversationId.trimmed();
    request.identity = context->identity;
    request.fileName = context->fileName;
    request.remoteUrl = context->remoteUrl;
    request.currentLocalPath = context->localPath;
    request.mimeType = context->mimeType;
    request.sizeBytes = context->sizeBytes;
    request.savePath = targetPath.trimmed();

    if (!m_conversationManager->downloadFileMessage(request))
    {
        setConversationComposerHintText(
            QStringLiteral("文件下载启动失败，请稍后重试。"));
        return false;
    }

    const QString displayFileName = context->fileName.trimmed().isEmpty()
                                        ? QStringLiteral("附件")
                                        : context->fileName.trimmed();
    setConversationComposerHintText(
        targetPath.trimmed().isEmpty()
            ? QStringLiteral("已开始下载文件：%1").arg(displayFileName)
            : QStringLiteral("已开始下载到指定位置：%1").arg(displayFileName));
    return true;
}

void ChatWindow::handleFileMessageActivated(const QModelIndex &index)
{
    const auto context = fileMessageUiContextFromIndex(index);
    if (!context.has_value())
    {
        return;
    }

    if (context->transferState == MessageTransferState::Uploading ||
        context->transferState == MessageTransferState::Sending)
    {
        setConversationComposerHintText(
            QStringLiteral("该文件消息仍在发送中，请稍后再试。"));
        return;
    }

    if (context->transferState == MessageTransferState::Downloading)
    {
        setConversationComposerHintText(
            QStringLiteral("文件正在下载中，请稍候。"));
        return;
    }

    if (!context->localPath.trimmed().isEmpty() &&
        QFileInfo::exists(context->localPath))
    {
        handleFileMessageOpenRequested(index);
        return;
    }

    requestFileMessageDownload(index);
}

void ChatWindow::handleFileMessageDownloadRequested(const QModelIndex &index)
{
    requestFileMessageDownload(index);
}

void ChatWindow::handleFileMessageDownloadToRequested(
    const QModelIndex &index,
    const QString &targetPath)
{
    if (targetPath.trimmed().isEmpty())
    {
        return;
    }

    requestFileMessageDownload(index, targetPath);
}

void ChatWindow::handleFileMessageOpenRequested(const QModelIndex &index)
{
    const auto context = fileMessageUiContextFromIndex(index);
    if (!context.has_value())
    {
        return;
    }

    if (context->localPath.trimmed().isEmpty() ||
        !QFileInfo::exists(context->localPath))
    {
        requestFileMessageDownload(index);
        return;
    }

    if (!QDesktopServices::openUrl(
            QUrl::fromLocalFile(QFileInfo(context->localPath).absoluteFilePath())))
    {
        setConversationComposerHintText(
            QStringLiteral("打开文件失败，请确认系统已配置默认打开方式。"));
        return;
    }

    setConversationComposerHintText(
        QStringLiteral("正在打开文件：%1")
            .arg(context->fileName.trimmed().isEmpty()
                     ? QStringLiteral("附件")
                     : context->fileName.trimmed()));
}

void ChatWindow::handleFileMessageOpenFolderRequested(
    const QModelIndex &index)
{
    const auto context = fileMessageUiContextFromIndex(index);
    if (!context.has_value())
    {
        return;
    }

    if (context->localPath.trimmed().isEmpty() ||
        !QFileInfo::exists(context->localPath))
    {
        setConversationComposerHintText(
            QStringLiteral("当前文件还没有下载到本地。"));
        return;
    }

    const QString folderPath = QFileInfo(context->localPath).absolutePath();
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(folderPath)))
    {
        setConversationComposerHintText(
            QStringLiteral("打开文件夹失败，请检查系统文件管理器配置。"));
        return;
    }

    setConversationComposerHintText(
        QStringLiteral("已打开文件所在目录。"));
}

void ChatWindow::toggleEmojiPicker()
{
    if (!m_messageEmojiButton || !m_messageEditor)
    {
        return;
    }

    ensureEmojiPickerCreated();
    if (!m_messageEmojiMenu)
    {
        return;
    }

    // 再次点击“表情”按钮时直接关闭当前面板，行为和常见聊天工具保持一致。
    if (m_messageEmojiMenu->isVisible())
    {
        m_messageEmojiMenu->hide();
        return;
    }

    const QPoint popupPosition = m_messageEmojiButton->mapToGlobal(
        QPoint(0, m_messageEmojiButton->height() + 6));
    m_messageEmojiMenu->popup(popupPosition);
}

void ChatWindow::ensureEmojiPickerCreated()
{
    if (m_messageEmojiMenu)
    {
        return;
    }

    // 表情面板当前刻意做成轻量弹层，而不是新开一个独立窗口：
    // 1. 只服务于当前聊天输入区；
    // 2. 点击外部区域会自动关闭；
    // 3. 后续如果要扩成分页、搜索或最近使用，也仍然可以在这块容器里继续演进。
    m_messageEmojiMenu = new QMenu(this);
    m_messageEmojiMenu->setObjectName(QStringLiteral("messageEmojiMenu"));
    m_messageEmojiMenu->setToolTipsVisible(true);

    auto *container = new QWidget(m_messageEmojiMenu);
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(12, 12, 12, 12);
    containerLayout->setSpacing(10);

    auto *titleLabel = new QLabel(QStringLiteral("常用表情"), container);
    titleLabel->setObjectName(QStringLiteral("panelHeaderSubtitle"));
    containerLayout->addWidget(titleLabel);

    auto *emojiGrid = new QGridLayout();
    emojiGrid->setContentsMargins(0, 0, 0, 0);
    emojiGrid->setHorizontalSpacing(6);
    emojiGrid->setVerticalSpacing(6);

    const QStringList emojis = defaultEmojiList();
    constexpr int kEmojiColumnCount = 6;
    for (int index = 0; index < emojis.size(); ++index)
    {
        const QString emoji = emojis.at(index);
        auto *emojiButton = new QPushButton(emoji, container);
        emojiButton->setObjectName(QStringLiteral("emojiPickerButton"));
        emojiButton->setToolTip(QStringLiteral("插入 %1").arg(emoji));
        emojiButton->setFixedSize(40, 40);

        // 每个按钮只负责一件事：把自己代表的 emoji 插入输入框。
        // 输入框当前光标位置、焦点恢复和弹层关闭，都统一收口到 ChatWindow 内部。
        connect(emojiButton,
                &QPushButton::clicked,
                this,
                [this, emoji]() {
                    insertEmojiIntoEditor(emoji);
                    if (m_messageEmojiMenu)
                    {
                        m_messageEmojiMenu->hide();
                    }
                });

        emojiGrid->addWidget(emojiButton,
                             index / kEmojiColumnCount,
                             index % kEmojiColumnCount);
    }

    containerLayout->addLayout(emojiGrid);

    auto *contentAction = new QWidgetAction(m_messageEmojiMenu);
    contentAction->setDefaultWidget(container);
    m_messageEmojiMenu->addAction(contentAction);
}

void ChatWindow::insertEmojiIntoEditor(const QString &emoji)
{
    if (!m_messageEditor || emoji.isEmpty())
    {
        return;
    }

    // 直接操作 QTextCursor，可以精确保持“插入到当前光标位置”的行为，
    // 而不是粗暴地把 emoji 追加到文本末尾。
    QTextCursor cursor = m_messageEditor->textCursor();
    cursor.insertText(emoji);
    m_messageEditor->setTextCursor(cursor);
    m_messageEditor->setFocus();
}
