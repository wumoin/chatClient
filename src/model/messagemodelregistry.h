#pragma once

#include "messagemodel.h"

#include <QObject>
#include <QHash>
#include <QString>
#include <QStringList>

// 多会话消息模型注册表：
// - 以 conversationId 为键管理多个 MessageModel；
// - 对外提供按会话写入文本/图片/文件消息的统一接口；
// - 视图层只需拿到目标会话的 model 并绑定即可。
//
// 它相当于“当前客户端已打开过的会话消息容器表”，生命周期通常长于单次会话切换。
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
     * @brief 用新的完整消息集合替换指定会话模型内容。
     * @param conversationId 会话唯一标识。
     * @param items 新的完整消息集合。
     */
    void replaceMessageItems(const QString &conversationId,
                             const QVector<MessageItem> &items);
    /**
     * @brief 向指定会话插入或更新一条消息。
     * @param conversationId 会话唯一标识。
     * @param item 完整消息项。
     */
    void upsertMessageItem(const QString &conversationId,
                           const MessageItem &item);
    /**
     * @brief 清空指定会话当前已加载的消息集合。
     * @param conversationId 会话唯一标识。
     */
    void clearConversation(const QString &conversationId);
    /**
     * @brief 清空当前所有已注册会话模型中的消息集合。
     */
    void clearAll();

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
    /**
     * @brief 只更新指定会话中某条图片消息的本地缓存路径与尺寸。
     * @param conversationId 目标会话 ID。
     * @param identity 用于定位目标消息的身份键。
     * @param localPath 已下载到本地的图片缓存路径；为空时不更新路径。
     * @param width 已知图片宽度；小于等于 0 时表示不更新。
     * @param height 已知图片高度；小于等于 0 时表示不更新。
     * @return true 表示命中并刷新了对应消息；false 表示目标消息还不在该会话 model 中。
     */
    bool updateImagePayload(const QString &conversationId,
                            const MessageItem &identity,
                            const QString &localPath,
                            int width = -1,
                            int height = -1);
    /**
     * @brief 只更新指定会话中某条文件消息的文件预览字段。
     * @param conversationId 目标会话 ID。
     * @param identity 用于定位目标消息的身份键。
     * @param localPath 已下载到本地的文件路径；为空时不更新。
     * @param remoteUrl 远端下载地址；为空时不更新。
     * @param fileName 文件展示名；为空时不更新。
     * @param mimeType 文件 MIME 类型；为空时不更新。
     * @param sizeBytes 文件大小；小于 0 时表示不更新。
     * @param attachmentId 正式附件 ID；为空时不更新。
     * @return true 表示命中并刷新了对应文件消息；false 表示目标消息还不在该会话 model 中。
     */
    bool updateFilePayload(const QString &conversationId,
                           const MessageItem &identity,
                           const QString &localPath = QString(),
                           const QString &remoteUrl = QString(),
                           const QString &fileName = QString(),
                           const QString &mimeType = QString(),
                           qint64 sizeBytes = -1,
                           const QString &attachmentId = QString());
    /**
     * @brief 只更新指定会话中某条消息的传输状态字段。
     * @param conversationId 目标会话 ID。
     * @param identity 用于定位目标消息的身份键。
     * @param transferState 新的传输状态。
     * @param transferProgress 当前进度百分比；未知时可传 -1。
     * @param statusText 直接给 delegate 展示的状态文案。
     * @return true 表示命中并刷新了对应消息；false 表示目标消息还不在该会话 model 中。
     */
    bool updateTransferState(const QString &conversationId,
                             const MessageItem &identity,
                             MessageTransferState transferState,
                             int transferProgress,
                             const QString &statusText);

private:
    QHash<QString, MessageModel *> m_models;
};
