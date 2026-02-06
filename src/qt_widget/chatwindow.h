#pragma once

#include <QString>
#include <QWidget>

class QLineEdit;
class QListWidget;
class QTextEdit;
class QVBoxLayout;

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

    // 创建单条消息行：
    // author: 消息作者
    // text: 消息正文
    // timeText: 时间文本（示例中直接传入格式化字符串）
    // fromSelf: 是否为当前用户发送，决定左右对齐与气泡样式
    QWidget *createMessageRow(const QString &author, const QString &text, const QString &timeText, bool fromSelf);

    // 左侧：会话搜索输入框。
    QLineEdit *m_searchEdit = nullptr;
    // 左侧：会话列表（当前为演示数据，后续可替换为模型/真实数据源）。
    QListWidget *m_sessionList = nullptr;

    // 右侧：消息区域容器（放在 QScrollArea 内部）。
    QWidget *m_messageContainer = nullptr;
    // 右侧：消息垂直布局，用于按时间顺序堆叠消息行。
    QVBoxLayout *m_messageLayout = nullptr;
    // 右侧：底部文本输入框。
    QTextEdit *m_messageEditor = nullptr;
};
