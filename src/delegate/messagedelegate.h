#pragma once

#include <QStyledItemDelegate>

// 聊天气泡绘制委托：
// - sizeHint(): 根据文本内容动态计算每行高度；
// - paint(): 直接在视口上绘制气泡、作者、正文和时间。
// 该类替代“每条消息一个 QWidget”的做法，提升大量消息时的渲染效率。
class MessageDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MessageDelegate(QObject *parent = nullptr);

    // 实际绘制单条消息气泡。
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    // 计算单行推荐尺寸。高度随正文行数变化，宽度跟随列表可视区。
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;

private:
    // 根据 viewport 宽度限制气泡最大宽度，避免超宽文本造成阅读困难。
    static int maxBubbleWidthForViewport(int viewportWidth);
};
