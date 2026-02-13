#include "messagedelegate.h"

#include "model/messagemodel.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QListView>
#include <QPainter>
#include <QRect>
#include <QTextLayout>
#include <QTextOption>
#include <QtMath>

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
constexpr int kTextMinLayoutWidth = 40;

int maxBubbleWidthForViewportInternal(int viewportWidth)
{
    const int availableWidth = viewportWidth - (kRowHorizontalMargin * 2);
    return qMax(kBubbleMinWidth, qMin(kBubbleMaxWidth, availableWidth));
}

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

QString fallbackText(const QModelIndex &index)
{
    // 复制与命中逻辑优先使用 TextRole：
    // - TextRole 表示“纯正文”；
    // - DisplayRole 可能是 image/file 的占位文案，作为兜底。
    QString text = index.data(MessageModel::TextRole).toString();
    if (text.isEmpty()) {
        text = index.data(Qt::DisplayRole).toString();
    }
    return text;
}

qreal layoutTextHeight(const QString &text, const QFont &font, int textWidth)
{
    // 与 drawText(WordWrap) 对齐的高度计算：
    // 使用 QTextLayout 显式分行，避免仅靠 QFontMetrics::boundingRect 在特殊换行场景下偏差。
    const int safeTextWidth = qMax(kTextMinLayoutWidth, textWidth);
    const QFontMetrics metrics(font);
    if (text.isEmpty()) {
        return metrics.lineSpacing();
    }

    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    layout.beginLayout();
    qreal y = 0.0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(safeTextWidth);
        line.setPosition(QPointF(0.0, y));
        y += line.height();
    }
    layout.endLayout();
    return qMax<qreal>(metrics.lineSpacing(), y);
}

struct BubbleLayout {
    QRect bubbleRect;
    QRect authorRect;
    QRect messageRect;
    QRect timeRect;
    int edgeAlign = Qt::AlignLeft;
};

BubbleLayout buildBubbleLayout(const QStyleOptionViewItem &option,
                               const QString &author,
                               const QString &text,
                               const QString &timeText,
                               bool fromSelf)
{
    const QFont aFont = authorFont(option.font);
    const QFont mFont = messageFont(option.font);
    const QFont tFont = timeFont(option.font);
    const QFontMetrics authorMetrics(aFont);
    const QFontMetrics messageMetrics(mFont);
    const QFontMetrics timeMetrics(tFont);

    const QRect rowRect = option.rect.adjusted(kRowHorizontalMargin,
                                               kRowVerticalMargin,
                                               -kRowHorizontalMargin,
                                               -kRowVerticalMargin);
    // 先按“最大文本宽度”估算内容宽，再夹到 [min,max] 形成最终气泡宽。
    // 这样既可避免超长单行影响可读性，也能避免短消息过窄造成视觉抖动。
    const int maxBubbleWidth = maxBubbleWidthForViewportInternal(rowRect.width());
    const int maxTextWidth = qMax(kTextMinLayoutWidth, maxBubbleWidth - (kBubblePaddingX * 2));
    const QRect textBounds = messageMetrics.boundingRect(QRect(0, 0, maxTextWidth, 20000),
                                                         Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                                                         text);
    const int contentWidth = qMax(authorMetrics.horizontalAdvance(author),
                                  qMax(textBounds.width(), timeMetrics.horizontalAdvance(timeText)));
    const int bubbleWidth = qMin(maxBubbleWidth, qMax(kBubbleMinWidth, contentWidth + (kBubblePaddingX * 2)));

    const int textWidth = qMax(kTextMinLayoutWidth, bubbleWidth - (kBubblePaddingX * 2));
    const int textHeight = qCeil(layoutTextHeight(text, mFont, textWidth));
    const int bubbleHeight = (kBubblePaddingY * 2)
                           + authorMetrics.height()
                           + kBubbleSpacing
                           + textHeight
                           + kBubbleSpacing
                           + timeMetrics.height();

    const int bubbleLeft = fromSelf ? (rowRect.right() - bubbleWidth + 1) : rowRect.left();
    const QRect bubbleRect(bubbleLeft, rowRect.top(), bubbleWidth, bubbleHeight);

    const int textLeft = bubbleRect.left() + kBubblePaddingX;
    const int textTop = bubbleRect.top() + kBubblePaddingY;
    const int edgeAlign = fromSelf ? Qt::AlignRight : Qt::AlignLeft;

    BubbleLayout layout;
    layout.bubbleRect = bubbleRect;
    layout.authorRect = QRect(textLeft, textTop, textWidth, authorMetrics.height());
    layout.messageRect = QRect(textLeft,
                               textTop + authorMetrics.height() + kBubbleSpacing,
                               textWidth,
                               textHeight);
    layout.timeRect = QRect(textLeft,
                            layout.messageRect.bottom() + 1 + kBubbleSpacing,
                            textWidth,
                            timeMetrics.height());
    layout.edgeAlign = edgeAlign;
    return layout;
}

