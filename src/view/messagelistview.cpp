#include "messagelistview.h"

#include "delegate/messagedelegate.h"
#include "model/messagemodel.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QClipboard>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QStyleOptionViewItem>

MessageListView::MessageListView(QWidget *parent)
    : QListView(parent)
{
    // MessageListView 自身内置默认行为，外部（如 ChatWindow）只负责创建和使用。
    setFrameShape(QFrame::NoFrame);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 聊天气泡高度不固定，按像素滚动比按 item 滚动更平滑。
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    // 保留“当前消息”语义，供右键复制与键盘复制使用。
    setSelectionMode(QAbstractItemView::SingleSelection);
    // 聊天记录区为只读，不允许进入编辑态。
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    // 各行高度由 delegate::sizeHint 动态计算，不能开启 uniformItemSizes。
    setUniformItemSizes(false);
    // 点击后允许获得焦点，这样 Ctrl+C 能命中本控件。
    setFocusPolicy(Qt::ClickFocus);

    // delegate 由视图自身托管；model 由外部注入（如 MessageModelRegistry）。
    setItemDelegate(new MessageDelegate(this));

    // 上下文菜单和复制快捷键都在 MessageListView 内部闭环。
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QListView::customContextMenuRequested, this, &MessageListView::showMessageContextMenu);
    auto *copyShortcut = new QShortcut(QKeySequence::Copy, this);
    copyShortcut->setContext(Qt::WidgetShortcut);
    connect(copyShortcut, &QShortcut::activated, this, &MessageListView::copyCurrentMessageText);

    // hover 场景下也需要持续收到 mouse move，才能实时切换 IBeam 光标。
    setMouseTracking(true);
    if (viewport()) {
        // QListView 实际接收鼠标事件的是 viewport，必须同步开启。
        viewport()->setMouseTracking(true);
    }
}

void MessageListView::setMessageModel(QAbstractItemModel *model)
{
    if (model == this->model()) {
        return;
    }

    clearSelectedText();
    m_dragSelecting = false;
    m_dragIndex = QPersistentModelIndex();
    m_dragAnchor = -1;
    resetHoverCursor();
    QListView::setModel(model);
}

bool MessageListView::hasSelectedText() const
{
    const MessageDelegate *delegate = messageDelegate();
    return delegate && delegate->hasTextSelection();
}

bool MessageListView::hasSelectedTextOnIndex(const QModelIndex &index) const
{
    const MessageDelegate *delegate = messageDelegate();
    return delegate && delegate->hasTextSelectionOnIndex(index);
}

QString MessageListView::selectedText() const
{
    const MessageDelegate *delegate = messageDelegate();
    if (!delegate) {
        return QString();
    }
    return delegate->selectedText(model());
}

bool MessageListView::copySelectedTextToClipboard() const
{
    const QString text = selectedText();
    if (text.isEmpty()) {
        return false;
    }

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        // 同时写入 Clipboard 与 Selection：
        // - Clipboard：通用 Ctrl+V 来源；
        // - Selection：Linux 下中键粘贴来源。
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
    return true;
}

void MessageListView::clearSelectedText()
{
    if (MessageDelegate *delegate = messageDelegate()) {
        if (delegate->hasTextSelection()) {
            delegate->clearTextSelection();
            viewport()->update();
        }
    }
}

void MessageListView::mousePressEvent(QMouseEvent *event)
{
    // 非左键（如右键弹菜单）保持 QListView 默认行为，避免破坏现有交互。
    if (event->button() != Qt::LeftButton) {
        QListView::mousePressEvent(event);
        return;
    }

    MessageDelegate *delegate = messageDelegate();
    if (!delegate) {
        // 未安装 MessageDelegate 时无法做字符级命中，退回基类。
        QListView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        // 点在空白区：清选区、重置拖拽状态，让列表走默认点击行为。
        clearSelectedText();
        m_dragSelecting = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        QListView::mousePressEvent(event);
        return;
    }

    if (QItemSelectionModel *selModel = selectionModel()) {
        // 先同步“当前消息”语义，保证后续 Ctrl+C 与右键菜单基于同一行。
        selModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    } else {
        setCurrentIndex(index);
    }

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    option.widget = this;

    const int cursorPos = delegate->textPositionAt(option, index, event->pos(), false);
    if (cursorPos < 0) {
        // 命中的是气泡非正文区域（如作者/时间/留白）：不进入拖拽选区。
        resetHoverCursor();
        clearSelectedText();
        QListView::mousePressEvent(event);
        return;
    }

    // 命中正文：进入“单行字符选区”模式，anchor/cursor 初始都在按下点。
    m_dragSelecting = true;
    m_dragIndex = QPersistentModelIndex(index);
    m_dragAnchor = cursorPos;
    delegate->setTextSelection(index, cursorPos, cursorPos);
    viewport()->update(visualRect(index));
    viewport()->setCursor(Qt::IBeamCursor);
    event->accept();
}

