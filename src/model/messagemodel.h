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

// 消息传输状态。
//
// 当前统一服务于“带附件的消息 UI 传输态”：
// - 图片 / 文件上传临时附件时显示 Uploading；
// - HTTP 上传完成、等待 WS 业务确认时显示 Sending；
// - 文件消息主动下载到本地时显示 Downloading；
// - 任一阶段失败时显示 Failed；
// - 已经进入稳定态后回到 None。
enum class MessageTransferState {
    // 当前没有额外传输态，视为正式稳定消息。
    None = 0,
    // 正在通过 HTTP 上传临时附件。
    Uploading,
    // 上传已完成，正在等待 WS 业务确认或正式广播。
    Sending,
    // 正在把正式附件下载到本地文件系统。
    Downloading,
    // 上传、下载或发送流程失败。
    Failed
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
    // 正式附件 ID；若当前消息还没带出该字段，可为空。
    QString attachmentId;
    // 文件显示名（例如 report.pdf）。
    QString fileName;
    // 本地路径（可为空）。
    QString localPath;
    // 服务端 URL（可为空）。
    QString remoteUrl;
    // 文件 MIME 类型（例如 application/pdf）；未知时为空。
    QString mimeType;
    // 文件大小（字节），未知时为 -1。
    qint64 sizeBytes = -1;
};

struct MessageItem {
    // 当前消息所属会话 ID。
    QString conversationId;
    // 服务端正式消息 ID。
    QString messageId;
    // 客户端本地消息 ID，用于 ack/new 去重。
    QString clientMessageId;
    // 会话内顺序号。
    qint64 seq = 0;
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
    // 当前消息的传输状态；正式稳定消息通常为 None。
    MessageTransferState transferState = MessageTransferState::None;
    // 当前传输进度百分比，取值约定为：
    // - [0, 100]：已知进度；
    // - -1：当前暂无可用百分比（例如只知道“发送中”）。
    int transferProgress = -1;
    // 直接提供给 delegate 展示的状态文案，例如“上传中 42%”“发送中”“上传失败”。
    QString transferStatusText;
};

