#include "messagedelegate.h"

#include "model/messagemodel.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QListView>
#include <QPainter>
#include <QRect>

namespace {
// 行级留白：让消息之间有呼吸感，同时给滚动条预留视觉空间。
constexpr int kRowHorizontalMargin = 12;
constexpr int kRowVerticalMargin = 5;
// 气泡内边距与元素间距：与旧版 QWidget 气泡保持一致。
constexpr int kBubblePaddingX = 12;
constexpr int kBubblePaddingY = 10;
constexpr int kBubbleSpacing = 6;
constexpr int kBubbleRadius = 12;
// 气泡宽度上限与下限：
// - 上限避免超宽一行难读；
// - 下限避免短消息（如“OK”）气泡过窄导致观感抖动。
constexpr int kBubbleMaxWidth = 520;
constexpr int kBubbleMinWidth = 120;

QFont authorFont(const QFont &base)
{
    QFont font(base);
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
    return font;
}

QFont messageFont(const QFont &base)
{
    QFont font(base);
    font.setPixelSize(14);
    font.setWeight(QFont::Normal);
    return font;
}

QFont timeFont(const QFont &base)
{
    QFont font(base);
    font.setPixelSize(11);
    font.setWeight(QFont::Normal);
    return font;
}
} // namespace

MessageDelegate::MessageDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

