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

    if (MessageModel *existing = m_models.value(conversationId, nullptr)) {
        return existing;
    }

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
    if (MessageModel *target = ensureModel(conversationId)) {
        target->setMessageItems(items);
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
