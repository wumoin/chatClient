#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>
#include <QtGlobal>

// 单条消息的数据结构。
// 这里保持“纯数据”设计，不耦合任何 QWidget，便于后续接入网络层或持久化层。
enum class MessageType {
    // 纯文本消息。
    Text = 0,
    // 图片消息（预留原图/缩略图地址与尺寸字段）。
    Image,
    // 文件消息（预留文件名/地址/大小字段）。
    File
};

// 图片消息扩展字段。
struct MessageImagePayload {
    // 本地缓存路径（可为空）。
    QString localPath;
    // 服务端 URL（可为空）。
    QString remoteUrl;
    // 图片尺寸（像素），未知时为 0。
    int width = 0;
    int height = 0;
};

// 文件消息扩展字段。
struct MessageFilePayload {
    // 文件显示名（例如 report.pdf）。
    QString fileName;
    // 本地路径（可为空）。
    QString localPath;
    // 服务端 URL（可为空）。
    QString remoteUrl;
    // 文件大小（字节），未知时为 -1。
    qint64 sizeBytes = -1;
};

struct MessageItem {
    // 发送者展示名，例如“我”“李华”。
    QString author;
    // 文本内容：
    // - 对 Text 消息：正文；
    // - 对 Image/File 消息：可作为说明文本或摘要。
    QString text;
    // UI 展示时间（当前版本直接存格式化后的字符串）。
    QString timeText;
    // true 表示我方消息，用于 delegate 决定左右对齐与配色。
    bool fromSelf = false;
    // 消息类型，决定 delegate 的渲染策略与交互能力。
    MessageType messageType = MessageType::Text;
    // 图片消息的扩展字段（非图片消息时可忽略）。
    MessageImagePayload image;
    // 文件消息的扩展字段（非文件消息时可忽略）。
    MessageFilePayload file;
};

// 消息列表模型：
// 1) 持有消息数组；
// 2) 通过 role 向视图/委托暴露消息字段；
// 3) 在插入消息时发送 beginInsertRows/endInsertRows 通知，驱动 QListView 增量刷新。
class MessageModel : public QAbstractListModel
{
    Q_OBJECT

public:
    // 自定义角色定义：
    // - 从 Qt::UserRole + 1 开始，避免和 Qt 内置角色冲突。
    // - delegate 通过这些 role 获取 author/type/text/time/fromSelf 及扩展字段。
    enum MessageRole {
        AuthorRole = Qt::UserRole + 1,
        TextRole,
        TimeRole,
        FromSelfRole,
        MessageTypeRole,
        ImageLocalPathRole,
        ImageRemoteUrlRole,
        ImageWidthRole,
        ImageHeightRole,
        FileNameRole,
        FileLocalPathRole,
        FileRemoteUrlRole,
        FileSizeBytesRole
    };

    explicit MessageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    // 追加任意类型消息到模型末尾。
    void addMessageItem(const MessageItem &item);
    // 文本消息接口：追加文本类型消息。
    void addTextMessage(const QString &author, const QString &text, const QString &timeText, bool fromSelf);
    // 便捷接口：追加图片消息（字段可按需留空）。
    void addImageMessage(const QString &author,
                         const QString &timeText,
                         bool fromSelf,
                         const QString &localPath = QString(),
                         const QString &remoteUrl = QString(),
                         int width = 0,
                         int height = 0,
                         const QString &caption = QString());
    // 便捷接口：追加文件消息（字段可按需留空）。
    void addFileMessage(const QString &author,
                        const QString &timeText,
                        bool fromSelf,
                        const QString &fileName = QString(),
                        const QString &localPath = QString(),
                        const QString &remoteUrl = QString(),
                        qint64 sizeBytes = -1,
                        const QString &caption = QString());

private:
    QVector<MessageItem> m_messages;
};