// 消息列表模型：
// 1) 持有“单个会话当前已加载消息”的数组；
// 2) 通过 role 向视图/委托暴露消息字段；
// 3) 在插入消息时发送 beginInsertRows/endInsertRows 通知，驱动 QListView 增量刷新。
//
// 这里不做网络请求，也不维护多会话索引；多会话路由由 MessageModelRegistry 负责。
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
        MessageIdRole,
        ClientMessageIdRole,
        SeqRole,
        ImageLocalPathRole,
        ImageRemoteUrlRole,
        ImageWidthRole,
        ImageHeightRole,
        FileAttachmentIdRole,
        FileNameRole,
        FileLocalPathRole,
        FileRemoteUrlRole,
        FileMimeTypeRole,
        FileSizeBytesRole,
        TransferStateRole,
        TransferProgressRole,
        TransferStatusTextRole
    };

    /**
     * @brief 构造消息模型并初始化空数据集。
     * @param parent 父级 QObject，可为空。
     */
    explicit MessageModel(QObject *parent = nullptr);

    /**
     * @brief 返回当前消息总行数。
     * @param parent 父索引，仅支持顶层时应为无效索引。
     * @return 顶层消息行数。
     */
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    /**
     * @brief 按角色读取指定消息行的数据。
     * @param index 目标行索引。
     * @param role 数据角色。
     * @return 对应角色的数据，若无效则返回空 QVariant。
     */
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    /**
     * @brief 返回角色编号到角色名的映射。
     * @return 角色映射表。
     */
    QHash<int, QByteArray> roleNames() const override;

    /**
     * @brief 追加任意类型消息项到模型末尾。
     * @param item 待追加的完整消息结构。
     */
    void addMessageItem(const MessageItem &item);
    /**
     * @brief 插入或更新一条消息，避免 ack/new 重复插入。
     * @param item 待写入的消息项。
     */
    void upsertMessageItem(const MessageItem &item);
    /**
     * @brief 追加文本消息。
     * @param author 发送者名称。
     * @param text 正文内容。
     * @param timeText 展示时间文本。
     * @param fromSelf 是否由当前用户发送。
     */
    void addTextMessage(const QString &author, const QString &text, const QString &timeText, bool fromSelf);
    /**
     * @brief 追加图片消息。
     * @param author 发送者名称。
     * @param timeText 展示时间文本。
     * @param fromSelf 是否由当前用户发送。
     * @param localPath 本地图片路径。
     * @param remoteUrl 远端图片地址。
     * @param width 图片宽度（像素）。
     * @param height 图片高度（像素）。
     * @param caption 图片说明文本。
     */
    void addImageMessage(const QString &author,
                         const QString &timeText,
                         bool fromSelf,
                         const QString &localPath = QString(),
                         const QString &remoteUrl = QString(),
                         int width = 0,
                         int height = 0,
                         const QString &caption = QString());
    /**
     * @brief 追加文件消息。
     * @param author 发送者名称。
     * @param timeText 展示时间文本。
     * @param fromSelf 是否由当前用户发送。
     * @param fileName 文件名。
     * @param localPath 本地文件路径。
     * @param remoteUrl 远端文件地址。
     * @param sizeBytes 文件大小（字节）。
     * @param caption 文件说明文本。
     */
    void addFileMessage(const QString &author,
                        const QString &timeText,
                        bool fromSelf,
                        const QString &fileName = QString(),
                        const QString &localPath = QString(),
                        const QString &remoteUrl = QString(),
                        qint64 sizeBytes = -1,
                        const QString &caption = QString());
    /**
     * @brief 用新的消息列表整体替换当前模型内容。
     * @param items 新的完整消息集合。
     */
    void setMessageItems(const QVector<MessageItem> &items);
    /**
     * @brief 清空当前模型中的全部消息。
     */
    void clear();
    /**
     * @brief 只更新某条图片消息的本地缓存路径与尺寸信息。
     *
     * 这个接口用于“消息本体已经进入 model，但图片文件稍后才准备好”的场景，
     * 例如：
     * 1. HTTP 拉回来的历史图片消息；
     * 2. `ws.new + route=message.created` 推送进来的正式图片消息。
     *
     * 它不会覆盖作者、正文、时间和传输状态等其它字段，只刷新图片展示真正需要的
     * `localPath / width / height`，避免为了回填缩略图又整条消息重新 upsert 一遍。
     *
     * @param identity 用于定位目标消息的身份键；通常带 `messageId / clientMessageId / seq` 之一即可。
     * @param localPath 已下载到本地的图片缓存路径；为空时不更新路径。
     * @param width 已知图片宽度；小于等于 0 时表示不更新。
     * @param height 已知图片高度；小于等于 0 时表示不更新。
     * @return true 表示命中并更新了某条图片消息；false 表示未命中或没有实际变化。
     */
    bool updateImagePayload(const MessageItem &identity,
                            const QString &localPath,
                            int width = -1,
                            int height = -1);
    /**
     * @brief 只更新某条文件消息的本地路径 / 远端地址 / 文件元数据。
     *
     * 这个接口用于文件消息已经进入模型后，再局部回填以下场景：
     * 1. 用户把正式附件下载到了本地；
     * 2. 某次重试后拿到了更完整的文件名、MIME 或大小信息；
     * 3. 后续如果补“文件发送中占位消息”，也可以继续复用同一条局部刷新路径。
     *
     * 它不会覆盖作者、正文、时间等通用展示字段，只更新文件预览真正依赖的
     * `localPath / remoteUrl / fileName / mimeType / sizeBytes / attachmentId`。
     *
     * @param identity 用于定位目标消息的身份键；通常带 `messageId / clientMessageId / seq` 之一即可。
     * @param localPath 已下载到本地的文件路径；为空时不更新。
     * @param remoteUrl 远端下载地址；为空时不更新。
     * @param fileName 文件展示名；为空时不更新。
     * @param mimeType 文件 MIME 类型；为空时不更新。
     * @param sizeBytes 文件大小；小于 0 时表示不更新。
     * @param attachmentId 正式附件 ID；为空时不更新。
     * @return true 表示命中并刷新了对应文件消息；false 表示未命中或没有实际变化。
     */
    bool updateFilePayload(const MessageItem &identity,
                           const QString &localPath = QString(),
                           const QString &remoteUrl = QString(),
                           const QString &fileName = QString(),
                           const QString &mimeType = QString(),
                           qint64 sizeBytes = -1,
                           const QString &attachmentId = QString());
    /**
     * @brief 只更新某条消息的传输状态字段。
     *
     * 这个接口把“附件消息的动态状态”单独收口出来，避免每次下载 / 上传进度变更时
     * 都重新构造整条 `MessageItem` 再走一遍完整 upsert。
     *
     * @param identity 用于定位目标消息的身份键。
     * @param transferState 新的传输状态。
     * @param transferProgress 新的传输进度百分比；未知时可传 -1。
     * @param statusText 直接展示给 delegate 的状态文案。
     * @return true 表示命中并产生了实际变化；false 表示未命中或状态未变化。
     */
    bool updateTransferState(const MessageItem &identity,
                             MessageTransferState transferState,
                             int transferProgress,
                             const QString &statusText);

private:
    // 按“服务端 message_id -> 本地 client_message_id -> seq”的优先级查找同一条消息，
    // 以便把本地临时消息、ack 返回和后续正式广播收敛成一行。
    int findMessageRowByIdentity(const MessageItem &item) const;
    // seq 大于 0 的正式消息按 seq 升序插入；
    // seq 不可用的本地临时消息统一追加到末尾，等待后续 ack/upsert 修正。
    int insertionRowForMessage(const MessageItem &item) const;

    QVector<MessageItem> m_messages;
};
