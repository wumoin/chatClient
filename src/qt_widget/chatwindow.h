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

// ChatWindow 当前承担“聊天主界面编排层”职责：
// 1) 构建左侧导航栏、中间列表栏、右侧详情区三段式布局；
// 2) 把 ConversationManager / FriendService / AuthService 暴露出来的状态投影成 QWidget；
// 3) 处理用户交互，并把意图回传给 service 层。
//
// 它不是消息、会话、好友状态的最终数据源。真正的业务状态分别来自：
// - AuthService：当前登录态；
// - ConversationManager：会话列表、消息模型、实时通道；
// - FriendService：好友列表快照。
//
// 当前仍保留一个过渡实现：中间栏的会话列表 / 好友列表继续使用 QListWidget，
// 因此需要把 model / DTO 快照再投影成 item widget。
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
     * @brief 处理发送动作并将输入框内容写入当前会话。
     */
    void handleSendMessage();

    /**
     * @brief 选择本地图片并以仅本地展示的形式追加到当前会话。
     */
    void handleSendLocalImage();

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

    // 当前左侧导航选中的主模式，决定中间栏和右侧内容区切到哪一页。
    SidebarSection m_currentSection{SidebarSection::kMessages};

    // 左上角导航头像徽标：显示当前登录用户头像，拿不到图片时退回文字头像。
    QLabel *m_navAvatarLabel = nullptr;
    // 左侧“消息”主入口按钮，切到会话列表 + 聊天内容页。
    QPushButton *m_messagesNavButton = nullptr;
    // 左侧“好友”主入口按钮，切到好友列表 + 好友详情页。
    QPushButton *m_friendsNavButton = nullptr;
    // 左侧“切换账号”按钮，点击后向外发出 switchAccountRequested。
    QPushButton *m_switchAccountNavButton = nullptr;
    // 左侧“登出”按钮，点击后向外发出 signOutRequested。
    QPushButton *m_signOutNavButton = nullptr;

    // 中间栏内容栈：在“消息页列表”和“好友页列表”之间切换。
    QStackedWidget *m_middleStack = nullptr;
    // 消息页顶部搜索框；当前主要是 UI 占位，尚未接真实会话过滤逻辑。
    QLineEdit *m_messageSearchEdit = nullptr;
    // 好友页顶部搜索框；当前主要是 UI 占位，尚未接真实好友过滤逻辑。
    QLineEdit *m_friendSearchEdit = nullptr;
    // 中间栏会话列表控件：把 ConversationManager 的会话快照投影成 QListWidget。
    QListWidget *m_sessionList = nullptr;
    // 中间栏好友列表控件：展示 FriendService 最近一次拉回来的正式好友快照。
    QListWidget *m_friendList = nullptr;
    // 好友页“添加好友”按钮，打开 AddFriendDialog。
    QPushButton *m_addFriendButton = nullptr;

    // 右侧内容区栈：在聊天详情页和好友详情页之间切换。
    QStackedWidget *m_contentStack = nullptr;
    // 聊天页顶部标题，显示当前选中会话名称。
    QLabel *m_conversationTitleLabel = nullptr;
    // 聊天页顶部副标题，显示未读/同步状态等会话补充信息。
    QLabel *m_conversationMetaLabel = nullptr;
    // 聊天页顶部“语音”按钮；当前属于占位入口，尚未接真实能力。
    QPushButton *m_conversationVoiceButton = nullptr;
    // 聊天页顶部“视频”按钮；当前属于占位入口，尚未接真实能力。
    QPushButton *m_conversationVideoButton = nullptr;
    // 输入框上方提示文案，会显示服务地址、图片发送提示、错误提示等。
    QLabel *m_messageComposerHintLabel = nullptr;
    // 好友详情页头像区域，优先显示真实头像，退回时显示文字头像。
    QLabel *m_friendDetailAvatarLabel = nullptr;
    // 好友详情页主标题，显示当前选中好友名称。
    QLabel *m_friendDetailTitleLabel = nullptr;
    // 好友详情页副标题，显示账号、用户 ID 等摘要信息。
    QLabel *m_friendDetailMetaLabel = nullptr;
    // 好友详情页说明区，显示提示语、错误信息或发起会话结果。
    QLabel *m_friendDetailHintLabel = nullptr;
    // 好友详情页“发起会话”按钮，用来创建或复用一对一私聊。
    QPushButton *m_startChatButton = nullptr;

    // 右侧消息列表视图，内部绑定某个 conversation_id 对应的 MessageModel。
    MessageListView *m_messageListView = nullptr;
    // 当前仅记录“UI 当前选中的是哪个会话”，不代表服务端 read state。
    QString m_currentConversationId;
    // 会话列表头像的简单内存缓存，以 peer_user_id 为 key，避免重复下载头像。
    QHash<QString, QPixmap> m_conversationAvatarCache;
    // 聊天输入框，负责承载当前待发送的文本内容。
    QTextEdit *m_messageEditor = nullptr;
    // 输入区“表情”按钮；当前属于占位入口，尚未接真实表情面板。
    QPushButton *m_messageEmojiButton = nullptr;
    // 输入区“图片”按钮，用来选择本地图片并追加到当前消息模型。
    QPushButton *m_messageFileButton = nullptr;
    // 输入区“发送”按钮，把当前输入框文本发到当前会话。
    QPushButton *m_messageSendButton = nullptr;
    // 用户资料 HTTP 客户端，当前主要用于下载当前用户 / 好友 / 会话头像。
    chatclient::api::UserApiClient *m_userApiClient = nullptr;
    // 外部注入的认证服务，用来判断是否有有效登录态；ChatWindow 不拥有它的生命周期。
    chatclient::service::AuthService *m_authService = nullptr;
    // 会话编排服务，由 ChatWindow 创建并持有，负责会话列表、消息模型和实时通道。
    chatclient::service::ConversationManager *m_conversationManager = nullptr;
    // 好友服务，依赖当前 AuthService 创建，负责好友列表刷新和好友域操作。
    chatclient::service::FriendService *m_friendService = nullptr;

    // 左下角资料卡里的用户名标签。
    QLabel *m_profileNameLabel = nullptr;
    // 左下角资料卡里的状态标签，显示实时连接状态或账号动作状态。
    QLabel *m_profileStatusLabel = nullptr;
    // 当前登录用户 ID，用于异步下载头像时确认返回结果是否仍属于当前资料卡。
    QString m_currentProfileUserId;
    // 当前登录用户头像 storage key，用来判断是否需要发起头像下载。
    QString m_currentProfileAvatarStorageKey;
    // 好友页当前选中的好友 user_id，用于详情区刷新和发起私聊。
    QString m_currentSelectedFriendUserId;
    // 应用退出路径中的关闭放行开关；为 false 时直接关闭窗口会先转成登出请求。
    bool m_allowClose = false;
};
