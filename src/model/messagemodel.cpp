#include "messagemodel.h"

// MessageModel 是右侧单个会话消息列表的直接数据源。
// 这里不处理网络、缓存或 WS，只维护当前这份消息列表如何以
// QAbstractListModel 角色暴露给 delegate / view。
namespace {
int messageTypeToInt(MessageType type)
{
    return static_cast<int>(type);
}

QString fallbackDisplayText(const MessageItem &item)
{
    if (!item.text.isEmpty()) {
        return item.text;
    }

    switch (item.messageType) {
    case MessageType::Image:
        return QStringLiteral("[图片消息]");
    case MessageType::File:
        if (!item.file.fileName.isEmpty()) {
            return QStringLiteral("[文件] %1").arg(item.file.fileName);
        }
        return QStringLiteral("[文件消息]");
    case MessageType::Text:
    default:
        return QString();
    }
}
} // namespace

MessageModel::MessageModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    // list model 仅支持“顶层行”，因此 parent 有效时返回 0。
    if (parent.isValid()) {
        return 0;
    }
    return m_messages.size();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    // 越界保护：视图可能在布局阶段用到临时 index。
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size()) {
        return QVariant();
    }

    const MessageItem &item = m_messages[index.row()];
    switch (role) {
    // 兼容默认显示角色：若 text 为空则返回类型占位文本，避免 image/file 消息在当前 delegate 下空白。
    case Qt::DisplayRole:
        return fallbackDisplayText(item);
    case TextRole:
        return item.text;
    case AuthorRole:
        return item.author;
    case TimeRole:
        return item.timeText;
    case FromSelfRole:
        return item.fromSelf;
    case MessageTypeRole:
        return messageTypeToInt(item.messageType);
    case MessageIdRole:
        return item.messageId;
    case ClientMessageIdRole:
        return item.clientMessageId;
    case SeqRole:
        return item.seq;
    case ImageLocalPathRole:
        return item.image.localPath;
    case ImageRemoteUrlRole:
        return item.image.remoteUrl;
    case ImageWidthRole:
        return item.image.width;
    case ImageHeightRole:
        return item.image.height;
    case FileNameRole:
        return item.file.fileName;
    case FileLocalPathRole:
        return item.file.localPath;
    case FileRemoteUrlRole:
        return item.file.remoteUrl;
    case FileSizeBytesRole:
        return item.file.sizeBytes;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> MessageModel::roleNames() const
{
    // role 名称主要用于调试和 QML 场景（在纯 QWidget + delegate 中也建议保留）。
    return {
        {AuthorRole, "author"},
        {TextRole, "text"},
        {TimeRole, "timeText"},
        {FromSelfRole, "fromSelf"},
        {MessageTypeRole, "messageType"},
        {MessageIdRole, "messageId"},
        {ClientMessageIdRole, "clientMessageId"},
        {SeqRole, "seq"},
        {ImageLocalPathRole, "imageLocalPath"},
        {ImageRemoteUrlRole, "imageRemoteUrl"},
        {ImageWidthRole, "imageWidth"},
        {ImageHeightRole, "imageHeight"},
        {FileNameRole, "fileName"},
        {FileLocalPathRole, "fileLocalPath"},
        {FileRemoteUrlRole, "fileRemoteUrl"},
        {FileSizeBytesRole, "fileSizeBytes"},
    };
}

void MessageModel::addMessageItem(const MessageItem &item)
{
    const int row = m_messages.size();
    // begin/endInsertRows 是 model-view 协议关键点：
    // 必须在容器修改前后成对调用，QListView 才能做增量刷新与滚动位置维护。
    beginInsertRows(QModelIndex(), row, row);
    m_messages.push_back(item);
    endInsertRows();
}

void MessageModel::upsertMessageItem(const MessageItem &item)
{
    const int existingRow = findMessageRowByIdentity(item);
    if (existingRow >= 0)
    {
        m_messages[existingRow] = item;
        const QModelIndex changedIndex = index(existingRow, 0);
        emit dataChanged(changedIndex, changedIndex);
        return;
    }

    const int row = insertionRowForMessage(item);
    beginInsertRows(QModelIndex(), row, row);
    m_messages.insert(row, item);
    endInsertRows();
}

void MessageModel::addTextMessage(const QString &author, const QString &text, const QString &timeText, bool fromSelf)
{
    MessageItem item;
    item.author = author;
    item.text = text;
    item.timeText = timeText;
    item.fromSelf = fromSelf;
    item.messageType = MessageType::Text;
    addMessageItem(item);
}

void MessageModel::addImageMessage(const QString &author,
                                   const QString &timeText,
                                   bool fromSelf,
                                   const QString &localPath,
                                   const QString &remoteUrl,
                                   int width,
                                   int height,
                                   const QString &caption)
{
    MessageItem item;
    item.author = author;
    item.text = caption;
    item.timeText = timeText;
    item.fromSelf = fromSelf;
    item.messageType = MessageType::Image;
    item.image.localPath = localPath;
    item.image.remoteUrl = remoteUrl;
    item.image.width = width;
    item.image.height = height;
    addMessageItem(item);
}

void MessageModel::addFileMessage(const QString &author,
                                  const QString &timeText,
                                  bool fromSelf,
                                  const QString &fileName,
                                  const QString &localPath,
                                  const QString &remoteUrl,
                                  qint64 sizeBytes,
                                  const QString &caption)
{
    MessageItem item;
    item.author = author;
    item.text = caption;
    item.timeText = timeText;
    item.fromSelf = fromSelf;
    item.messageType = MessageType::File;
    item.file.fileName = fileName;
    item.file.localPath = localPath;
    item.file.remoteUrl = remoteUrl;
    item.file.sizeBytes = sizeBytes;
    addMessageItem(item);
}

void MessageModel::setMessageItems(const QVector<MessageItem> &items)
{
    beginResetModel();
    m_messages = items;
    endResetModel();
}

void MessageModel::clear()
{
    if (m_messages.isEmpty()) {
        return;
    }

    beginResetModel();
    m_messages.clear();
    endResetModel();
}

int MessageModel::findMessageRowByIdentity(const MessageItem &item) const
{
    if (!item.messageId.trimmed().isEmpty())
    {
        for (int row = 0; row < m_messages.size(); ++row)
        {
            if (m_messages.at(row).messageId == item.messageId)
            {
                return row;
            }
        }
    }

    if (!item.clientMessageId.trimmed().isEmpty())
    {
        for (int row = 0; row < m_messages.size(); ++row)
        {
            if (m_messages.at(row).clientMessageId == item.clientMessageId)
            {
                return row;
            }
        }
    }

    if (item.seq > 0)
    {
        for (int row = 0; row < m_messages.size(); ++row)
        {
            if (m_messages.at(row).seq == item.seq)
            {
                return row;
            }
        }
    }

    return -1;
}

int MessageModel::insertionRowForMessage(const MessageItem &item) const
{
    if (item.seq <= 0)
    {
        return m_messages.size();
    }

    for (int row = 0; row < m_messages.size(); ++row)
    {
        const qint64 existingSeq = m_messages.at(row).seq;
        if (existingSeq > 0 && existingSeq > item.seq)
        {
            return row;
        }
    }

    return m_messages.size();
}
