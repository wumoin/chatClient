#include "service/auth_error_localizer.h"

#include <QRegularExpression>

namespace chatclient::service {
namespace {

// 认证错误本地化策略：
// 1) 优先按稳定业务码映射，避免服务端文案调整后影响客户端提示；
// 2) 再按当前已知英文 message 做兼容映射；
// 3) 最后再做字段名翻译和通用兜底。
const QRegularExpression kFieldTypeErrorPattern(
    QStringLiteral("^Field '([^']+)' must be a string(?: when provided)?$"));

/**
 * @brief 把服务端字段名翻译为更适合界面展示的中文字段名。
 * @param fieldName 服务端错误消息中的原始字段名。
 * @return 中文字段名；如果当前没有专门映射，则回退原始字段名。
 */
QString translateFieldName(const QString &fieldName)
{
    if (fieldName == QStringLiteral("account"))
    {
        return QStringLiteral("账号");
    }
    if (fieldName == QStringLiteral("password"))
    {
        return QStringLiteral("密码");
    }
    if (fieldName == QStringLiteral("nickname"))
    {
        return QStringLiteral("昵称");
    }
    if (fieldName == QStringLiteral("avatar_url"))
    {
        return QStringLiteral("头像地址");
    }
    if (fieldName == QStringLiteral("device_id"))
    {
        return QStringLiteral("设备标识");
    }
    if (fieldName == QStringLiteral("device_platform"))
    {
        return QStringLiteral("设备平台");
    }
    if (fieldName == QStringLiteral("device_name"))
    {
        return QStringLiteral("设备名称");
    }
    if (fieldName == QStringLiteral("client_version"))
    {
        return QStringLiteral("客户端版本");
    }

    return fieldName;
}

}  // namespace

QString localizeAuthError(const chatclient::dto::auth::ApiErrorDto &error)
{
    // 先按 errorCode 做稳定映射。
    // 这是最推荐的路径，因为它不依赖服务端英文文案细节。
    if (error.errorCode == 40901)
    {
        return QStringLiteral("账号已存在");
    }

    if (error.errorCode == 40101)
    {
        return QStringLiteral("账号或密码错误");
    }
    if (error.errorCode == 40102)
    {
        return QStringLiteral("登录态已失效，请重新登录");
    }
    if (error.errorCode == 40902)
    {
        return QStringLiteral("当前设备已登录该账号");
    }

    if (error.errorCode == 40301)
    {
        return QStringLiteral("账号已被禁用");
    }

    if (error.errorCode == 40302)
    {
        return QStringLiteral("账号已被锁定");
    }

    const QString message = error.message.trimmed();
    if (message.isEmpty())
    {
        return QString();
    }

    // 再按当前真实返回过的英文 message 做兼容映射。
    // 这样服务端短期内还没彻底完成错误码细分时，客户端仍能给出中文提示。
    if (message == QStringLiteral("invalid json body"))
    {
        return QStringLiteral("请求内容不是合法的 JSON");
    }
    if (message == QStringLiteral("account already exists"))
    {
        return QStringLiteral("账号已存在");
    }
    if (message == QStringLiteral("device already logged in"))
    {
        return QStringLiteral("当前设备已登录该账号");
    }
    if (message == QStringLiteral("invalid credentials"))
    {
        return QStringLiteral("账号或密码错误");
    }
    if (message == QStringLiteral("invalid access token"))
    {
        return QStringLiteral("登录态已失效，请重新登录");
    }
    if (message == QStringLiteral("account disabled"))
    {
        return QStringLiteral("账号已被禁用");
    }
    if (message == QStringLiteral("account locked"))
    {
        return QStringLiteral("账号已被锁定");
    }
    if (message ==
        QStringLiteral("account must not contain leading or trailing spaces"))
    {
        return QStringLiteral("账号不能包含前后空格");
    }
    if (message == QStringLiteral("account length must be between 3 and 64"))
    {
        return QStringLiteral("账号长度必须在 3 到 64 个字符之间");
    }
    if (message ==
        QStringLiteral("account may contain only letters, digits, '_', '.' and '-'"))
    {
        return QStringLiteral("账号只允许字母、数字、下划线、点和短横线");
    }
    if (message == QStringLiteral("password length must be between 8 and 72"))
    {
        return QStringLiteral("密码长度必须在 8 到 72 个字符之间");
    }
    if (message == QStringLiteral("password must not be empty"))
    {
        return QStringLiteral("密码不能为空");
    }
    if (message == QStringLiteral("nickname length must be between 1 and 64"))
    {
        return QStringLiteral("昵称长度必须在 1 到 64 个字符之间");
    }
    if (message == QStringLiteral("avatar_url length must not exceed 2048"))
    {
        return QStringLiteral("头像地址长度不能超过 2048 个字符");
    }
    if (message == QStringLiteral(
                       "device_id must not be empty, contain leading/trailing spaces or exceed 128 characters"))
    {
        return QStringLiteral(
            "设备标识不能为空、不能包含前后空格，且长度不能超过 128 个字符");
    }
    if (message ==
        QStringLiteral("device_platform length must be between 1 and 32"))
    {
        return QStringLiteral("设备平台长度必须在 1 到 32 个字符之间");
    }
    if (message == QStringLiteral(
                       "device_platform may contain only letters, digits, '_' and '-'"))
    {
        return QStringLiteral("设备平台只允许字母、数字、下划线和短横线");
    }
    if (message == QStringLiteral("device_name length must not exceed 128"))
    {
        return QStringLiteral("设备名称长度不能超过 128 个字符");
    }
    if (message == QStringLiteral("client_version length must not exceed 32"))
    {
        return QStringLiteral("客户端版本长度不能超过 32 个字符");
    }

    const auto match = kFieldTypeErrorPattern.match(message);
    if (match.hasMatch())
    {
        const QString fieldName = translateFieldName(match.captured(1));
        return QStringLiteral("%1字段类型不正确").arg(fieldName);
    }

    // 500 类错误当前统一收敛成通用中文提示，避免把底层英文异常直接暴露给最终用户。
    if (error.errorCode == 50000)
    {
        return QStringLiteral("该用户已被注册");
    }

    return message;
}

}  // namespace chatclient::service
