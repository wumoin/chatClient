#include "addfrienddialog.h"

#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString loadChatStyleSheet()
{
    QFile file(QStringLiteral(":/chatwindow.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

}  // namespace

AddFriendDialog::AddFriendDialog(QWidget *parent)
    : QDialog(parent)
{
    setModal(true);
    setObjectName(QStringLiteral("addFriendDialog"));
    setWindowTitle(QStringLiteral("添加好友"));
    setFixedSize(440, 420);
    setStyleSheet(loadChatStyleSheet());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("添加好友"), this);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));

    auto *subtitleLabel = new QLabel(
        QStringLiteral("先搜索账号是否存在，确认结果后再填写验证消息并发送好友申请。"),
        this);
    subtitleLabel->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitleLabel->setWordWrap(true);

    auto *searchRow = new QHBoxLayout();
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(10);

    m_accountEdit = new QLineEdit(this);
    m_accountEdit->setObjectName(QStringLiteral("dialogLineEdit"));
    m_accountEdit->setPlaceholderText(QStringLiteral("输入对方账号"));

    m_searchButton = new QPushButton(QStringLiteral("搜索用户"), this);
    m_searchButton->setObjectName(QStringLiteral("dialogPrimaryButton"));

    searchRow->addWidget(m_accountEdit, 1);
    searchRow->addWidget(m_searchButton);

    m_emptyHintLabel = new QLabel(
        QStringLiteral("先输入账号并搜索，搜到后再决定是否发起好友申请。"),
        this);
    m_emptyHintLabel->setObjectName(QStringLiteral("dialogEmptyHintLabel"));
    m_emptyHintLabel->setWordWrap(true);

    m_resultCard = new QFrame(this);
    m_resultCard->setObjectName(QStringLiteral("dialogResultCard"));
    auto *resultLayout = new QVBoxLayout(m_resultCard);
    resultLayout->setContentsMargins(14, 14, 14, 14);
    resultLayout->setSpacing(8);

    auto *resultTag = new QLabel(QStringLiteral("搜索结果"), m_resultCard);
    resultTag->setObjectName(QStringLiteral("dialogResultTag"));

    m_resultTitleLabel = new QLabel(QString(), m_resultCard);
    m_resultTitleLabel->setObjectName(QStringLiteral("dialogResultTitle"));

    m_resultMetaLabel = new QLabel(QString(), m_resultCard);
    m_resultMetaLabel->setObjectName(QStringLiteral("dialogResultMeta"));

    m_resultHintLabel = new QLabel(QString(), m_resultCard);
    m_resultHintLabel->setObjectName(QStringLiteral("dialogResultHint"));
    m_resultHintLabel->setWordWrap(true);

    m_verifyEdit = new QTextEdit(this);
    m_verifyEdit->setObjectName(QStringLiteral("dialogTextEdit"));
    m_verifyEdit->setPlaceholderText(QStringLiteral("验证消息（可选）"));
    m_verifyEdit->setMinimumHeight(92);

    resultLayout->addWidget(resultTag);
    resultLayout->addWidget(m_resultTitleLabel);
    resultLayout->addWidget(m_resultMetaLabel);
    resultLayout->addWidget(m_resultHintLabel);
    resultLayout->addWidget(m_verifyEdit);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("dialogStatusLabel"));
    m_statusLabel->setWordWrap(true);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(10);

    auto *cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setObjectName(QStringLiteral("dialogSecondaryButton"));

    m_submitButton = new QPushButton(QStringLiteral("申请添加"), this);
    m_submitButton->setObjectName(QStringLiteral("dialogPrimaryButton"));

    buttonRow->addStretch(1);
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(m_submitButton);

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addLayout(searchRow);
    layout->addWidget(m_emptyHintLabel);
    layout->addWidget(m_resultCard);
    layout->addWidget(m_statusLabel);
    layout->addStretch(1);
    layout->addLayout(buttonRow);

    updateSearchResult(QString());
    setStatusMessage(QString(), QStringLiteral("info"));

    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    connect(m_searchButton, &QPushButton::clicked, this, [this]() {
        const QString account = m_accountEdit->text().trimmed();
        if (account.isEmpty()) {
            updateSearchResult(QString());
            setStatusMessage(QStringLiteral("请先输入要搜索的账号。"),
                             QStringLiteral("error"));
            return;
        }

        // 当前阶段先把“先搜索、再申请”的交互骨架单独收口到弹窗类中。
        // 真实用户存在性校验后续接服务端接口，这里先用演示态结果承接 UI。
        updateSearchResult(account);
        setStatusMessage(QStringLiteral("已显示搜索结果，确认后可申请添加好友。"),
                         QStringLiteral("info"));
    });

    connect(m_submitButton, &QPushButton::clicked, this, [this]() {
        if (!m_submitButton->isEnabled()) {
            setStatusMessage(QStringLiteral("请先搜索用户，再决定是否申请添加。"),
                             QStringLiteral("error"));
            return;
        }

        QMessageBox::information(
            this,
            QStringLiteral("添加好友"),
            QStringLiteral("好友搜索和申请接口尚未接入，当前已完成独立窗口骨架。"));
        accept();
    });
}

void AddFriendDialog::updateSearchResult(const QString &account)
{
    const bool hasResult = !account.trimmed().isEmpty();

    m_emptyHintLabel->setVisible(!hasResult);
    m_resultCard->setVisible(hasResult);
    m_verifyEdit->setEnabled(hasResult);
    m_submitButton->setEnabled(hasResult);

    if (!hasResult) {
        m_resultTitleLabel->clear();
        m_resultMetaLabel->clear();
        m_resultHintLabel->clear();
        m_verifyEdit->clear();
        return;
    }

    m_resultTitleLabel->setText(account);
    m_resultMetaLabel->setText(QStringLiteral("账号搜索结果（演示）"));
    m_resultHintLabel->setText(
        QStringLiteral("后续这里会显示用户昵称、账号、头像和关系状态。当前先完成“搜索 -> 确认结果 -> 申请添加”的两阶段界面。"));
}

void AddFriendDialog::setStatusMessage(const QString &text, const QString &tone)
{
    m_statusLabel->setProperty("statusTone", tone);
    m_statusLabel->setText(text);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}