void drawTextSelectionBackground(QPainter *painter,
                                 const QRect &messageRect,
                                 const QString &text,
                                 const QFont &font,
                                 int selectStart,
                                 int selectEnd)
{
    if (selectStart >= selectEnd || text.isEmpty() || messageRect.width() <= 0 || messageRect.height() <= 0) {
        return;
    }

    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    layout.beginLayout();
    qreal y = 0.0;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(messageRect.width());
        line.setPosition(QPointF(0.0, y));
        y += line.height();

        const int lineStart = line.textStart();
        const int lineEnd = lineStart + line.textLength();
        const int segmentStart = qMax(selectStart, lineStart);
        const int segmentEnd = qMin(selectEnd, lineEnd);
        if (segmentStart >= segmentEnd) {
            continue;
        }

        // 选区是“字符索引区间”，需要投影为每一行内的 [xStart, xEnd] 像素区间。
        const qreal xStart = line.cursorToX(segmentStart);
        const qreal xEnd = line.cursorToX(segmentEnd);
        const QRectF selectedRect(messageRect.left() + xStart,
                                  messageRect.top() + line.y(),
                                  qMax<qreal>(1.0, xEnd - xStart),
                                  line.height());
        painter->fillRect(selectedRect, QColor(QStringLiteral("#93bdff")));
    }
    layout.endLayout();
}

int cursorAtTextPosition(const QRect &messageRect,
                         const QString &text,
                         const QFont &font,
                         const QPoint &viewPos,
                         bool clampToTextRect)
{
    if (messageRect.width() <= 0 || messageRect.height() <= 0) {
        return -1;
    }

    QPoint samplePos = viewPos;
    if (!messageRect.contains(samplePos)) {
        if (!clampToTextRect) {
            // 精确命中模式：点在正文外直接视为“无字符命中”。
            return -1;
        }
        // 拖拽模式：将鼠标坐标夹到正文边界，得到连续可预期的选区端点。
        samplePos.setX(qBound(messageRect.left(), samplePos.x(), messageRect.right()));
        samplePos.setY(qBound(messageRect.top(), samplePos.y(), messageRect.bottom()));
    }

    const qreal localX = samplePos.x() - messageRect.left();
    const qreal localY = samplePos.y() - messageRect.top();
    if (text.isEmpty()) {
        return 0;
    }

    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    layout.beginLayout();
    qreal y = 0.0;
    QTextLine targetLine;
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(messageRect.width());
        line.setPosition(QPointF(0.0, y));
        y += line.height();

        targetLine = line;
        if (localY <= (line.y() + line.height())) {
            // 命中到第一条满足 y 条件的行即停止，保持 O(可见行数) 成本。
            break;
        }
    }
    layout.endLayout();

    if (!targetLine.isValid()) {
        return 0;
    }

    const int cursor = targetLine.xToCursor(localX, QTextLine::CursorBetweenCharacters);
    return qBound(0, cursor, text.size());
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

bool MessageDelegate::normalizedSelection(int anchor, int cursor, int *start, int *end)
{
    if (anchor < 0 || cursor < 0 || !start || !end) {
        return false;
    }

    *start = qMin(anchor, cursor);
    *end = qMax(anchor, cursor);
    // 半开区间 [start, end) 约定：end==start 代表空选区，不应触发复制与高亮。
    return *end > *start;
}

int MessageDelegate::textPositionAt(const QStyleOptionViewItem &option,
                                    const QModelIndex &index,
                                    const QPoint &viewPos,
                                    bool clampToTextRect) const
{
    if (!index.isValid()) {
        return -1;
    }

    const QString author = index.data(MessageModel::AuthorRole).toString();
    const QString text = fallbackText(index);
    const QString timeText = index.data(MessageModel::TimeRole).toString();
    const bool fromSelf = index.data(MessageModel::FromSelfRole).toBool();
    const BubbleLayout layout = buildBubbleLayout(option, author, text, timeText, fromSelf);
    return cursorAtTextPosition(layout.messageRect,
                                text,
                                messageFont(option.font),
                                viewPos,
                                clampToTextRect);
}

