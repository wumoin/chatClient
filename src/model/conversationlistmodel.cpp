#include "model/conversationlistmodel.h"

namespace chatclient::model {
namespace {

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
    beginResetModel();
    m_conversations = conversations;
    m_rowByConversationId = buildRowIndex(m_conversations);
    endResetModel();
}

void ConversationListModel::upsertConversation(
    const chatclient::dto::conversation::ConversationSummaryDto &conversation)
{
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
    if (!conversation.peerUser.nickname.trimmed().isEmpty()) {
        return conversation.peerUser.nickname.trimmed();
    }

    if (!conversation.peerUser.account.trimmed().isEmpty()) {
        return conversation.peerUser.account.trimmed();
    }

    return conversation.conversationId;
}

}  // namespace chatclient::model
