#pragma once

#include <QJsonObject>
#include <QUrl>

#include <QString>

namespace chatclient::config {

// AppConfig 负责统一承接客户端运行配置。
// 当前只先收口两类信息：
// 1) 应用展示信息，例如窗口标题；
// 2) 服务端基础信息，例如 HTTP base URL、注册/登录路径和 WebSocket URL。
//
// 这样后续接入 QNetworkAccessManager / QWebSocket 时，就不需要在各个窗口里继续散落硬编码地址。
class AppConfig
{
public:
    /**
     * @brief 初始化全局客户端配置。
     * @param errorMessage 初始化失败时写入的错误消息，可为空。
     * @return true 表示配置加载成功；false 表示配置文件不存在或字段不合法。
     */
    static bool initialize(QString *errorMessage = nullptr);

    /**
     * @brief 读取已经初始化完成的全局配置实例。
     * @return 只读配置单例引用。
     */
    static const AppConfig &instance();

    /**
     * @brief 返回当前实际加载的配置文件路径。
     * @return 配置文件绝对路径字符串。
     */
    const QString &configFilePath() const;

    /**
     * @brief 返回应用展示名称。
     * @return 例如 `ChatClient` 这样的应用名字符串。
     */
    const QString &displayName() const;

    /**
     * @brief 返回登录窗口标题。
     * @return 登录窗口顶栏标题文本。
     */
    const QString &loginWindowTitle() const;

    /**
     * @brief 返回聊天窗口标题。
     * @return 聊天窗口顶栏标题文本。
     */
    const QString &chatWindowTitle() const;

    /**
     * @brief 返回 HTTP 服务基础地址。
     * @return 例如 `http://127.0.0.1:8848` 这样的基础 URL。
     */
    const QUrl &httpBaseUrl() const;

    /**
     * @brief 返回注册接口完整地址。
     * @return 解析 `base_url + register_path` 后得到的完整注册 URL。
     */
    QUrl registerUrl() const;

    /**
     * @brief 返回登录接口完整地址。
     * @return 解析 `base_url + login_path` 后得到的完整登录 URL。
     */
    QUrl loginUrl() const;

    /**
     * @brief 返回登出接口完整地址。
     * @return 解析 `base_url + logout_path` 后得到的完整登出 URL。
     */
    QUrl logoutUrl() const;

    /**
     * @brief 返回临时头像上传接口完整地址。
     * @return 解析 `base_url + avatar_temp_upload_path` 后得到的完整上传 URL。
     */
    QUrl avatarTempUploadUrl() const;

    /**
     * @brief 返回指定用户头像文件接口完整地址。
     * @param userId 目标用户 ID。
     * @return 将路径模板中的 `{user_id}` 替换后得到的完整头像 URL。
     */
    QUrl userAvatarUrl(const QString &userId) const;

    /**
     * @brief 返回按账号搜索用户接口完整地址。
     * @param account 要搜索的目标账号。
     * @return 自动附带 `account` 查询参数后的完整 URL。
     */
    QUrl userSearchUrl(const QString &account) const;

    /**
     * @brief 返回好友列表接口完整地址。
     * @return 解析 `base_url + friend_list_path` 后得到的完整 URL。
     */
    QUrl friendListUrl() const;

    /**
     * @brief 返回发送好友申请接口完整地址。
     * @return 解析 `base_url + friend_send_request_path` 后得到的完整 URL。
     */
    QUrl friendSendRequestUrl() const;

    /**
     * @brief 返回我发出的好友申请列表接口完整地址。
     * @return 解析 `base_url + friend_outgoing_requests_path` 后得到的完整 URL。
     */
    QUrl friendOutgoingRequestsUrl() const;

    /**
     * @brief 返回我收到的好友申请列表接口完整地址。
     * @return 解析 `base_url + friend_incoming_requests_path` 后得到的完整 URL。
     */
    QUrl friendIncomingRequestsUrl() const;

    /**
     * @brief 返回同意好友申请接口完整地址。
     * @param requestId 要处理的好友申请 ID。
     * @return 将路径模板中的 `{request_id}` 替换后得到的完整 URL。
     */
    QUrl friendAcceptRequestUrl(const QString &requestId) const;

    /**
     * @brief 返回拒绝好友申请接口完整地址。
     * @param requestId 要处理的好友申请 ID。
     * @return 将路径模板中的 `{request_id}` 替换后得到的完整 URL。
     */
    QUrl friendRejectRequestUrl(const QString &requestId) const;

    /**
     * @brief 返回创建或复用私聊会话接口完整地址。
     * @return 解析 `base_url + conversation_private_path` 后得到的完整 URL。
     */
    QUrl conversationPrivateUrl() const;

    /**
     * @brief 返回当前用户会话列表接口完整地址。
     * @return 解析 `base_url + conversation_list_path` 后得到的完整 URL。
     */
    QUrl conversationListUrl() const;

    /**
     * @brief 返回指定会话详情接口完整地址。
     * @param conversationId 目标会话 ID。
     * @return 将路径模板中的 `{conversation_id}` 替换后得到的完整 URL。
     */
    QUrl conversationDetailUrl(const QString &conversationId) const;

    /**
     * @brief 返回指定会话历史消息接口完整地址。
     * @param conversationId 目标会话 ID。
     * @return 将路径模板中的 `{conversation_id}` 替换后得到的完整 URL。
     */
    QUrl conversationMessagesUrl(const QString &conversationId) const;