void MessageDelegate::setTextSelection(const QModelIndex &index, int anchor, int cursor)
{
    if (!index.isValid()) {
        clearTextSelection();
        return;
    }

    const QString text = fallbackText(index);
    const int maxCursor = text.size();
    // 外部传入的 anchor/cursor 允许越界，这里统一裁剪到合法范围，避免后续取子串崩溃。
    m_selectionIndex = QPersistentModelIndex(index);
    m_selectionAnchor = qBound(0, anchor, maxCursor);
    m_selectionCursor = qBound(0, cursor, maxCursor);
}

void MessageDelegate::clearTextSelection()
{
    m_selectionIndex = QPersistentModelIndex();
    m_selectionAnchor = -1;
    m_selectionCursor = -1;
}

bool MessageDelegate::hasTextSelection() const
{
    if (!m_selectionIndex.isValid()) {
        return false;
    }
    int start = 0;
    int end = 0;
    return normalizedSelection(m_selectionAnchor, m_selectionCursor, &start, &end);
}

bool MessageDelegate::hasTextSelectionOnIndex(const QModelIndex &index) const
{
    return hasTextSelection() && index.isValid() && (index == QModelIndex(m_selectionIndex));
}

QString MessageDelegate::selectedText(const QAbstractItemModel *model) const
{
    if (!hasTextSelection() || !m_selectionIndex.isValid()) {
        return QString();
    }
    if (model && m_selectionIndex.model() != model) {
        // 避免外部误把旧选区应用到其它模型实例上。
        return QString();
    }

    const QString text = fallbackText(QModelIndex(m_selectionIndex));
    if (text.isEmpty()) {
        return QString();
    }

    int start = 0;
    int end = 0;
    if (!normalizedSelection(m_selectionAnchor, m_selectionCursor, &start, &end)) {
        return QString();
    }
    if (start >= text.size()) {
        return QString();
    }
    // 使用 QString::mid 与 UTF-16 索引语义保持一致。
    return text.mid(start, end - start);
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

    const QString author = index.data(MessageModel::AuthorRole).toString();
    const QString text = fallbackText(index);
    const QString timeText = index.data(MessageModel::TimeRole).toString();
    const bool fromSelf = index.data(MessageModel::FromSelfRole).toBool();

    QStyleOptionViewItem optionForLayout(option);
    optionForLayout.rect.setWidth(viewportWidth);
    const BubbleLayout layout = buildBubbleLayout(optionForLayout, author, text, timeText, fromSelf);
    return QSize(viewportWidth, layout.bubbleRect.height() + (kRowVerticalMargin * 2));
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // 1) 拉取渲染所需数据。
    const QString author = index.data(MessageModel::AuthorRole).toString();
    const QString text = fallbackText(index);
    const QString timeText = index.data(MessageModel::TimeRole).toString();
    const bool fromSelf = index.data(MessageModel::FromSelfRole).toBool();

    const QFont aFont = authorFont(option.font);
    const QFont mFont = messageFont(option.font);
    const QFont tFont = timeFont(option.font);
    const BubbleLayout layout = buildBubbleLayout(option, author, text, timeText, fromSelf);

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QColor bubbleBg = fromSelf ? QColor(QStringLiteral("#dcecff")) : QColor(QStringLiteral("#ffffff"));
    const QColor bubbleBorder = fromSelf ? QColor(QStringLiteral("#b8d3f6")) : QColor(QStringLiteral("#dbe5f6"));

    painter->setBrush(bubbleBg);
    painter->setPen(QPen(bubbleBorder, 1));
    painter->drawRoundedRect(layout.bubbleRect, kBubbleRadius, kBubbleRadius);

    // 4) 依次绘制作者、正文、时间。正文始终左对齐，作者/时间随归属左右对齐。
    painter->setFont(aFont);
    painter->setPen(QColor(QStringLiteral("#3d4f6b")));
    painter->drawText(layout.authorRect, layout.edgeAlign | Qt::AlignVCenter, author);

    int selectStart = 0;
    int selectEnd = 0;
    if (hasTextSelectionOnIndex(index)
        && normalizedSelection(m_selectionAnchor, m_selectionCursor, &selectStart, &selectEnd)) {
        // 先绘制选区背景再绘制正文，保证文字始终在高亮层上方清晰可读。
        drawTextSelectionBackground(painter, layout.messageRect, text, mFont, selectStart, selectEnd);
    }

    painter->setFont(mFont);
    painter->setPen(QColor(QStringLiteral("#162740")));
    painter->drawText(layout.messageRect, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);

    painter->setFont(tFont);
    painter->setPen(QColor(QStringLiteral("#7d8ea8")));
    painter->drawText(layout.timeRect, layout.edgeAlign | Qt::AlignVCenter, timeText);

    painter->restore();
}
