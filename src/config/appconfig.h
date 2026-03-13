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
     * @brief 返回 WebSocket 服务地址。
     * @return WebSocket 完整 URL。
     */
    const QUrl &webSocketUrl() const;

    /**
     * @brief 返回适合界面直接展示的 HTTP 服务地址文本。
     * @return HTTP base URL 的字符串形式。
     */
    QString httpBaseUrlText() const;

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

    QString displayName_;
    QString loginWindowTitle_;
    QString chatWindowTitle_;
    QUrl httpBaseUrl_;
    QString registerPath_;
    QString loginPath_;
    QUrl webSocketUrl_;
};

}  // namespace chatclient::config