    /**
     * @brief 返回指定会话发送文本消息接口完整地址。
     * @param conversationId 目标会话 ID。
     * @return 将路径模板中的 `{conversation_id}` 替换后得到的完整 URL。
     */
    QUrl conversationSendTextUrl(const QString &conversationId) const;

    /**
     * @brief 返回 WebSocket 服务地址。
     * @return WebSocket 完整 URL。
     */
    const QUrl &webSocketUrl() const;

    /**
     * @brief 返回适合界面直接展示的 HTTP 服务地址文本。
     * @return HTTP base URL 的字符串形式。
     */
    QString httpBaseUrlText() const;

    /**
     * @brief 返回日志前缀中的应用名。
     * @return 日志模块统一使用的应用名。
     */
    const QString &logAppName() const;

    /**
     * @brief 返回日志最小输出级别文本。
     * @return 例如 `INFO`、`DEBUG` 这样的日志级别字符串。
     */
    const QString &logMinimumLevel() const;

    /**
     * @brief 判断是否启用控制台日志输出。
     * @return true 表示输出到控制台；false 表示关闭控制台输出。
     */
    bool isConsoleLogEnabled() const;

    /**
     * @brief 判断是否启用文件日志输出。
     * @return true 表示输出到日志文件；false 表示关闭文件输出。
     */
    bool isFileLogEnabled() const;

    /**
     * @brief 判断日志时间是否使用本地时区显示。
     * @return true 表示使用本地时间；false 表示使用 UTC 时间。
     */
    bool displayLocalTimeInLog() const;

    /**
     * @brief 返回日志文件目录配置原值。
     * @return `app.json` 中配置的日志目录文本。
     */
    const QString &logDirectory() const;

    /**
     * @brief 返回日志文件名配置原值。
     * @return `app.json` 中配置的日志文件名文本。
     */
    const QString &logFileName() const;

    /**
     * @brief 返回解析后的日志文件绝对路径。
     * @return 以配置文件目录为基准解析后的日志文件绝对路径。
     */
    QString resolvedLogFilePath() const;

private:
    /**
     * @brief 从默认配置文件路径加载配置内容。
     * @param errorMessage 加载失败时写入的错误消息，可为空。
     * @return true 表示加载成功；false 表示加载失败。
     */
    bool load(QString *errorMessage);

    /**
     * @brief 返回编译期注入的默认配置文件绝对路径。
     * @return 默认配置文件路径字符串。
     */
    static QString defaultConfigPath();

    /**
     * @brief 校验并读取根 JSON 对象中的字符串字段。
     * @param object 待读取的 JSON 对象。
     * @param key 字段名。
     * @param out 成功时写入的字符串值。
     * @param errorMessage 失败时写入的错误消息。
     * @return true 表示字段存在且类型正确；false 表示缺失或类型不匹配。
     */
    static bool readRequiredString(const QJsonObject &object,
                                   const QString &key,
                                   QString *out,
                                   QString *errorMessage);

    /**
     * @brief 读取可选字符串字段。
     * @param object 待读取的 JSON 对象。
     * @param key 字段名。
     * @param out 成功时写入的字符串值。
     * @param errorMessage 类型不匹配时写入的错误消息。
     * @return true 表示字段缺失或读取成功；false 表示类型不匹配或为空。
     */
    static bool readOptionalString(const QJsonObject &object,
                                   const QString &key,
                                   QString *out,
                                   QString *errorMessage);

    /**
     * @brief 读取可选布尔字段。
     * @param object 待读取的 JSON 对象。
     * @param key 字段名。
     * @param out 成功时写入的布尔值。
     * @param errorMessage 类型不匹配时写入的错误消息。
     * @return true 表示字段缺失或读取成功；false 表示类型不匹配。
     */
    static bool readOptionalBool(const QJsonObject &object,
                                 const QString &key,
                                 bool *out,
                                 QString *errorMessage);

    /**
     * @brief 校验日志级别文本是否受支持。
     * @param levelText 日志级别原始文本。
     * @return true 表示当前客户端日志模块支持该级别；false 表示不支持。
     */
    static bool isSupportedLogLevel(const QString &levelText);

    QString configFilePath_;
    QString displayName_;
    QString loginWindowTitle_;
    QString chatWindowTitle_;
    QUrl httpBaseUrl_;
    QString registerPath_;
    QString loginPath_;
    QString logoutPath_;
    QString avatarTempUploadPath_;
    QString userAvatarPathTemplate_;
    QString userSearchPath_;
    QString friendListPath_;
    QString friendSendRequestPath_;
    QString friendOutgoingRequestsPath_;
    QString friendIncomingRequestsPath_;
    QString friendAcceptRequestPathTemplate_;
    QString friendRejectRequestPathTemplate_;
    QString conversationPrivatePath_;
    QString conversationListPath_;
    QString conversationDetailPathTemplate_;
    QString conversationMessagesPathTemplate_;
    QString conversationSendTextPathTemplate_;
    QUrl webSocketUrl_;
    QString logAppName_;
    QString logMinimumLevel_ = QStringLiteral("INFO");
    bool logEnableConsole_ = true;
    bool logEnableFile_ = true;
    bool logDisplayLocalTime_ = true;
    QString logDirectory_ = QStringLiteral("../logs");
    QString logFileName_ = QStringLiteral("chatclient.log");
};

}  // namespace chatclient::config
