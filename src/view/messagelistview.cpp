#include "messagelistview.h"

#include "delegate/messagedelegate.h"
#include "model/messagemodel.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QMenu>
#include <QMouseEvent>
#include <QShortcut>
#include <QStandardPaths>
#include <QStyleOptionViewItem>

// MessageListView 是消息区域的交互外壳：
// - delegate 负责画气泡
// - model 负责提供数据
// - view 自己负责选择、拖拽选中、右键复制和快捷键行为
//
// 这样 ChatWindow 只需要关心“当前显示哪个 model”，不需要自己处理
// 文本复制或局部交互细节。
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
    m_pressedFileCardIndex = QPersistentModelIndex();
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
        m_pressedFileCardIndex = QPersistentModelIndex();
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

    if (delegate->fileCardContains(option, index, event->pos())) {
        // 文件卡片是“点击打开 / 下载”的交互区，不进入文本选择态。
        clearSelectedText();
        m_dragSelecting = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        m_pressedFileCardIndex = QPersistentModelIndex(index);
        viewport()->setCursor(Qt::PointingHandCursor);
        event->accept();
        return;
    }

    const int cursorPos = delegate->textPositionAt(option, index, event->pos(), false);
    if (cursorPos < 0) {
        // 命中的是气泡非正文区域（如作者/时间/留白）：不进入拖拽选区。
        m_pressedFileCardIndex = QPersistentModelIndex();
        resetHoverCursor();
        clearSelectedText();
        QListView::mousePressEvent(event);
        return;
    }

    // 命中正文：进入“单行字符选区”模式，anchor/cursor 初始都在按下点。
    m_dragSelecting = true;
    m_dragIndex = QPersistentModelIndex(index);
    m_dragAnchor = cursorPos;
    m_pressedFileCardIndex = QPersistentModelIndex();
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
        m_pressedFileCardIndex = QPersistentModelIndex();
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

    if (delegate->fileCardContains(option, index, event->pos())) {
        m_dragSelecting = false;
        m_dragIndex = QPersistentModelIndex();
        m_dragAnchor = -1;
        m_pressedFileCardIndex = QPersistentModelIndex();
        emit fileMessageActivated(index);
        event->accept();
        return;
    }

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
    if (m_pressedFileCardIndex.isValid() &&
        (event->buttons() & Qt::LeftButton))
    {
        updateHoverCursor(event->pos());
        event->accept();
        return;
    }

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
    if (event->button() == Qt::LeftButton && m_pressedFileCardIndex.isValid()) {
        MessageDelegate *delegate = messageDelegate();
        const QModelIndex index(m_pressedFileCardIndex);
        bool shouldActivate = false;
        if (delegate && index.isValid()) {
            QStyleOptionViewItem option;
            initViewItemOption(&option);
            option.rect = visualRect(index);
            option.widget = this;
            shouldActivate = delegate->fileCardContains(option,
                                                        index,
                                                        event->pos());
        }

        m_pressedFileCardIndex = QPersistentModelIndex();
        updateHoverCursor(event->pos());
        if (shouldActivate) {
            emit fileMessageActivated(index);
            event->accept();
            return;
        }
    }

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
    QAction *copyAction = nullptr;
    QAction *downloadAction = nullptr;
    QAction *downloadToAction = nullptr;
    QAction *openFolderAction = nullptr;
    QAction *openFileAction = nullptr;

    const bool hasSelectedText = hasSelectedTextOnIndex(index);
    if (isFileMessageIndex(index))
    {
        const bool hasLocalFile = hasDownloadedLocalFile(index);
        const MessageTransferState transferState = static_cast<MessageTransferState>(
            index.data(MessageModel::TransferStateRole).toInt());
        const bool transferBusy =
            transferState == MessageTransferState::Uploading ||
            transferState == MessageTransferState::Sending ||
            transferState == MessageTransferState::Downloading;

        downloadAction = menu.addAction(hasLocalFile
                                            ? QStringLiteral("重新下载到默认位置")
                                            : QStringLiteral("下载到默认位置"));
        downloadAction->setEnabled(!transferBusy);
        downloadToAction = menu.addAction(QStringLiteral("下载到..."));
        downloadToAction->setEnabled(!transferBusy);
        menu.addSeparator();
        openFolderAction = menu.addAction(QStringLiteral("打开所在文件夹"));
        openFolderAction->setEnabled(hasLocalFile);
        openFileAction = menu.addAction(QStringLiteral("打开文件"));
        openFileAction->setEnabled(hasLocalFile);

        if (hasSelectedText || !index.data(MessageModel::TextRole).toString().trimmed().isEmpty())
        {
            menu.addSeparator();
            copyAction = menu.addAction(
                hasSelectedText ? QStringLiteral("复制选中文本")
                                : QStringLiteral("复制附言"));
        }
    }
    else
    {
        // 仅当“右键所在行 == 当前文本选区所在行”时显示“复制选中文本”，
        // 避免用户右键其它消息时误以为会复制该行子串。
        copyAction = menu.addAction(hasSelectedText ? QStringLiteral("复制选中文本")
                                                    : QStringLiteral("复制消息"));
    }

    const QAction *chosen = menu.exec(viewport()->mapToGlobal(pos));
    if (!chosen) {
        return;
    }

    if (chosen == downloadAction) {
        emit fileMessageDownloadRequested(index);
        return;
    }

    if (chosen == downloadToAction) {
        const QString suggestedPath = suggestedDownloadPath(index);
        const QString targetPath = QFileDialog::getSaveFileName(
            this,
            QStringLiteral("选择文件下载位置"),
            suggestedPath);
        if (!targetPath.trimmed().isEmpty()) {
            emit fileMessageDownloadToRequested(index, targetPath);
        }
        return;
    }

    if (chosen == openFolderAction) {
        emit fileMessageOpenFolderRequested(index);
        return;
    }

    if (chosen == openFileAction) {
        emit fileMessageOpenRequested(index);
        return;
    }

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

    if (delegate->fileCardContains(option, index, viewPos)) {
        viewport()->setCursor(Qt::PointingHandCursor);
        return;
    }

    const int cursorPos = delegate->textPositionAt(option, index, viewPos, false);
    if (cursorPos >= 0) {
        // 仅正文区域显示 IBeam；作者/时间/气泡留白保持默认箭头。
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        resetHoverCursor();
    }
}

