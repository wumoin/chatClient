#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QLabel;
class QListWidget;
class QTextEdit;
class MessageListView;
class MessageModelRegistry;

// ChatWindow 仅承担“界面展示”职责：
// 1) 构建聊天页面布局（会话列表、消息区、输入区）。
// 2) 为关键控件设置 objectName，交给 chatwindow.qss 统一控制样式。
// 3) 提供静态示例数据，便于前期联调视觉与交互。
//
// 注意：这里不处理网络通信、消息收发、数据持久化等业务逻辑。
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
     */
    void setCurrentUserProfile(const QString &displayName,
                               const QString &statusText);

private:
    /**
     * @brief 创建左侧会话导航栏。
     * @return 左侧导航栏容器指针。
     */
    QWidget *createSidebar();

    /**
     * @brief 创建右侧聊天主面板。
     * @return 右侧主面板容器指针。
     */
    QWidget *createChatPanel();

    /**
     * @brief 向当前会话追加一条文本消息并滚动到底部。
     * @param author 发送者名称。
     * @param text 消息正文。
     * @param fromSelf 是否为当前用户发送。
     */
    void appendMessage(const QString &author, const QString &text, bool fromSelf);
    /**
     * @brief 处理发送动作并将输入框内容写入当前会话。
     */
    void handleSendMessage();

    // 左侧：会话搜索输入框。
    QLineEdit *m_searchEdit = nullptr;
    // 左侧：会话列表（当前为演示数据，后续可替换为模型/真实数据源）。
    QListWidget *m_sessionList = nullptr;

    // 右侧：消息列表视图（QListView + model + delegate）。
    MessageListView *m_messageListView = nullptr;
    // 消息模型注册表：管理多会话 MessageModel。
    MessageModelRegistry *m_messageModelRegistry = nullptr;
    // 当前聊天会话 id（用于选择注册表中的目标模型）。
    QString m_currentConversationId;
    // 右侧：底部文本输入框。
    QTextEdit *m_messageEditor = nullptr;
    // 左下角：当前登录用户展示名。
    QLabel *m_profileNameLabel = nullptr;
    // 左下角：当前登录用户状态文本。
    QLabel *m_profileStatusLabel = nullptr;
};
