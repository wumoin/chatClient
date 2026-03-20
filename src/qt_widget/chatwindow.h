#pragma once

#include "dto/conversation_dto.h"
#include "dto/friend_dto.h"

#include <QHash>
#include <QPixmap>
#include <QString>
#include <QWidget>

class QCloseEvent;
class QImage;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class MessageListView;

namespace chatclient::api {
class UserApiClient;
}

namespace chatclient::service {
class AuthService;
class ConversationManager;
class FriendService;
}

// ChatWindow 当前承担“聊天主界面骨架”职责：
// 1) 构建左侧导航栏、中间列表栏、右侧详情区三段式布局；
// 2) 提供“消息 / 好友”两种主模式切换；
// 3) 保留现有消息演示链路，并为后续好友能力预留独立入口。
//
// 注意：
// - 消息页当前仍然使用演示数据；
// - 好友页当前已接入真实好友列表 HTTP 数据源；
// - 会话列表当前继续使用 QListWidget。
class ChatWindow : public QWidget
{
    Q_OBJECT

  public:
    /**
     * @brief 构造聊天窗口并完成主界面初始化。
     * @param parent 父级 QWidget，可为空。
     */
    explicit ChatWindow(QWidget *parent = nullptr);

    /**
     * @brief 设置当前登录用户的展示信息。
     * @param displayName 界面展示名称。
     * @param statusText 当前状态文本。
     * @param userId 当前登录用户 ID，可为空。
     * @param avatarStorageKey 当前用户头像 storage key，可为空。
     */
    void setCurrentUserProfile(const QString &displayName,
                               const QString &statusText,
                               const QString &userId = QString(),
                               const QString &avatarStorageKey = QString());

    /**
     * @brief 切换左侧账号动作按钮的忙碌状态。
     * @param submitting true 表示账号动作提交中；false 表示恢复可点击。
     * @param quitting true 表示当前执行“登出并退出程序”；false 表示执行“切换账号”。
     */
    void setSessionActionSubmitting(bool submitting, bool quitting);

    /**
     * @brief 为聊天窗口注入当前认证服务。
     * @param authService 当前登录态服务，可为空。
     */
    void setAuthService(chatclient::service::AuthService *authService);

    /**
     * @brief 允许窗口在应用退出路径中直接关闭。
     */
    void allowWindowClose();

  signals:
    /**
     * @brief 用户点击左侧导航栏中的“切换账号”按钮。
     */
    void switchAccountRequested();

    /**
     * @brief 用户发起“登出并退出程序”动作。
     */
    void signOutRequested();

  private:
    /**
     * @brief 拦截窗口关闭动作，并转成“登出并退出程序”请求。
     * @param event 关闭事件对象。
     */
    void closeEvent(QCloseEvent *event) override;

    /**
     * @brief 统一拦截输入框中的回车事件，实现 Enter 发送、Shift+Enter 换行。
     * @param watched 当前被监听的对象。
     * @param event 当前事件。
     * @return true 表示事件已消费；false 表示继续交给默认处理。
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

    enum class SidebarSection
    {
        kMessages = 0,
        kFriends = 1,
    };

    /**
     * @brief 创建左侧主导航栏。
     * @return 左侧导航栏容器指针。
     */
    QWidget *createNavigationRail();

    /**
     * @brief 创建中间列表栏容器。
     * @return 中间列表栏容器指针。
     */
    QWidget *createMiddlePanel();

    /**
     * @brief 创建“消息”模式下的中间列表页。
     * @return 消息列表页容器指针。
     */
    QWidget *createMessagesPage();

    /**
     * @brief 创建“好友”模式下的中间列表页。
     * @return 好友列表页容器指针。
     */
    QWidget *createFriendsPage();

    /**
     * @brief 创建“消息”模式下的右侧聊天详情页。
     * @return 聊天详情页容器指针。
     */
    QWidget *createMessageContentPage();

    /**
     * @brief 创建“好友”模式下的右侧详情页。
     * @return 好友详情页容器指针。
     */
    QWidget *createFriendContentPage();

    /**
     * @brief 切换左侧导航对应的主模式。
     * @param section 目标模式。
     */
    void switchSection(SidebarSection section);

    /**
     * @brief 响应当前会话项切换，并更新右侧聊天详情。
     */
    void handleSessionSelectionChanged();

    /**
     * @brief 响应当前好友项切换，并更新右侧好友详情。
     */
    void handleFriendSelectionChanged();

    /**
     * @brief 打开“添加好友”独立弹窗。
     */
    void showAddFriendDialog();

    /**
     * @brief 拉取当前登录用户的正式好友列表。
     * @param keepSelection true 表示尽量保留当前选中项；false 表示按默认规则重新选中。
     */
    void refreshFriendList(bool keepSelection = true);

    /**
     * @brief 将 ConversationListModel 当前快照同步到中间栏会话列表控件。
     * @param keepSelection true 表示尽量保留当前选中会话；false 表示按默认规则重新选中。
     */
    void updateSessionListFromManager(bool keepSelection = true);

    /**
     * @brief 用服务端返回的好友列表刷新中间栏。
     * @param friends 当前正式好友集合。
     * @param keepSelection true 表示尽量保留当前选中项。
     */
    void updateFriendList(
        const chatclient::dto::friendship::FriendListItems &friends,
        bool keepSelection);

