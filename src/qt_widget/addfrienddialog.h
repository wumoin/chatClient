#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QTextEdit;
class QFrame;

// AddFriendDialog 承担“添加好友”独立窗口职责：
// 1) 构建“先搜索账号，再申请添加”的两阶段界面；
// 2) 管理搜索结果卡、验证消息输入框和提交按钮的启用状态；
// 3) 让 ChatWindow 不再承担弹窗内部的布局与状态逻辑。
//
// 当前仍然只是前端骨架：
// - 真实搜索用户接口尚未接入；
// - 真实发送好友申请接口尚未接入。
class AddFriendDialog : public QDialog
{
  public:
    /**
     * @brief 构造“添加好友”弹窗。
     * @param parent 父级 QWidget，可为空。
     */
    explicit AddFriendDialog(QWidget *parent = nullptr);

  private:
    /**
     * @brief 根据当前搜索结果状态更新界面。
     * @param account 搜索到的账号；为空表示未命中或未搜索。
     */
    void updateSearchResult(const QString &account);

    /**
     * @brief 更新底部状态提示文本与语义色。
     * @param text 当前提示文本。
     * @param tone 当前提示语义，例如 info / error。
     */
    void setStatusMessage(const QString &text, const QString &tone);

    QLineEdit *m_accountEdit = nullptr;
    QPushButton *m_searchButton = nullptr;
    QLabel *m_emptyHintLabel = nullptr;
    QFrame *m_resultCard = nullptr;
    QLabel *m_resultTitleLabel = nullptr;
    QLabel *m_resultMetaLabel = nullptr;
    QLabel *m_resultHintLabel = nullptr;
    QTextEdit *m_verifyEdit = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_submitButton = nullptr;
};
