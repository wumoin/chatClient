#include "service/friend_error_localizer.h"

// localizer 负责把服务端稳定错误码和少量兼容 message 映射成
// 面向用户的中文提示。日志是否中文化与这里无关：日志是给开发排障，
// localizer 是给 UI 直接展示。
namespace chatclient::service {

QString localizeFriendError(
    const chatclient::dto::friendship::ApiErrorDto &error)
{
    switch (error.errorCode)
    {
    case 40001:
        if (error.message == QStringLiteral("account is required"))
        {
            return QStringLiteral("请输入要搜索的账号");
        }
        if (error.message ==
            QStringLiteral("request_message length must not exceed 200"))
        {
            return QStringLiteral("申请附言不能超过 200 个字符");
        }
        if (error.message == QStringLiteral("target_user_id is required"))
        {
            return QStringLiteral("缺少目标用户信息，请重新搜索");
        }
        return QStringLiteral("请求参数不正确，请检查后重试");
    case 40102:
        return QStringLiteral("当前登录已失效，请重新登录");
    case 40300:
        if (error.message ==
            QStringLiteral("forbidden to handle this friend request"))
        {
            return QStringLiteral("这条好友申请不属于当前账号");
        }
        return QStringLiteral("当前没有权限执行该操作");
    case 40400:
        if (error.message == QStringLiteral("target user not found"))
        {
            return QStringLiteral("未找到该用户");
        }
        if (error.message == QStringLiteral("friend request not found"))
        {
            return QStringLiteral("好友申请不存在或已被删除");
        }
        return QStringLiteral("目标资源不存在");
    case 40903:
        return QStringLiteral("对方已经是你的好友");
    case 40904:
        return QStringLiteral("好友申请已经发送，请等待对方处理");
    case 40905:
        return QStringLiteral("这条好友申请已经处理过了");
    default:
        break;
    }

    if (error.httpStatus == 0)
    {
        return QStringLiteral("网络请求失败，请检查服务端是否已启动");
    }

    return QString();
}

}  // namespace chatclient::service
