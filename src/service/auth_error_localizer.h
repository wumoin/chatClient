#pragma once

#include "dto/auth_dto.h"

#include <QString>

namespace chatclient::service {

/**
 * @brief 将认证接口错误转换为适合界面直接展示的中文提示。
 * @param error 认证接口失败 DTO，包含 HTTP 状态码、业务码和原始英文消息。
 * @return 已完成中文化处理的错误提示；若无法映射则尽量保留原始消息。
 */
QString localizeAuthError(
    const chatclient::dto::auth::ApiErrorDto &error);

}  // namespace chatclient::service
