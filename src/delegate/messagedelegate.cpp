#include "messagedelegate.h"

#include "model/messagemodel.h"

#include <QAbstractItemModel>
#include <QColor>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QImageReader>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPixmapCache>
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
constexpr int kImageMaxWidth = 280;
constexpr int kImageMaxHeight = 220;
constexpr int kPlaceholderImageWidth = 220;
constexpr int kPlaceholderImageHeight = 150;

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

QFont transferStatusFont(const QFont &base)
{
    QFont font(base);
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
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

MessageType messageTypeFromIndex(const QModelIndex &index)
{
    // model 里用 int role 暴露消息类型，这里统一转回枚举，后续布局和绘制都复用这个入口。
    return static_cast<MessageType>(
        index.data(MessageModel::MessageTypeRole).toInt());
}

MessageTransferState transferStateFromIndex(const QModelIndex &index)
{
    return static_cast<MessageTransferState>(
        index.data(MessageModel::TransferStateRole).toInt());
}

QString messageBodyText(const QModelIndex &index)
{
    const MessageType messageType = messageTypeFromIndex(index);
    if (messageType == MessageType::Image ||
        messageType == MessageType::File)
    {
        return index.data(MessageModel::TextRole).toString();
    }

    return fallbackText(index);
}

QSize resolveImageSourceSize(const QModelIndex &index)
{
    // 图片尺寸优先级：
    // 1. 直接使用 model 已知宽高
    // 2. 退回到本地文件探测
    // 3. 仍然拿不到时使用占位尺寸
    const int width = index.data(MessageModel::ImageWidthRole).toInt();
    const int height = index.data(MessageModel::ImageHeightRole).toInt();
    if (width > 0 && height > 0) {
        return QSize(width, height);
    }

    const QString localPath =
        index.data(MessageModel::ImageLocalPathRole).toString().trimmed();
    if (localPath.isEmpty()) {
        return QSize(kPlaceholderImageWidth, kPlaceholderImageHeight);
    }

    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    const QSize size = reader.size();
    if (size.isValid()) {
        return size;
    }

    return QSize(kPlaceholderImageWidth, kPlaceholderImageHeight);
}

QSize scaledImageSize(const QSize &sourceSize, int maxWidth)
{
    const QSize safeSource = sourceSize.isValid()
                                 ? sourceSize
                                 : QSize(kPlaceholderImageWidth,
                                         kPlaceholderImageHeight);
    QSize target = safeSource;
    target.scale(qMin(maxWidth, kImageMaxWidth),
                 kImageMaxHeight,
                 Qt::KeepAspectRatio);
    if (!target.isValid() || target.width() <= 0 || target.height() <= 0) {
        return QSize(kPlaceholderImageWidth, kPlaceholderImageHeight);
    }
    return target;
}

QString imageCacheKey(const QString &localPath, const QSize &targetSize)
{
    return QStringLiteral("message_image:%1:%2x%3")
        .arg(localPath, QString::number(targetSize.width()),
             QString::number(targetSize.height()));
}

QPixmap loadImagePixmap(const QString &localPath, const QSize &targetSize)
{
    // 缩略图按“本地路径 + 目标尺寸”做缓存，避免列表滚动时反复解码同一张图片。
    if (localPath.trimmed().isEmpty() || !targetSize.isValid() ||
        !QFileInfo::exists(localPath))
    {
        return QPixmap();
    }

    const QString cacheKey = imageCacheKey(localPath, targetSize);
    QPixmap cachedPixmap;
    if (QPixmapCache::find(cacheKey, &cachedPixmap)) {
        return cachedPixmap;
    }

    QImageReader reader(localPath);
    reader.setAutoTransform(true);
    const QImage image = reader.read();
    if (image.isNull()) {
        return QPixmap();
    }

    const QPixmap pixmap = QPixmap::fromImage(
        image.scaled(targetSize,
                     Qt::KeepAspectRatioByExpanding,
                     Qt::SmoothTransformation));
    if (!pixmap.isNull()) {
        QPixmapCache::insert(cacheKey, pixmap);
    }
    return pixmap;
}

QString textForLayout(const QString &text)
{
    // QTextLayout 不会将 '\n' 直接当作硬换行；替换为段落分隔符后可与 drawText 的多行效果对齐。
    if (!text.contains(u'\n')) {
        return text;
    }
    QString layoutText = text;
    layoutText.replace(u'\n', QChar::LineSeparator);
    return layoutText;
}

qreal layoutTextHeight(const QString &text, const QFont &font, int textWidth)
{
    // 与 drawText(WordWrap) 对齐的高度计算：
    // 使用 QTextLayout 显式分行，避免仅靠 QFontMetrics::boundingRect 在特殊换行场景下偏差。
    const int safeTextWidth = qMax(kTextMinLayoutWidth, textWidth);
    const QFontMetrics metrics(font);
    const QString layoutText = textForLayout(text);
    if (layoutText.isEmpty()) {
        return metrics.lineSpacing();
    }

    QTextLayout layout(layoutText, font);
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
    QRect imageRect;
    QRect timeRect;
    int edgeAlign = Qt::AlignLeft;
    bool hasImage = false;
};

BubbleLayout buildBubbleLayout(const QStyleOptionViewItem &option,
                               const QModelIndex &index,
                               const QString &author,
                               const QString &text,
                               const QString &timeText,
                               bool fromSelf)
{
    // 统一先做一次布局计算，再让 sizeHint() 和 paint() 共同复用。
    // 这样消息气泡的测量和实际绘制才能保持一致，不容易出现裁剪或跳动。
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
    const MessageType messageType = messageTypeFromIndex(index);
    if (messageType == MessageType::Image) {
        const int maxBubbleWidth =
            maxBubbleWidthForViewportInternal(rowRect.width());
        const int maxContentWidth =
            qMax(kTextMinLayoutWidth, maxBubbleWidth - (kBubblePaddingX * 2));
        const QSize imageSize =
            scaledImageSize(resolveImageSourceSize(index), maxContentWidth);
        const int captionWidth =
            qMax(kTextMinLayoutWidth, maxBubbleWidth - (kBubblePaddingX * 2));
        const QRect captionBounds =
            text.trimmed().isEmpty()
                ? QRect()
                : messageMetrics.boundingRect(
                      QRect(0, 0, captionWidth, 20000),
                      Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
                      text);
        const int captionHeight = text.trimmed().isEmpty()
                                      ? 0
                                      : qCeil(layoutTextHeight(text,
                                                               mFont,
                                                               captionWidth));
        const int contentWidth = qMax(authorMetrics.horizontalAdvance(author),
                                      qMax(imageSize.width(),
                                           qMax(captionBounds.width(),
                                                timeMetrics.horizontalAdvance(
                                                    timeText))));
        const int bubbleWidth = qMin(maxBubbleWidth,
                                     qMax(kBubbleMinWidth,
                                          contentWidth +
                                              (kBubblePaddingX * 2)));
        const int bubbleHeight =
            (kBubblePaddingY * 2) + authorMetrics.height() + kBubbleSpacing +
            imageSize.height() +
            (captionHeight > 0 ? kBubbleSpacing + captionHeight : 0) +
            kBubbleSpacing + timeMetrics.height();

        const int bubbleLeft =
            fromSelf ? (rowRect.right() - bubbleWidth + 1) : rowRect.left();
        const QRect bubbleRect(bubbleLeft, rowRect.top(), bubbleWidth,
                               bubbleHeight);
        const int contentLeft = bubbleRect.left() + kBubblePaddingX;
        const int contentTop = bubbleRect.top() + kBubblePaddingY;
        const int edgeAlign = fromSelf ? Qt::AlignRight : Qt::AlignLeft;
        const QRect authorRect(contentLeft, contentTop,
                               bubbleWidth - (kBubblePaddingX * 2),
                               authorMetrics.height());
        const QRect imageRect(contentLeft,
                              authorRect.bottom() + 1 + kBubbleSpacing,
                              imageSize.width(),
                              imageSize.height());
        const QRect captionRect(
            contentLeft,
            imageRect.bottom() + 1 +
                (captionHeight > 0 ? kBubbleSpacing : 0),
            bubbleWidth - (kBubblePaddingX * 2),
            captionHeight);
        const QRect timeRect(
            contentLeft,
            (captionHeight > 0 ? captionRect.bottom() : imageRect.bottom()) +
                1 + kBubbleSpacing,
            bubbleWidth - (kBubblePaddingX * 2),
            timeMetrics.height());

        BubbleLayout layout;
        layout.bubbleRect = bubbleRect;
        layout.authorRect = authorRect;
        layout.imageRect = imageRect;
        layout.messageRect = captionRect;
        layout.timeRect = timeRect;
        layout.edgeAlign = edgeAlign;
        layout.hasImage = true;
        return layout;
    }

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
    const QString layoutText = textForLayout(text);
    if (selectStart >= selectEnd || layoutText.isEmpty() || messageRect.width() <= 0 || messageRect.height() <= 0) {
        return;
    }

    QTextLayout layout(layoutText, font);
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

    const QString layoutText = textForLayout(text);
    QTextLayout layout(layoutText, font);
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
    const QString text = messageBodyText(index);
    const QString timeText = index.data(MessageModel::TimeRole).toString();
    const bool fromSelf = index.data(MessageModel::FromSelfRole).toBool();
    const BubbleLayout layout =
        buildBubbleLayout(option, index, author, text, timeText, fromSelf);
    // 图片消息如果没有 caption，就不存在可选中的正文区域。
    if (layout.messageRect.height() <= 0 || text.isEmpty()) {
        return -1;
    }
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

    const QString text = messageBodyText(index);
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
    // 高度计算必须和 paint 使用同一套布局规则，否则图片和多行文本很容易被裁掉。
    const BubbleLayout layout = buildBubbleLayout(optionForLayout,
                                                  index,
                                                  author,
                                                  text,
                                                  timeText,
                                                  fromSelf);
    return QSize(viewportWidth, layout.bubbleRect.height() + (kRowVerticalMargin * 2));
}

void MessageDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // delegate 只负责“如何画”，不在这里改变消息业务状态。
    // 消息是否已确认、是否有未读都应该在 model/manager 层先算好。
    // 1) 拉取渲染所需数据。
    const QString author = index.data(MessageModel::AuthorRole).toString();
    const QString text = messageBodyText(index);
    const QString timeText = index.data(MessageModel::TimeRole).toString();
    const bool fromSelf = index.data(MessageModel::FromSelfRole).toBool();
    const MessageType messageType = messageTypeFromIndex(index);
    const MessageTransferState transferState = transferStateFromIndex(index);
    const int transferProgress =
        index.data(MessageModel::TransferProgressRole).toInt();
    const QString transferStatusText =
        index.data(MessageModel::TransferStatusTextRole).toString().trimmed();
    const QString imageLocalPath =
        index.data(MessageModel::ImageLocalPathRole).toString().trimmed();

    const QFont aFont = authorFont(option.font);
    const QFont mFont = messageFont(option.font);
    const QFont tFont = timeFont(option.font);
    const QFont statusFont = transferStatusFont(option.font);
    const BubbleLayout layout =
        buildBubbleLayout(option, index, author, text, timeText, fromSelf);

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

    if (layout.hasImage)
    {
        const QPixmap imagePixmap =
            loadImagePixmap(imageLocalPath, layout.imageRect.size());
        QPainterPath imagePath;
        imagePath.addRoundedRect(layout.imageRect, 10, 10);
        painter->save();
        painter->setClipPath(imagePath);
        if (!imagePixmap.isNull())
        {
            painter->drawPixmap(layout.imageRect, imagePixmap);
        }
        else
        {
            // 图片还没准备好时先画占位块，避免消息气泡完全空白。
            painter->fillRect(layout.imageRect,
                              QColor(QStringLiteral("#edf3fb")));
            painter->setPen(QColor(QStringLiteral("#8ea0bb")));
            painter->drawText(layout.imageRect,
                              Qt::AlignCenter,
                              QStringLiteral("图片"));
        }
        painter->restore();

        if (transferState != MessageTransferState::None)
        {
            // 图片消息在上传中 / 发送中 / 失败时，直接在图片区域叠一层状态遮罩。
            // 这样用户不需要切出会话，也能一眼看出当前这张图有没有真的发出去。
            QPainterPath overlayPath;
            overlayPath.addRoundedRect(layout.imageRect, 10, 10);

            painter->save();
            painter->setClipPath(overlayPath);
            const QColor overlayColor =
                transferState == MessageTransferState::Failed
                    ? QColor(20, 33, 52, 168)
                    : QColor(20, 33, 52, 112);
            painter->fillRect(layout.imageRect, overlayColor);

            if ((transferState == MessageTransferState::Uploading ||
                 transferState == MessageTransferState::Sending) &&
                transferProgress >= 0)
            {
                const QRect trackRect(
                    layout.imageRect.left() + 18,
                    layout.imageRect.bottom() - 18,
                    qMax(24, layout.imageRect.width() - 36),
                    6);
                const int progressWidth = qBound(
                    0,
                    static_cast<int>((trackRect.width() * transferProgress) /
                                     100.0),
                    trackRect.width());
                const QRect fillRect(trackRect.left(),
                                     trackRect.top(),
                                     progressWidth,
                                     trackRect.height());

                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(255, 255, 255, 72));
                painter->drawRoundedRect(trackRect, 3, 3);
                painter->setBrush(QColor(QStringLiteral("#dcecff")));
                painter->drawRoundedRect(fillRect, 3, 3);
            }
            painter->restore();

            painter->save();
            painter->setFont(statusFont);
            painter->setPen(QColor(QStringLiteral("#ffffff")));
            const QRect statusRect =
                layout.imageRect.adjusted(18, 14, -18, -28);
            painter->drawText(statusRect,
                              Qt::AlignCenter | Qt::TextWordWrap,
                              transferStatusText);
            painter->restore();
        }

        if (!layout.messageRect.isEmpty() && !text.isEmpty())
        {
            int selectStart = 0;
            int selectEnd = 0;
            if (hasTextSelectionOnIndex(index) &&
                normalizedSelection(m_selectionAnchor,
                                    m_selectionCursor,
                                    &selectStart,
                                    &selectEnd))
            {
                drawTextSelectionBackground(
                    painter, layout.messageRect, text, mFont, selectStart,
                    selectEnd);
            }

            painter->setFont(mFont);
            painter->setPen(QColor(QStringLiteral("#162740")));
            painter->drawText(layout.messageRect,
                              Qt::TextWordWrap | Qt::AlignLeft |
                                  Qt::AlignTop,
                              text);
        }
    }
    else
    {
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
    }

    painter->setFont(tFont);
    painter->setPen(QColor(QStringLiteral("#7d8ea8")));
    painter->drawText(layout.timeRect, layout.edgeAlign | Qt::AlignVCenter, timeText);

    painter->restore();
}
