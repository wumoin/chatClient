#pragma once

#include "dto/friend_dto.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QTextEdit;
class QFrame;

namespace chatclient::service {
class AuthService;
class FriendService;
}

// AddFriendDialog 承担“好友申请中心”独立窗口职责：
// 1) 通过顶部选项栏切换“申请好友 / 新的朋友”两个子页面；
// 2) 在申请页展示搜索结果与已发送申请记录；
// 3) 在新的朋友页展示收到的申请，并直接处理同意 / 拒绝。
//
class AddFriendDialog : public QDialog
{
  public:
    /**
     * @brief 构造“添加好友”弹窗。
     * @param authService 当前认证服务，用于读取 access token。
     * @param parent 父级 QWidget，可为空。
     */
    explicit AddFriendDialog(chatclient::service::AuthService *authService,
                             QWidget *parent = nullptr);

  private:
    /**
     * @brief 拉取当前用户已发送的好友申请记录。
     */
    void loadOutgoingRequests();

    /**
     * @brief 拉取当前用户收到的好友申请记录。
     */
    void loadIncomingRequests();

    /**
     * @brief 切换顶部选项栏当前页面。
     * @param applyMode true 表示切到“申请好友”；false 表示切到“新的朋友”。
     */
    void switchMode(bool applyMode);

    /**
     * @brief 根据当前搜索结果更新顶部卡片。
     * @param response 搜索成功返回的 DTO；`exists=false` 时表示未命中。
     */
    void updateSearchResult(
        const chatclient::dto::friendship::SearchUserResponseDto &response);

    /**
     * @brief 清空当前搜索结果并恢复顶部占位提示。
     * @param placeholder 顶部占位提示文本。
     */
    void clearSearchResult(const QString &placeholder);

    /**
     * @brief 刷新底部“已发送申请记录”列表。
     * @param requests 当前已发送好友申请集合。
     */
    void updateOutgoingRequests(
        const chatclient::dto::friendship::FriendRequestItems &requests);

    /**
     * @brief 刷新“新的朋友”列表。
     * @param requests 当前收到的好友申请集合。
     */
    void updateIncomingRequests(
        const chatclient::dto::friendship::FriendRequestItems &requests);

    /**
     * @brief 根据当前状态统一更新按钮可用性。
     */
    void updateActionState();

    /**
     * @brief 在本地缓存和界面列表里更新指定好友申请。
     * @param request 服务端返回的最新好友申请记录。
     */
    void applyHandledIncomingRequest(
        const chatclient::dto::friendship::FriendRequestItemDto &request);

    /**
     * @brief 更新底部状态提示文本与语义色。
     * @param text 当前提示文本。
     * @param tone 当前提示语义，例如 info / error。
     */
    void setStatusMessage(const QString &text, const QString &tone);

    QPushButton *m_applyModeButton = nullptr;
    QPushButton *m_newFriendsModeButton = nullptr;
    QStackedWidget *m_modeStack = nullptr;
    QLineEdit *m_accountEdit = nullptr;
    QPushButton *m_searchButton = nullptr;
    QLabel *m_searchPlaceholderLabel = nullptr;
    QFrame *m_resultCard = nullptr;
    QLabel *m_resultTitleLabel = nullptr;
    QLabel *m_resultMetaLabel = nullptr;
    QLabel *m_resultHintLabel = nullptr;
    QTextEdit *m_verifyEdit = nullptr;
    QListWidget *m_outgoingList = nullptr;
    QLabel *m_outgoingEmptyHintLabel = nullptr;
    QListWidget *m_incomingList = nullptr;
    QLabel *m_incomingEmptyHintLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_submitButton = nullptr;
    chatclient::service::FriendService *m_friendService = nullptr;
    chatclient::dto::friendship::SearchUserResponseDto m_currentSearchResult;
    chatclient::dto::friendship::FriendRequestItems m_outgoingRequests;
    chatclient::dto::friendship::FriendRequestItems m_incomingRequests;
};