    /**
     * @brief 根据当前展示名称更新左上角默认头像文本。
     * @param displayName 当前登录用户展示名。
     */
    void updateProfileAvatar(const QString &displayName);

    /**
     * @brief 使用图片更新左上角真实头像。
     * @param image 服务端返回的头像图片。
     */
    void updateProfileAvatarImage(const QImage &image);

    /**
     * @brief 根据当前好友名称更新右侧详情区默认头像文本。
     * @param displayName 当前选中好友展示名。
     */
    void updateFriendDetailAvatar(const QString &displayName);

    /**
     * @brief 使用图片更新右侧详情区真实头像。
     * @param image 服务端返回的好友头像图片。
     */
    void updateFriendDetailAvatarImage(const QImage &image);

    /**
     * @brief 更新消息页头部标题和副标题。
     * @param title 当前会话标题。
     * @param meta 当前会话副标题。
     */
    void setConversationHeaderText(const QString &title,
                                   const QString &meta);

    /**
     * @brief 更新消息输入区上方提示文案。
     * @param text 当前提示文本。
     */
    void setConversationComposerHintText(const QString &text);

    /**
     * @brief 统一更新消息页顶部操作按钮的可用状态。
     * @param enabled true 表示可点击；false 表示禁用。
     */
    void setConversationHeaderActionsEnabled(bool enabled);

    /**
     * @brief 统一更新消息输入区相关控件的可用状态。
     * @param enabled true 表示可编辑 / 可点击；false 表示禁用。
     */
    void setMessageComposerActionsEnabled(bool enabled);

    /**
     * @brief 更新发送按钮文案。
     * @param text 需要展示的新文案。
     */
    void setMessageSendButtonText(const QString &text);

    /**
     * @brief 处理发送动作并将输入框内容写入当前会话。
     */
    void handleSendMessage();

    /**
     * @brief 从好友详情页发起或复用一对一私聊会话。
     */
    void handleStartPrivateConversation();

    /**
     * @brief 按当前登录态建立最小 WebSocket 实时通道。
     */
    void connectRealtimeChannel();

    /**
     * @brief 更新左下角资料卡中的实时通道状态文本。
     * @param statusText 适合直接展示到界面的状态文本。
     */
    void updateRealtimeStatus(const QString &statusText);

    /**
     * @brief 在中间栏展示“会话加载中 / 失败 / 为空”之类的占位文案。
     * @param message 需要展示的说明文本。
     */
    void showSessionPlaceholder(const QString &message);

    SidebarSection m_currentSection{SidebarSection::kMessages};

    // 左侧导航栏：消息 / 好友两种主入口。
    QLabel *m_navAvatarLabel = nullptr;
    QPushButton *m_messagesNavButton = nullptr;
    QPushButton *m_friendsNavButton = nullptr;
    QPushButton *m_switchAccountNavButton = nullptr;
    QPushButton *m_signOutNavButton = nullptr;

    // 中间栏：根据左侧导航切换不同列表页。
    QStackedWidget *m_middleStack = nullptr;
    QLineEdit *m_messageSearchEdit = nullptr;
    QLineEdit *m_friendSearchEdit = nullptr;
    QListWidget *m_sessionList = nullptr;
    QListWidget *m_friendList = nullptr;
    QPushButton *m_addFriendButton = nullptr;

    // 右侧内容区：消息详情页 / 好友详情页。
    QStackedWidget *m_contentStack = nullptr;
    QLabel *m_conversationTitleLabel = nullptr;
    QLabel *m_conversationMetaLabel = nullptr;
    QPushButton *m_conversationVoiceButton = nullptr;
    QPushButton *m_conversationVideoButton = nullptr;
    QLabel *m_messageComposerHintLabel = nullptr;
    QLabel *m_friendDetailAvatarLabel = nullptr;
    QLabel *m_friendDetailTitleLabel = nullptr;
    QLabel *m_friendDetailMetaLabel = nullptr;
    QLabel *m_friendDetailHintLabel = nullptr;
    QPushButton *m_startChatButton = nullptr;

    // 消息页：仍然沿用现有 QListView + model + delegate 展示链路。
    MessageListView *m_messageListView = nullptr;
    QString m_currentConversationId;
    QHash<QString, QPixmap> m_conversationAvatarCache;
    QTextEdit *m_messageEditor = nullptr;
    QPushButton *m_messageEmojiButton = nullptr;
    QPushButton *m_messageFileButton = nullptr;
    QPushButton *m_messageSendButton = nullptr;
    chatclient::api::UserApiClient *m_userApiClient = nullptr;
    chatclient::service::AuthService *m_authService = nullptr;
    chatclient::service::ConversationManager *m_conversationManager = nullptr;
    chatclient::service::FriendService *m_friendService = nullptr;

    // 左下角：当前登录用户信息。
    QLabel *m_profileNameLabel = nullptr;
    QLabel *m_profileStatusLabel = nullptr;
    QString m_currentProfileUserId;
    QString m_currentProfileAvatarStorageKey;
    QString m_currentSelectedFriendUserId;
    bool m_allowClose = false;
};