int MessageDelegate::maxBubbleWidthForViewport(int viewportWidth)
{
    // 气泡最大宽度不能超过行内容区，且必须落在 [kBubbleMinWidth, kBubbleMaxWidth] 之间。
    const int availableWidth = viewportWidth - (kRowHorizontalMargin * 2);
    return qMax(kBubbleMinWidth, qMin(kBubbleMaxWidth, availableWidth));
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 尽量用实际 viewport 宽度计算换行高度，避免窗口缩放后高度估算失真。
    int viewportWidth = option.rect.width();
    if (const auto *view = qobject_cast<const QListView *>(option.widget)) {
        viewportWidth = view->viewport()->width();
    }
    // 兜底值：在 early layout 阶段 option.rect 可能为 0。
    if (viewportWidth <= 0) {
        viewportWidth = 760;
    }

    // 从 model 读取当前消息字段。delegate 不保存状态，始终以 model 为单一事实来源。
    const QString author = index.data(MessageModel::AuthorRole).toString();
    const QString text = index.data(Qt::DisplayRole).toString();
    const QString timeText = index.data(MessageModel::TimeRole).toString();

    const QFont baseFont = option.font;
    const QFont aFont = authorFont(baseFont);
    const QFont mFont = messageFont(baseFont);
    const QFont tFont = timeFont(baseFont);

    const QFontMetrics authorMetrics(aFont);
    const QFontMetrics messageMetrics(mFont);
    const QFontMetrics timeMetrics(tFont);

    const int maxBubbleWidth = maxBubbleWidthForViewport(viewportWidth);
    // 正文布局宽度 = 气泡宽度上限 - 左右 padding。
    const int textLayoutWidth = qMax(40, maxBubbleWidth - (kBubblePaddingX * 2));
    // 通过 boundingRect 估算多行文本高度，和 paint() 的绘制规则保持一致。
    const QRect textBounds = messageMetrics.boundingRect(QRect(0, 0, textLayoutWidth, 20000),
                                                         Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                                         text);
    const int textHeight = qMax(messageMetrics.lineSpacing(), textBounds.height());

    // 总高度 = 上下 padding + 作者 + 正文 + 时间 + 两段垂直间距。
    const int bubbleHeight = (kBubblePaddingY * 2)
                           + authorMetrics.height()
                           + kBubbleSpacing
                           + textHeight
                           + kBubbleSpacing
                           + timeMetrics.height();
    return QSize(viewportWidth, bubbleHeight + (kRowVerticalMargin * 2));
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 1) 拉取渲染所需数据。
    const QString author = index.data(MessageModel::AuthorRole).toString();
    const QString text = index.data(Qt::DisplayRole).toString();
    const QString timeText = index.data(MessageModel::TimeRole).toString();
    const bool fromSelf = index.data(MessageModel::FromSelfRole).toBool();

    const QFont baseFont = option.font;
    const QFont aFont = authorFont(baseFont);
    const QFont mFont = messageFont(baseFont);
    const QFont tFont = timeFont(baseFont);

    const QFontMetrics authorMetrics(aFont);
    const QFontMetrics messageMetrics(mFont);
    const QFontMetrics timeMetrics(tFont);

    // 2) 先得到当前 item 的可绘制区域（扣除行间外边距）。
    const QRect rowRect = option.rect.adjusted(kRowHorizontalMargin,
                                               kRowVerticalMargin,
                                               -kRowHorizontalMargin,
                                               -kRowVerticalMargin);
    const int maxBubbleWidth = maxBubbleWidthForViewport(rowRect.width());
    const int textLayoutWidth = qMax(40, maxBubbleWidth - (kBubblePaddingX * 2));
    const QRect textBounds = messageMetrics.boundingRect(QRect(0, 0, textLayoutWidth, 20000),
                                                         Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                                         text);
    const int textHeight = qMax(messageMetrics.lineSpacing(), textBounds.height());
    const int contentWidth = qMax(authorMetrics.horizontalAdvance(author),
                                  qMax(textBounds.width(), timeMetrics.horizontalAdvance(timeText)));
    const int bubbleWidth = qMin(maxBubbleWidth, qMax(kBubbleMinWidth, contentWidth + (kBubblePaddingX * 2)));
    const int bubbleHeight = (kBubblePaddingY * 2)
                           + authorMetrics.height()
                           + kBubbleSpacing
                           + textHeight
                           + kBubbleSpacing
                           + timeMetrics.height();

    // 3) 依据消息归属决定气泡左右停靠。
    const int bubbleLeft = fromSelf ? (rowRect.right() - bubbleWidth + 1) : rowRect.left();
    const QRect bubbleRect(bubbleLeft, rowRect.top(), bubbleWidth, bubbleHeight);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QColor bubbleBg = fromSelf ? QColor(QStringLiteral("#dcecff")) : QColor(QStringLiteral("#ffffff"));
    const QColor bubbleBorder = fromSelf ? QColor(QStringLiteral("#b8d3f6")) : QColor(QStringLiteral("#dbe5f6"));

    painter->setBrush(bubbleBg);
    painter->setPen(QPen(bubbleBorder, 1));
    painter->drawRoundedRect(bubbleRect, kBubbleRadius, kBubbleRadius);

    // 4) 依次绘制作者、正文、时间。正文始终左对齐，作者/时间随归属左右对齐。
    int textLeft = bubbleRect.left() + kBubblePaddingX;
    int textTop = bubbleRect.top() + kBubblePaddingY;
    const int textWidth = bubbleRect.width() - (kBubblePaddingX * 2);
    const int edgeAlign = fromSelf ? Qt::AlignRight : Qt::AlignLeft;

    const QRect authorRect(textLeft, textTop, textWidth, authorMetrics.height());
    painter->setFont(aFont);
    painter->setPen(QColor(QStringLiteral("#3d4f6b")));
    painter->drawText(authorRect, edgeAlign | Qt::AlignVCenter, author);

    textTop += authorMetrics.height() + kBubbleSpacing;
    const QRect messageRect(textLeft, textTop, textWidth, textHeight);
    painter->setFont(mFont);
    painter->setPen(QColor(QStringLiteral("#162740")));
    painter->drawText(messageRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);

    textTop += textHeight + kBubbleSpacing;
    const QRect timeRect(textLeft, textTop, textWidth, timeMetrics.height());
    painter->setFont(tFont);
    painter->setPen(QColor(QStringLiteral("#7d8ea8")));
    painter->drawText(timeRect, edgeAlign | Qt::AlignVCenter, timeText);

    painter->restore();
}
