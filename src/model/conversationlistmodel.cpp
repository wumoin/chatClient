#include "model/conversationlistmodel.h"

namespace chatclient::model {
namespace {

// 预先把 conversation_id 映射成行号，后续按会话更新摘要时就不需要每次线性扫描整张列表。
QHash<QString, int> buildRowIndex(
    const QVector<chatclient::dto::conversation::ConversationSummaryDto>
        &conversations)
{
    QHash<QString, int> rowIndex;
    rowIndex.reserve(conversations.size());
    for (int index = 0; index < conversations.size(); ++index) {
        rowIndex.insert(conversations.at(index).conversationId, index);
    }
    return rowIndex;
}

}  // namespace

ConversationListModel::ConversationListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int ConversationListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_conversations.size();
}

QVariant ConversationListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= m_conversations.size()) {
        return QVariant();
    }

    const auto &conversation = m_conversations.at(index.row());
    // 这个 model 只暴露“会话摘要”信息。真正的历史消息内容不放在这里，
    // 而是由 MessageModelRegistry 按 conversation_id 单独维护。
    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return displayTitle(conversation);
    case ConversationIdRole:
        return conversation.conversationId;
    case ConversationTypeRole:
        return conversation.conversationType;
    case PeerUserIdRole:
        return conversation.peerUser.userId;
    case AccountRole:
        return conversation.peerUser.account;
    case AvatarUrlRole:
        return conversation.peerUser.avatarUrl;
    case LastMessagePreviewRole:
        return conversation.lastMessagePreview;
    case LastMessageSeqRole:
        return conversation.lastMessageSeq;
    case LastReadSeqRole:
        return conversation.lastReadSeq;
    case UnreadCountRole:
        return conversation.unreadCount;
    case LastMessageAtMsRole:
        return conversation.hasLastMessageAtMs ? conversation.lastMessageAtMs
                                               : 0;
    case CreatedAtMsRole:
        return conversation.createdAtMs;
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> ConversationListModel::roleNames() const
{
    return {
        {ConversationIdRole, "conversationId"},
        {ConversationTypeRole, "conversationType"},
        {PeerUserIdRole, "peerUserId"},
        {TitleRole, "title"},
        {AccountRole, "account"},
        {AvatarUrlRole, "avatarUrl"},
        {LastMessagePreviewRole, "lastMessagePreview"},
        {LastMessageSeqRole, "lastMessageSeq"},
        {LastReadSeqRole, "lastReadSeq"},
        {UnreadCountRole, "unreadCount"},
        {LastMessageAtMsRole, "lastMessageAtMs"},
        {CreatedAtMsRole, "createdAtMs"},
    };
}

void ConversationListModel::setConversations(
    const QVector<chatclient::dto::conversation::ConversationSummaryDto>
        &conversations)
{
    // 启动阶段的 HTTP 快照会整体替换本地会话列表，因此这里直接 reset 更清晰。
    beginResetModel();
    m_conversations = conversations;
    m_rowByConversationId = buildRowIndex(m_conversations);
    endResetModel();
}

void ConversationListModel::upsertConversation(
    const chatclient::dto::conversation::ConversationSummaryDto &conversation)
{
    // 会话摘要的新增和更新统一走 upsert：HTTP 首次同步、WS 新建会话、
    // 以及收到新消息后最后一条消息预览变化，都会复用这条路径。
    const int existingRow =
        m_rowByConversationId.value(conversation.conversationId, -1);
    if (existingRow >= 0) {
        m_conversations[existingRow] = conversation;
        const QModelIndex changedIndex = index(existingRow, 0);
        emit dataChanged(changedIndex, changedIndex);
        return;
    }

    const int newRow = m_conversations.size();
    beginInsertRows(QModelIndex(), newRow, newRow);
    m_conversations.push_back(conversation);
    m_rowByConversationId.insert(conversation.conversationId, newRow);
    endInsertRows();
}

void ConversationListModel::clear()
{
    if (m_conversations.isEmpty()) {
        return;
    }

    beginResetModel();
    m_conversations.clear();
    m_rowByConversationId.clear();
    endResetModel();
}

bool ConversationListModel::hasConversation(const QString &conversationId) const
{
    return m_rowByConversationId.contains(conversationId);
}

bool ConversationListModel::conversationById(
    const QString &conversationId,
    chatclient::dto::conversation::ConversationSummaryDto *out) const
{
    const int row = m_rowByConversationId.value(conversationId, -1);
    if (row < 0 || row >= m_conversations.size()) {
        return false;
    }

    if (out) {
        *out = m_conversations.at(row);
    }
    return true;
}

QString ConversationListModel::displayTitle(
    const chatclient::dto::conversation::ConversationSummaryDto &conversation)
{
    // 列表标题按“昵称 -> 账号 -> conversation_id”回退，保证任何情况下都有稳定文案。
    if (!conversation.peerUser.nickname.trimmed().isEmpty()) {
        return conversation.peerUser.nickname.trimmed();
    }

    if (!conversation.peerUser.account.trimmed().isEmpty()) {
        return conversation.peerUser.account.trimmed();
    }

    return conversation.conversationId;
}

}  // namespace chatclient::model
