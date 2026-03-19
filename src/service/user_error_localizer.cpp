#include "service/user_error_localizer.h"

namespace chatclient::service {

QString localizeUserError(const chatclient::dto::user::ApiErrorDto &error)
{
    switch (error.errorCode)
    {
    case 40001:
        if (error.message == QStringLiteral("avatar file is required"))
        {
            return QStringLiteral("请选择需要上传的头像图片");
        }
        if (error.message == QStringLiteral("avatar file must be an image"))
        {
            return QStringLiteral("头像文件必须是图片格式");
        }
        if (error.message == QStringLiteral("avatar_upload_key is required"))
        {
            return QStringLiteral("缺少头像上传凭据，请重新上传头像");
        }
        break;
    case 40400:
        return QStringLiteral("头像文件不存在或已失效，请重新上传");
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
