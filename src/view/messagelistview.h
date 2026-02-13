#pragma once

#include <QListView>
#include <QPersistentModelIndex>

class MessageDelegate;
class QAbstractItemModel;
class QMouseEvent;
class QEvent;
class QPoint;
class QModelIndex;
class QString;

// 消息列表视图：
// - 负责将鼠标拖动转换为“正文字符选区”；
// - 复用 MessageDelegate 的命中测试与高亮绘制能力；
// - 对外提供选区复制接口，供 Ctrl+C 与右键菜单复用。
class MessageListView : public QListView
{
    Q_OBJECT

public:
    /**
     * @brief 构造消息列表视图并初始化默认交互。
     * @param parent 父级 QWidget，可为空。
     */
    explicit MessageListView(QWidget *parent = nullptr);

    /**
     * @brief 绑定外部消息模型。
     * @param model 要显示的消息模型指针。
     */
    void setMessageModel(QAbstractItemModel *model);

    /**
     * @brief 判断当前是否存在有效正文选区。
     * @return true 表示存在可复制选区。
     */
    bool hasSelectedText() const;
    /**
     * @brief 判断指定消息行是否包含当前选区。
     * @param index 目标消息索引。
     * @return true 表示选区属于该消息行。
     */
    bool hasSelectedTextOnIndex(const QModelIndex &index) const;
    /**
     * @brief 获取当前选区文本。
     * @return 当前选中的文本内容；无选区时返回空字符串。
     */
    QString selectedText() const;
    /**
     * @brief 复制当前选区到系统剪贴板。
     * @return true 表示复制成功。
     */
    bool copySelectedTextToClipboard() const;
    /**
     * @brief 清空当前文本选区并触发重绘。
     */
    void clearSelectedText();

protected:
    /**
     * @brief 处理鼠标按下事件并决定是否开始拖拽选区。
     * @param event 鼠标事件对象。
     */
    void mousePressEvent(QMouseEvent *event) override;
    /**
     * @brief 处理鼠标双击事件，命中正文时选中整条正文文本。
     * @param event 鼠标事件对象。
     */
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    /**
     * @brief 处理鼠标移动事件（更新选区或悬停光标）。
     * @param event 鼠标事件对象。
     */
    void mouseMoveEvent(QMouseEvent *event) override;
    /**
     * @brief 处理鼠标释放事件并结束拖拽选区。
     * @param event 鼠标事件对象。
     */
    void mouseReleaseEvent(QMouseEvent *event) override;
    /**
     * @brief 处理鼠标离开事件并恢复默认光标。
     * @param event 通用事件对象。
     */
    void leaveEvent(QEvent *event) override;

private:
    /**
     * @brief 显示消息右键菜单并处理菜单动作。
     * @param pos 右键触发的视图坐标。
     */
    void showMessageContextMenu(const QPoint &pos);
    /**
     * @brief 复制当前消息文本（优先正文选区）。
     */
    void copyCurrentMessageText();
    /**
     * @brief 复制指定消息行文案。
     * @param index 目标消息索引。
     */
    void copyMessageText(const QModelIndex &index) const;
    /**
     * @brief 获取当前消息委托并校验类型。
     * @return MessageDelegate 指针；类型不匹配时返回 nullptr。
     */
    MessageDelegate *messageDelegate() const;
    /**
     * @brief 根据悬停位置切换文本光标或默认光标。
     * @param viewPos 当前鼠标视图坐标。
     */
    void updateHoverCursor(const QPoint &viewPos);
    /**
     * @brief 清除手动光标设置并恢复系统默认光标。
     */
    void resetHoverCursor();

    // true 表示处于“左键拖拽文本选区”过程。
    bool m_dragSelecting = false;
    // 记录拖拽起始所在行；拖拽期间固定在同一行内选区。
    QPersistentModelIndex m_dragIndex;
    // 记录拖拽锚点字符位置（对应 selection anchor）。
    int m_dragAnchor = -1;

    // 禁止外部直接调用基类 setModel，统一走 setMessageModel。
    using QListView::setModel;
};
