#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QListView;
class QListWidget;
class QModelIndex;
class QPoint;
class QTextEdit;
class MessageModel;

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
    // 构造函数：
    // 初始化窗口尺寸与标题，创建左右两栏布局，并加载聊天窗口专用 QSS。
    explicit ChatWindow(QWidget *parent = nullptr);

private:
    // 创建左侧边栏：
    // 包含品牌标题、会话搜索框、会话列表和底部用户信息卡片。
    QWidget *createSidebar();

    // 创建右侧聊天主面板：
    // 包含顶部会话信息栏、消息滚动区域、底部消息输入区与操作按钮。
    QWidget *createChatPanel();

    // 追加消息到模型，并滚动到底部。
    void appendMessage(const QString &author, const QString &text, bool fromSelf);
    // 处理发送动作：读取输入框文本并写入消息模型。
    void handleSendMessage();
    // 复制指定消息行的正文到剪贴板。
    void copyMessageText(const QModelIndex &index);
    // 复制当前选中消息正文。
    void copyCurrentMessageText();
    // 显示消息列表右键菜单（包含复制操作）。
    void showMessageContextMenu(const QPoint &pos);

    // 左侧：会话搜索输入框。
    QLineEdit *m_searchEdit = nullptr;
    // 左侧：会话列表（当前为演示数据，后续可替换为模型/真实数据源）。
    QListWidget *m_sessionList = nullptr;

    // 右侧：消息列表视图（QListView + model + delegate）。
    QListView *m_messageListView = nullptr;
    // 右侧：消息数据模型。
    MessageModel *m_messageModel = nullptr;
    // 右侧：底部文本输入框。
    QTextEdit *m_messageEditor = nullptr;
};
