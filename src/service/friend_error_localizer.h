#pragma once

#include "dto/friend_dto.h"

#include <QString>

namespace chatclient::service {

/**
 * @brief 将好友域接口错误转换成中文提示。
 * @param error 服务端或网络层返回的统一错误 DTO。
 * @return 可直接展示给用户的中文提示；若无法识别则返回空字符串。
 */
QString localizeFriendError(
    const chatclient::dto::friendship::ApiErrorDto &error);

}  // namespace chatclient::service
