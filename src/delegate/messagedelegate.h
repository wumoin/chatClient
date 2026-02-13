#pragma once

#include <QStyledItemDelegate>
#include <QPersistentModelIndex>

class QAbstractItemModel;
class QPoint;
class QStyleOptionViewItem;

// 聊天气泡绘制委托：
// - sizeHint(): 根据文本内容动态计算每行高度；
// - paint(): 直接在视口上绘制气泡、作者、正文和时间。
// 该类替代“每条消息一个 QWidget”的做法，提升大量消息时的渲染效率。
class MessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    /**
     * @brief 构造消息绘制委托。
     * @param parent 父级 QObject，可为空。
     */
    explicit MessageDelegate(QObject *parent = nullptr);

    /**
     * @brief 将视图坐标映射为正文字符位置。
     * @param option 当前项绘制选项。
     * @param index 当前消息索引。
     * @param viewPos 视图坐标。
     * @param clampToTextRect 是否夹取到正文边界。
     * @return [0, text.length()] 内的字符索引；未命中且不夹取时返回 -1。
     */
    int textPositionAt(const QStyleOptionViewItem &option,
                       const QModelIndex &index,
                       const QPoint &viewPos,
                       bool clampToTextRect = false) const;
    /**
     * @brief 更新当前正文选区状态。
     * @param index 选区所在消息索引。
     * @param anchor 选区锚点字符位置。
     * @param cursor 选区游标字符位置。
     */
    void setTextSelection(const QModelIndex &index, int anchor, int cursor);
    /**
     * @brief 清空当前正文选区。
     */
    void clearTextSelection();
    /**
     * @brief 判断是否存在可复制的正文选区。
     * @return true 表示存在有效选区。
     */
    bool hasTextSelection() const;
    /**
     * @brief 判断当前选区是否属于指定消息行。
     * @param index 待检查消息索引。
     * @return true 表示选区属于该行。
     */
    bool hasTextSelectionOnIndex(const QModelIndex &index) const;
    /**
     * @brief 读取当前选区文本片段。
     * @param model 可选模型指针，用于模型一致性校验。
     * @return 选中文本；无效选区时返回空字符串。
     */
    QString selectedText(const QAbstractItemModel *model) const;

    /**
     * @brief 绘制单条消息气泡内容。
     * @param painter 绘制器对象。
     * @param option 当前项绘制选项。
     * @param index 当前消息索引。
     */
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    /**
     * @brief 计算单条消息推荐尺寸。
     * @param option 当前项绘制选项。
     * @param index 当前消息索引。
     * @return 推荐尺寸（宽度跟随视图，高度由文本内容决定）。
     */
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    /**
     * @brief 根据视口宽度计算气泡允许的最大宽度。
     * @param viewportWidth 当前消息列表视口宽度（像素）。
     * @return 气泡最大宽度（已裁剪到最小/最大边界范围内）。
     */
    static int maxBubbleWidthForViewport(int viewportWidth);
    /**
     * @brief 将选区锚点和游标归一化为半开区间 [start, end)。
     * @param anchor 原始选区锚点字符位置。
     * @param cursor 原始选区游标字符位置。
     * @param start 输出参数，归一化后的起始位置。
     * @param end 输出参数，归一化后的结束位置（不包含）。
     * @return true 表示得到有效非空选区；false 表示无效或空选区。
     */
    static bool normalizedSelection(int anchor, int cursor, int *start, int *end);

    // 当前正文选区所在行（持久索引可在行插入/删除时尽量保持稳定）。
    QPersistentModelIndex m_selectionIndex;
    // 选区锚点与游标位置（单位：UTF-16 码元索引，语义与 QString::mid 一致）。
    int m_selectionAnchor = -1;
    int m_selectionCursor = -1;
};
