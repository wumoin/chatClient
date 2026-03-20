#pragma once

#include "dto/conversation_dto.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace chatclient::model {

/**
 * @brief 会话摘要列表展示模型。
 *
 * 当前只负责承载“会话列表该显示什么”：
 * - 不发 HTTP；
 * - 不处理 WebSocket；
 * - 不关心历史消息怎么拉取。
 */
class ConversationListModel : public QAbstractListModel
{
    Q_OBJECT

  public:
    enum ConversationRole
    {
        ConversationIdRole = Qt::UserRole + 1,
        ConversationTypeRole,
        PeerUserIdRole,
        TitleRole,
        AccountRole,
        AvatarUrlRole,
        LastMessagePreviewRole,
        LastMessageSeqRole,
        LastReadSeqRole,
        UnreadCountRole,
        LastMessageAtMsRole,
        CreatedAtMsRole,
    };

    /**
     * @brief 构造会话摘要列表模型。
     * @param parent 父级 QObject，可为空。
     */
    explicit ConversationListModel(QObject *parent = nullptr);

    /**
     * @brief 返回当前会话摘要条目数。
     * @param parent 父索引，仅支持顶层时应为无效索引。
     * @return 会话摘要总数。
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    /**
     * @brief 按角色读取指定会话摘要的数据。
     * @param index 目标行索引。
     * @param role 数据角色。
     * @return 对应角色的数据，若无效则返回空 QVariant。
     */
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    /**
     * @brief 返回模型角色到名称的映射。
     * @return 角色映射表。
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 用新的完整会话集合整体替换模型内容。
     * @param conversations 新的会话摘要集合。
     */
    void setConversations(
        const QVector<chatclient::dto::conversation::ConversationSummaryDto>
            &conversations);
    /**
     * @brief 插入或更新单条会话摘要。
     * @param conversation 目标会话摘要。
     */
    void upsertConversation(
        const chatclient::dto::conversation::ConversationSummaryDto
            &conversation);
    /**
     * @brief 清空全部会话摘要。
     */
    void clear();
    /**
     * @brief 判断某条会话摘要是否已存在。
     * @param conversationId 会话唯一标识。
     * @return true 表示已存在；false 表示不存在。
     */
    bool hasConversation(const QString &conversationId) const;
    /**
     * @brief 查询指定会话摘要。
     * @param conversationId 会话唯一标识。
     * @param out 成功时写入会话摘要。
     * @return true 表示查询成功；false 表示不存在。
     */
    bool conversationById(
        const QString &conversationId,
        chatclient::dto::conversation::ConversationSummaryDto *out) const;

  private:
    static QString displayTitle(
        const chatclient::dto::conversation::ConversationSummaryDto
            &conversation);

    QVector<chatclient::dto::conversation::ConversationSummaryDto>
        m_conversations;
    QHash<QString, int> m_rowByConversationId;
};

}  // namespace chatclient::model
