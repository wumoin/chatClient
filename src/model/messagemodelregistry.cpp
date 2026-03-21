#include "messagemodelregistry.h"

MessageModelRegistry::MessageModelRegistry(QObject *parent)
    : QObject(parent)
{
}

MessageModel *MessageModelRegistry::ensureModel(const QString &conversationId)
{
    if (conversationId.isEmpty()) {
        return nullptr;
    }

    // 每个 conversation_id 只持有一个 MessageModel，切换会话时只是切换绑定关系，
    // 不会重新创建整套消息视图对象。
    if (MessageModel *existing = m_models.value(conversationId, nullptr)) {
        return existing;
    }

    // registry 作为 QObject 父对象统一托管各会话 model 生命周期。
    MessageModel *created = new MessageModel(this);
    m_models.insert(conversationId, created);
    return created;
}

MessageModel *MessageModelRegistry::model(const QString &conversationId) const
{
    return m_models.value(conversationId, nullptr);
}

bool MessageModelRegistry::hasModel(const QString &conversationId) const
{
    return m_models.contains(conversationId);
}

QStringList MessageModelRegistry::conversationIds() const
{
    return m_models.keys();
}

void MessageModelRegistry::replaceMessageItems(
    const QString &conversationId,
    const QVector<MessageItem> &items)
{
    // HTTP 冷启动同步时，会把某个会话的一页历史消息整体替换进来。
    if (MessageModel *target = ensureModel(conversationId)) {
        target->setMessageItems(items);
    }
}

void MessageModelRegistry::upsertMessageItem(const QString &conversationId,
                                             const MessageItem &item)
{
    // WS ack / new 都可能把同一条正式消息再写回来，使用 upsert 可以避免重复插入。
    if (MessageModel *target = ensureModel(conversationId)) {
        target->upsertMessageItem(item);
    }
}

void MessageModelRegistry::clearConversation(const QString &conversationId)
{
    if (MessageModel *target = model(conversationId)) {
        target->clear();
    }
}

void MessageModelRegistry::clearAll()
{
    // 清空的是各会话里的消息内容，而不是销毁 MessageModel 实例本身，
    // 这样聊天窗口切换绑定时不需要重新分配对象。
    const auto models = m_models.values();
    for (MessageModel *model : models) {
        if (model) {
            model->clear();
        }
    }
}

void MessageModelRegistry::addTextMessage(const QString &conversationId,
                                          const QString &author,
                                          const QString &text,
                                          const QString &timeText,
                                          bool fromSelf)
{
    if (MessageModel *target = ensureModel(conversationId)) {
        target->addTextMessage(author, text, timeText, fromSelf);
    }
}

void MessageModelRegistry::addImageMessage(const QString &conversationId,
                                           const QString &author,
                                           const QString &timeText,
                                           bool fromSelf,
                                           const QString &localPath,
                                           const QString &remoteUrl,
                                           int width,
                                           int height,
                                           const QString &caption)
{
    // 图片消息既可以来自本地临时预览，也可以来自后续真正落库后的正式消息；
    // registry 只负责把它路由到正确的会话 model。
    if (MessageModel *target = ensureModel(conversationId)) {
        target->addImageMessage(author,
                                timeText,
                                fromSelf,
                                localPath,
                                remoteUrl,
                                width,
                                height,
                                caption);
    }
}

void MessageModelRegistry::addFileMessage(const QString &conversationId,
                                          const QString &author,
                                          const QString &timeText,
                                          bool fromSelf,
                                          const QString &fileName,
                                          const QString &localPath,
                                          const QString &remoteUrl,
                                          qint64 sizeBytes,
                                          const QString &caption)
{
    // 文件消息与图片消息同理，这一层不区分来源，只负责把消息挂到目标会话上。
    if (MessageModel *target = ensureModel(conversationId)) {
        target->addFileMessage(author,
                               timeText,
                               fromSelf,
                               fileName,
                               localPath,
                               remoteUrl,
                               sizeBytes,
                               caption);
    }
}

bool MessageModelRegistry::updateImagePayload(const QString &conversationId,
                                              const MessageItem &identity,
                                              const QString &localPath,
                                              int width,
                                              int height)
{
    if (MessageModel *target = model(conversationId))
    {
        return target->updateImagePayload(identity, localPath, width, height);
    }

    return false;
}

bool MessageModelRegistry::updateFilePayload(const QString &conversationId,
                                             const MessageItem &identity,
                                             const QString &localPath,
                                             const QString &remoteUrl,
                                             const QString &fileName,
                                             const QString &mimeType,
                                             qint64 sizeBytes,
                                             const QString &attachmentId)
{
    if (MessageModel *target = model(conversationId))
    {
        return target->updateFilePayload(identity,
                                         localPath,
                                         remoteUrl,
                                         fileName,
                                         mimeType,
                                         sizeBytes,
                                         attachmentId);
    }

    return false;
}

bool MessageModelRegistry::updateTransferState(const QString &conversationId,
                                               const MessageItem &identity,
                                               MessageTransferState transferState,
                                               int transferProgress,
                                               const QString &statusText)
{
    if (MessageModel *target = model(conversationId))
    {
        return target->updateTransferState(identity,
                                           transferState,
                                           transferProgress,
                                           statusText);
    }

    return false;
}
