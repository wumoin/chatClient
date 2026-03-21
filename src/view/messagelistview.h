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

signals:
    /**
     * @brief 用户点击了文件消息卡片主体。
     * @param index 当前命中的文件消息索引。
     */
    void fileMessageActivated(const QModelIndex &index);
    /**
     * @brief 用户请求把文件消息下载到默认目录。
     * @param index 当前命中的文件消息索引。
     */
    void fileMessageDownloadRequested(const QModelIndex &index);
    /**
     * @brief 用户请求把文件消息下载到指定路径。
     * @param index 当前命中的文件消息索引。
     * @param targetPath 用户在文件对话框中选择的目标绝对路径。
     */
    void fileMessageDownloadToRequested(const QModelIndex &index,
                                        const QString &targetPath);
    /**
     * @brief 用户请求直接打开已经下载好的本地文件。
     * @param index 当前命中的文件消息索引。
     */
    void fileMessageOpenRequested(const QModelIndex &index);
    /**
     * @brief 用户请求打开已经下载文件所在目录。
     * @param index 当前命中的文件消息索引。
     */
    void fileMessageOpenFolderRequested(const QModelIndex &index);

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
    /**
     * @brief 判断当前索引是否为文件消息。
     * @param index 目标消息索引。
     * @return true 表示该行是文件消息。
     */
    bool isFileMessageIndex(const QModelIndex &index) const;
    /**
     * @brief 判断当前文件消息是否已有可访问的本地文件。
     * @param index 目标消息索引。
     * @return true 表示 FileLocalPathRole 指向的文件当前存在。
     */
    bool hasDownloadedLocalFile(const QModelIndex &index) const;
    /**
     * @brief 生成“下载到...”对话框的默认建议路径。
     * @param index 目标文件消息索引。
     * @return 建议的绝对路径；若信息不足则返回空字符串。
     */
    QString suggestedDownloadPath(const QModelIndex &index) const;

    // true 表示处于“左键拖拽文本选区”过程。
    bool m_dragSelecting = false;
    // 记录拖拽起始所在行；拖拽期间固定在同一行内选区。
    QPersistentModelIndex m_dragIndex;
    // 记录拖拽锚点字符位置（对应 selection anchor）。
    int m_dragAnchor = -1;
    // 记录当前是否按下了文件消息卡片；松开且仍命中时触发“打开/下载”动作。
    QPersistentModelIndex m_pressedFileCardIndex;

    // 禁止外部直接调用基类 setModel，统一走 setMessageModel。
    using QListView::setModel;
};
