#pragma once

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

class MessageModel;

// 多会话消息模型注册表：
// - 以 conversationId 为键管理多个 MessageModel；
// - 对外提供按会话写入文本/图片/文件消息的统一接口；
// - 视图层只需拿到目标会话的 model 并绑定即可。
class MessageModelRegistry : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造模型注册表，不预创建任何会话。
     * @param parent 父级 QObject，可为空。
     */
    explicit MessageModelRegistry(QObject *parent = nullptr);

    /**
     * @brief 确保指定会话的模型存在。
     * @param conversationId 会话唯一标识。
     * @return 已存在或新创建的消息模型；会话 id 为空时返回 nullptr。
     */
    MessageModel *ensureModel(const QString &conversationId);
    /**
     * @brief 查询指定会话的消息模型（不创建）。
     * @param conversationId 会话唯一标识。
     * @return 已存在模型指针；不存在时返回 nullptr。
     */
    MessageModel *model(const QString &conversationId) const;
    /**
     * @brief 判断指定会话模型是否已创建。
     * @param conversationId 会话唯一标识。
     * @return true 表示已存在；false 表示未创建。
     */
    bool hasModel(const QString &conversationId) const;
    /**
     * @brief 返回当前所有已注册会话 id。
     * @return 会话 id 列表。
     */
    QStringList conversationIds() const;

    /**
     * @brief 向指定会话追加文本消息。
     * @param conversationId 会话唯一标识。
     * @param author 发送者名称。
     * @param text 正文内容。
     * @param timeText 展示时间文本。
     * @param fromSelf 是否由当前用户发送。
     */
    void addTextMessage(const QString &conversationId,
                        const QString &author,
                        const QString &text,
                        const QString &timeText,
                        bool fromSelf);
    /**
     * @brief 向指定会话追加图片消息。
     * @param conversationId 会话唯一标识。
     * @param author 发送者名称。
     * @param timeText 展示时间文本。
     * @param fromSelf 是否由当前用户发送。
     * @param localPath 本地图片路径。
     * @param remoteUrl 远端图片地址。
     * @param width 图片宽度（像素）。
     * @param height 图片高度（像素）。
     * @param caption 图片说明文本。
     */
    void addImageMessage(const QString &conversationId,
                         const QString &author,
                         const QString &timeText,
                         bool fromSelf,
                         const QString &localPath = QString(),
                         const QString &remoteUrl = QString(),
                         int width = 0,
                         int height = 0,
                         const QString &caption = QString());
    /**
     * @brief 向指定会话追加文件消息。
     * @param conversationId 会话唯一标识。
     * @param author 发送者名称。
     * @param timeText 展示时间文本。
     * @param fromSelf 是否由当前用户发送。
     * @param fileName 文件名。
     * @param localPath 本地文件路径。
     * @param remoteUrl 远端文件地址。
     * @param sizeBytes 文件大小（字节）。
     * @param caption 文件说明文本。
     */
    void addFileMessage(const QString &conversationId,
                        const QString &author,
                        const QString &timeText,
                        bool fromSelf,
                        const QString &fileName = QString(),
                        const QString &localPath = QString(),
                        const QString &remoteUrl = QString(),
                        qint64 sizeBytes = -1,
                        const QString &caption = QString());

private:
    QHash<QString, MessageModel *> m_models;
};