void MessageListView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QListView::mouseDoubleClickEvent(event);
        return;
    }

    MessageDelegate *delegate = messageDelegate();
    if (!delegate) {
        QListView::mouseDoubleClickEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        clearSelectedText();
        m_dragSelecting = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        QListView::mouseDoubleClickEvent(event);
        return;
    }

    if (QItemSelectionModel *selModel = selectionModel()) {
        selModel->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect);
    } else {
        setCurrentIndex(index);
    }

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    option.widget = this;

    const int cursorPos = delegate->textPositionAt(option, index, event->pos(), false);
    if (cursorPos < 0) {
        QListView::mouseDoubleClickEvent(event);
        return;
    }

    QString text = index.data(MessageModel::TextRole).toString();
    if (text.isEmpty()) {
        text = index.data(Qt::DisplayRole).toString();
    }
    const int textLength = text.size();
    delegate->setTextSelection(index, 0, textLength);
    viewport()->update(visualRect(index));
    viewport()->setCursor(Qt::IBeamCursor);

    // 双击全选后结束拖拽态，避免 mouseReleaseEvent 把选区重算回单点。
    m_dragSelecting = false;
    m_dragIndex = QPersistentModelIndex();
    m_dragAnchor = -1;
    event->accept();
}

void MessageListView::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragSelecting || !(event->buttons() & Qt::LeftButton)) {
        // 非拖拽态：仅维护 hover 光标，不改变选区。
        updateHoverCursor(event->pos());
        QListView::mouseMoveEvent(event);
        return;
    }

    MessageDelegate *delegate = messageDelegate();
    const QModelIndex index(m_dragIndex);
    if (!delegate || !index.isValid()) {
        // 视图刷新/模型变化导致拖拽上下文失效时，及时退出拖拽态。
        m_dragSelecting = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        QListView::mouseMoveEvent(event);
        return;
    }

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    option.widget = this;

    const int cursorPos = delegate->textPositionAt(option, index, event->pos(), true);
    if (cursorPos >= 0) {
        // clamp=true：即便鼠标稍微移出正文矩形，也会夹取到边界位置，拖拽更连贯。
        delegate->setTextSelection(index, m_dragAnchor, cursorPos);
        viewport()->update(visualRect(index));
    }
    viewport()->setCursor(Qt::IBeamCursor);
    event->accept();
}

void MessageListView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_dragSelecting) {
        // 非“左键释放拖拽”场景保持默认行为。
        QListView::mouseReleaseEvent(event);
        return;
    }

    MessageDelegate *delegate = messageDelegate();
    const QModelIndex index(m_dragIndex);
    if (delegate && index.isValid()) {
        QStyleOptionViewItem option;
        initViewItemOption(&option);
        option.rect = visualRect(index);
        option.widget = this;

        const int cursorPos = delegate->textPositionAt(option, index, event->pos(), true);
        if (cursorPos >= 0) {
            // 释放时再做一次最终 cursor 结算，确保最后一帧选区准确。
            delegate->setTextSelection(index, m_dragAnchor, cursorPos);
            viewport()->update(visualRect(index));
        }
    }

    m_dragSelecting = false;
    m_dragIndex = QPersistentModelIndex();
    m_dragAnchor = -1;
    updateHoverCursor(event->pos());
    event->accept();
}

void MessageListView::leaveEvent(QEvent *event)
{
    // 鼠标离开列表可视区后恢复默认箭头，避免文本光标停留在其它控件上方时违和。
    resetHoverCursor();
    QListView::leaveEvent(event);
}

void MessageListView::showMessageContextMenu(const QPoint &pos)
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        return;
    }

    setCurrentIndex(index);
    QMenu menu(this);
    // 仅当“右键所在行 == 当前文本选区所在行”时显示“复制选中文本”，
    // 避免用户右键其它消息时误以为会复制该行子串。
    const bool hasSelectedText = hasSelectedTextOnIndex(index);
    QAction *copyAction = menu.addAction(hasSelectedText ? QStringLiteral("复制选中文本")
                                                          : QStringLiteral("复制消息"));

    const QAction *chosen = menu.exec(viewport()->mapToGlobal(pos));
    if (chosen != copyAction) {
        return;
    }

    if (hasSelectedText && copySelectedTextToClipboard()) {
        return;
    }
    copyMessageText(index);
}

void MessageListView::copyCurrentMessageText()
{
    // 复制优先级：
    // 1) 若当前存在“正文子串选区”，优先复制选中片段；
    // 2) 否则回退为“复制当前消息整条文案”。
    if (copySelectedTextToClipboard()) {
        return;
    }
    copyMessageText(currentIndex());
}

void MessageListView::copyMessageText(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return;
    }

    QString text = index.data(MessageModel::TextRole).toString();
    // 非文本消息可能没有纯 text 字段，此时回退复制展示文案（如 [图片消息]/[文件] xxx）。
    if (text.isEmpty()) {
        text = index.data(Qt::DisplayRole).toString();
    }
    if (text.isEmpty()) {
        return;
    }

    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text, QClipboard::Clipboard);
        clipboard->setText(text, QClipboard::Selection);
    }
}

MessageDelegate *MessageListView::messageDelegate() const
{
    return qobject_cast<MessageDelegate *>(itemDelegate());
}

void MessageListView::updateHoverCursor(const QPoint &viewPos)
{
    MessageDelegate *delegate = messageDelegate();
    if (!delegate) {
        resetHoverCursor();
        return;
    }

    const QModelIndex index = indexAt(viewPos);
    if (!index.isValid()) {
        // 未命中任何 item（行间空白、底部空白）时应恢复默认光标。
        resetHoverCursor();
        return;
    }

    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.rect = visualRect(index);
    option.widget = this;

    const int cursorPos = delegate->textPositionAt(option, index, viewPos, false);
    if (cursorPos >= 0) {
        // 仅正文区域显示 IBeam；作者/时间/气泡留白保持默认箭头。
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        resetHoverCursor();
    }
}

void MessageListView::resetHoverCursor()
{
    if (viewport()) {
        viewport()->unsetCursor();
    }
}