bool MessageListView::isFileMessageIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    return index.data(MessageModel::MessageTypeRole).toInt() ==
           static_cast<int>(MessageType::File);
}

bool MessageListView::hasDownloadedLocalFile(const QModelIndex &index) const
{
    if (!isFileMessageIndex(index)) {
        return false;
    }

    const QString localPath =
        index.data(MessageModel::FileLocalPathRole).toString().trimmed();
    return !localPath.isEmpty() && QFileInfo::exists(localPath);
}

QString MessageListView::suggestedDownloadPath(const QModelIndex &index) const
{
    if (!isFileMessageIndex(index)) {
        return QString();
    }

    const QString localPath =
        index.data(MessageModel::FileLocalPathRole).toString().trimmed();
    if (!localPath.isEmpty()) {
        return QFileInfo(localPath).absoluteFilePath();
    }

    QString fileName =
        index.data(MessageModel::FileNameRole).toString().trimmed();
    if (fileName.isEmpty()) {
        fileName = QStringLiteral("attachment.bin");
    }

    QString downloadRoot =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (downloadRoot.trimmed().isEmpty()) {
        downloadRoot =
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }
    if (downloadRoot.trimmed().isEmpty()) {
        return fileName;
    }

    return QFileInfo(QDir(downloadRoot).filePath(fileName)).absoluteFilePath();
}

void MessageListView::resetHoverCursor()
{
    if (viewport()) {
        viewport()->unsetCursor();
    }
}
