#include "addfrienddialog.h"

#include "service/auth_service.h"
#include "service/friend_service.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <functional>

namespace {

constexpr int kSearchResultAreaHeight = 128;
constexpr int kVerifyMessageHeight = 92;

QString loadChatStyleSheet()
{
    QFile file(QStringLiteral(":/chatwindow.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

QListWidgetItem *createRichListItem(const QString &title,
                                    const QString &subtitle,
                                    const QString &detail,
                                    QListWidget *parent)
{
    QString text = title;
    if (!subtitle.isEmpty())
    {
        text += QStringLiteral("\n") + subtitle;
    }
    if (!detail.isEmpty())
    {
        text += QStringLiteral("\n") + detail;
    }

    auto *item = new QListWidgetItem(text, parent);
    item->setSizeHint(QSize(0, 76));
    item->setToolTip(text);
    return item;
}

QString formatTimestamp(const qint64 value)
{
    if (value <= 0)
    {
        return QStringLiteral("--");
    }

    return QDateTime::fromMSecsSinceEpoch(value).toString(
        QStringLiteral("yyyy-MM-dd HH:mm"));
}

QString requestStatusText(const QString &status)
{
    if (status == QStringLiteral("pending"))
    {
        return QStringLiteral("待处理");
    }
    if (status == QStringLiteral("accepted"))
    {
        return QStringLiteral("已通过");
    }
    if (status == QStringLiteral("rejected"))
    {
        return QStringLiteral("已拒绝");
    }
    if (status == QStringLiteral("canceled"))
    {
        return QStringLiteral("已取消");
    }
    if (status == QStringLiteral("expired"))
    {
        return QStringLiteral("已过期");
    }

    return status;
}

QString incomingDetailText(
    const chatclient::dto::friendship::FriendRequestItemDto &request)
{
    QString detail = QStringLiteral("附言：%1")
                         .arg(request.requestMessage.trimmed().isEmpty()
                                  ? QStringLiteral("未填写附言")
                                  : request.requestMessage);

    if (request.hasHandledAt)
    {
        detail += QStringLiteral(" · 处理于 %1")
                      .arg(formatTimestamp(request.handledAtMs));
    }

    return detail;
}

QWidget *createIncomingRequestWidget(
    const chatclient::dto::friendship::FriendRequestItemDto &request,
    const bool actionsEnabled,
    AddFriendDialog *dialog,
    const std::function<void()> &onAccept,
    const std::function<void()> &onReject)
{
    auto *card = new QFrame(dialog);
    card->setObjectName(QStringLiteral("incomingRequestCard"));

    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);

    auto *titleLabel = new QLabel(
        QStringLiteral("%1 (%2)")
            .arg(request.peerUser.nickname, request.peerUser.account),
        card);
    titleLabel->setObjectName(QStringLiteral("incomingRequestTitle"));

    auto *metaLabel = new QLabel(
        QStringLiteral("状态：%1 · 发送于 %2")
            .arg(requestStatusText(request.status),
                 formatTimestamp(request.createdAtMs)),
        card);
    metaLabel->setObjectName(QStringLiteral("incomingRequestMeta"));

    auto *detailLabel = new QLabel(incomingDetailText(request), card);
    detailLabel->setObjectName(QStringLiteral("incomingRequestDetail"));
    detailLabel->setWordWrap(true);

    auto *actionRow = new QHBoxLayout();
    actionRow->setContentsMargins(0, 0, 0, 0);
    actionRow->setSpacing(8);

    auto *rejectButton = new QPushButton(QStringLiteral("拒绝"), card);
    rejectButton->setObjectName(QStringLiteral("dialogInlineSecondaryButton"));
    auto *acceptButton = new QPushButton(QStringLiteral("同意"), card);
    acceptButton->setObjectName(QStringLiteral("dialogInlinePrimaryButton"));

    const bool pending = request.status == QStringLiteral("pending");
    acceptButton->setEnabled(pending && actionsEnabled);
    rejectButton->setEnabled(pending && actionsEnabled);

    QObject::connect(acceptButton, &QPushButton::clicked, dialog, onAccept);
    QObject::connect(rejectButton, &QPushButton::clicked, dialog, onReject);

    actionRow->addStretch(1);
    actionRow->addWidget(rejectButton);
    actionRow->addWidget(acceptButton);

    layout->addWidget(titleLabel);
    layout->addWidget(metaLabel);
    layout->addWidget(detailLabel);
    layout->addLayout(actionRow);

    return card;
}

}  // namespace

AddFriendDialog::AddFriendDialog(chatclient::service::AuthService *authService,
                                 QWidget *parent)
    : QDialog(parent),
      m_friendService(new chatclient::service::FriendService(authService, this))
{
    setModal(true);
    setObjectName(QStringLiteral("addFriendDialog"));
    setWindowTitle(QStringLiteral("好友申请中心"));
    setMinimumSize(560, 660);
    setStyleSheet(loadChatStyleSheet());

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);

    auto *titleLabel = new QLabel(QStringLiteral("好友申请中心"), this);
    titleLabel->setObjectName(QStringLiteral("dialogTitle"));

    auto *subtitleLabel = new QLabel(
        QStringLiteral("在这里可以搜索用户并发出申请，也可以查看和处理收到的好友申请。"),
        this);
    subtitleLabel->setObjectName(QStringLiteral("dialogSubtitle"));
    subtitleLabel->setWordWrap(true);

    auto *modeRow = new QHBoxLayout();
    modeRow->setContentsMargins(0, 0, 0, 0);
    modeRow->setSpacing(10);

    m_applyModeButton = new QPushButton(QStringLiteral("申请好友"), this);
    m_applyModeButton->setObjectName(QStringLiteral("dialogModeButton"));
    m_applyModeButton->setCheckable(true);

    m_newFriendsModeButton = new QPushButton(QStringLiteral("新的朋友"), this);
    m_newFriendsModeButton->setObjectName(QStringLiteral("dialogModeButton"));
    m_newFriendsModeButton->setCheckable(true);

    modeRow->addWidget(m_applyModeButton);
    modeRow->addWidget(m_newFriendsModeButton);
    modeRow->addStretch(1);

    m_modeStack = new QStackedWidget(this);

    auto *applyPage = new QWidget(m_modeStack);
    auto *applyLayout = new QVBoxLayout(applyPage);
    applyLayout->setContentsMargins(0, 0, 0, 0);
    applyLayout->setSpacing(12);

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

    auto *searchSectionTitle = new QLabel(QStringLiteral("搜索结果"), this);
    searchSectionTitle->setObjectName(QStringLiteral("dialogSectionTitle"));

    m_searchPlaceholderLabel = new QLabel(
        QStringLiteral("输入账号后点击搜索，搜索结果会显示在这里。"),
        this);
    m_searchPlaceholderLabel->setObjectName(QStringLiteral("dialogEmptyHintLabel"));
    m_searchPlaceholderLabel->setWordWrap(true);
    m_searchPlaceholderLabel->setFixedHeight(kSearchResultAreaHeight);
    m_searchPlaceholderLabel->setSizePolicy(QSizePolicy::Preferred,
                                            QSizePolicy::Fixed);
    m_searchPlaceholderLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_resultCard = new QFrame(this);
    m_resultCard->setObjectName(QStringLiteral("dialogResultCard"));
    m_resultCard->setFixedHeight(kSearchResultAreaHeight);
    m_resultCard->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    auto *resultLayout = new QVBoxLayout(m_resultCard);
    resultLayout->setContentsMargins(14, 14, 14, 14);
    resultLayout->setSpacing(8);

    m_resultTitleLabel = new QLabel(QString(), m_resultCard);
    m_resultTitleLabel->setObjectName(QStringLiteral("dialogResultTitle"));

    m_resultMetaLabel = new QLabel(QString(), m_resultCard);
    m_resultMetaLabel->setObjectName(QStringLiteral("dialogResultMeta"));

    m_resultHintLabel = new QLabel(QString(), m_resultCard);
    m_resultHintLabel->setObjectName(QStringLiteral("dialogResultHint"));
    m_resultHintLabel->setWordWrap(true);

    resultLayout->addWidget(m_resultTitleLabel);
    resultLayout->addWidget(m_resultMetaLabel);
    resultLayout->addWidget(m_resultHintLabel);

    m_verifyEdit = new QTextEdit(this);
    m_verifyEdit->setObjectName(QStringLiteral("dialogTextEdit"));
    m_verifyEdit->setPlaceholderText(QStringLiteral("验证消息（可选）"));
    m_verifyEdit->setFixedHeight(kVerifyMessageHeight);
    m_verifyEdit->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

    auto *outgoingSectionTitle = new QLabel(QStringLiteral("已发送申请"), this);
    outgoingSectionTitle->setObjectName(QStringLiteral("dialogSectionTitle"));

    m_outgoingEmptyHintLabel = new QLabel(
        QStringLiteral("正在加载已发送申请记录..."), this);
    m_outgoingEmptyHintLabel->setObjectName(QStringLiteral("dialogEmptyHintLabel"));
    m_outgoingEmptyHintLabel->setWordWrap(true);

    m_outgoingList = new QListWidget(this);
    m_outgoingList->setObjectName(QStringLiteral("dialogList"));
    m_outgoingList->setSelectionMode(QAbstractItemView::NoSelection);

    applyLayout->addLayout(searchRow);
    applyLayout->addWidget(searchSectionTitle);
    applyLayout->addWidget(m_searchPlaceholderLabel);
    applyLayout->addWidget(m_resultCard);
    applyLayout->addWidget(m_verifyEdit);
    applyLayout->addWidget(outgoingSectionTitle);
    applyLayout->addWidget(m_outgoingEmptyHintLabel);
    applyLayout->addWidget(m_outgoingList, 1);

    auto *incomingPage = new QWidget(m_modeStack);
    auto *incomingLayout = new QVBoxLayout(incomingPage);
    incomingLayout->setContentsMargins(0, 0, 0, 0);
    incomingLayout->setSpacing(12);

    auto *incomingSectionTitle = new QLabel(QStringLiteral("新的朋友"), this);
    incomingSectionTitle->setObjectName(QStringLiteral("dialogSectionTitle"));

    auto *incomingSectionHint = new QLabel(
        QStringLiteral("这里会展示收到的好友申请，你可以直接同意或拒绝。"),
        this);
    incomingSectionHint->setObjectName(QStringLiteral("dialogEmptyHintLabel"));
    incomingSectionHint->setWordWrap(true);

    m_incomingEmptyHintLabel = new QLabel(
        QStringLiteral("正在加载新的朋友列表..."), this);
    m_incomingEmptyHintLabel->setObjectName(QStringLiteral("dialogEmptyHintLabel"));
    m_incomingEmptyHintLabel->setWordWrap(true);

    m_incomingList = new QListWidget(this);
    m_incomingList->setObjectName(QStringLiteral("dialogList"));
    m_incomingList->setSelectionMode(QAbstractItemView::NoSelection);

    incomingLayout->addWidget(incomingSectionTitle);
    incomingLayout->addWidget(incomingSectionHint);
    incomingLayout->addWidget(m_incomingEmptyHintLabel);
    incomingLayout->addWidget(m_incomingList, 1);

    m_modeStack->addWidget(applyPage);
    m_modeStack->addWidget(incomingPage);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("dialogStatusLabel"));
    m_statusLabel->setWordWrap(true);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->setContentsMargins(0, 0, 0, 0);
    buttonRow->setSpacing(10);

    auto *closeButton = new QPushButton(QStringLiteral("关闭"), this);
    closeButton->setObjectName(QStringLiteral("dialogSecondaryButton"));

    m_submitButton = new QPushButton(QStringLiteral("申请添加"), this);
    m_submitButton->setObjectName(QStringLiteral("dialogPrimaryButton"));

    buttonRow->addStretch(1);
    buttonRow->addWidget(closeButton);
    buttonRow->addWidget(m_submitButton);

    layout->addWidget(titleLabel);
    layout->addWidget(subtitleLabel);
    layout->addLayout(modeRow);
    layout->addWidget(m_modeStack, 1);
    layout->addWidget(m_statusLabel);
    layout->addLayout(buttonRow);

    clearSearchResult(QStringLiteral("输入账号后点击搜索，搜索结果会显示在这里。"));
    updateOutgoingRequests({});
    updateIncomingRequests({});
    setStatusMessage(QString(), QStringLiteral("info"));
    switchMode(true);

    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_applyModeButton, &QPushButton::clicked, this, [this]() {
        switchMode(true);
    });
    connect(m_newFriendsModeButton, &QPushButton::clicked, this, [this]() {
        switchMode(false);
    });
    connect(m_searchButton, &QPushButton::clicked, this, [this]() {
        QString errorMessage;
        if (!m_friendService->searchUser(m_accountEdit->text(), &errorMessage))
        {
            setStatusMessage(errorMessage, QStringLiteral("error"));
        }
    });
    connect(m_accountEdit, &QLineEdit::returnPressed, this, [this]() {
        m_searchButton->click();
    });
    connect(m_submitButton, &QPushButton::clicked, this, [this]() {
        QString errorMessage;
        if (!m_friendService->sendFriendRequest(
                m_currentSearchResult.user.userId,
                m_verifyEdit->toPlainText(),
                &errorMessage))
        {
            setStatusMessage(errorMessage, QStringLiteral("error"));
        }
    });

    connect(m_friendService,
            &chatclient::service::FriendService::searchStarted,
            this,
            [this]() {
                clearSearchResult(QStringLiteral("正在搜索账号，请稍候..."));
                setStatusMessage(QStringLiteral("正在搜索用户..."),
                                 QStringLiteral("info"));
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::searchSucceeded,
            this,
            [this](const chatclient::dto::friendship::SearchUserResponseDto &response) {
                updateSearchResult(response);
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::searchFailed,
            this,
            [this](const QString &message) {
                clearSearchResult(QStringLiteral("搜索失败后可重新输入账号继续尝试。"));
                setStatusMessage(message, QStringLiteral("error"));
                updateActionState();
            });

    connect(m_friendService,
            &chatclient::service::FriendService::outgoingRequestsStarted,
            this,
            [this]() {
                if (m_outgoingEmptyHintLabel)
                {
                    m_outgoingEmptyHintLabel->setText(
                        QStringLiteral("正在刷新已发送申请记录..."));
                    m_outgoingEmptyHintLabel->setVisible(true);
                }
                if (m_outgoingList)
                {
                    m_outgoingList->setEnabled(false);
                }
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::outgoingRequestsSucceeded,
            this,
            [this](const chatclient::dto::friendship::FriendRequestItems &requests) {
                updateOutgoingRequests(requests);
                if (m_outgoingList)
                {
                    m_outgoingList->setEnabled(true);
                }
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::outgoingRequestsFailed,
            this,
            [this](const QString &message) {
                if (m_outgoingList)
                {
                    m_outgoingList->setEnabled(true);
                }
                if (m_outgoingEmptyHintLabel)
                {
                    m_outgoingEmptyHintLabel->setText(message);
                    m_outgoingEmptyHintLabel->setVisible(true);
                }
                setStatusMessage(message, QStringLiteral("error"));
                updateActionState();
            });

    connect(m_friendService,
            &chatclient::service::FriendService::incomingRequestsStarted,
            this,
            [this]() {
                if (m_incomingEmptyHintLabel)
                {
                    m_incomingEmptyHintLabel->setText(
                        QStringLiteral("正在刷新新的朋友列表..."));
                    m_incomingEmptyHintLabel->setVisible(true);
                }
                updateIncomingRequests(m_incomingRequests);
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::incomingRequestsSucceeded,
            this,
            [this](const chatclient::dto::friendship::FriendRequestItems &requests) {
                updateIncomingRequests(requests);
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::incomingRequestsFailed,
            this,
            [this](const QString &message) {
                if (m_incomingEmptyHintLabel)
                {
                    m_incomingEmptyHintLabel->setText(message);
                    m_incomingEmptyHintLabel->setVisible(true);
                }
                updateIncomingRequests(m_incomingRequests);
                setStatusMessage(message, QStringLiteral("error"));
                updateActionState();
            });

    connect(m_friendService,
            &chatclient::service::FriendService::sendFriendRequestStarted,
            this,
            [this]() {
                setStatusMessage(QStringLiteral("正在发送好友申请..."),
                                 QStringLiteral("info"));
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::sendFriendRequestSucceeded,
            this,
            [this](const chatclient::dto::friendship::FriendRequestItemDto &) {
                m_verifyEdit->clear();
                setStatusMessage(QStringLiteral("好友申请已发送"),
                                 QStringLiteral("success"));
                loadOutgoingRequests();
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::sendFriendRequestFailed,
            this,
            [this](const QString &message) {
                setStatusMessage(message, QStringLiteral("error"));
                updateActionState();
            });

    connect(m_friendService,
            &chatclient::service::FriendService::handleFriendRequestStarted,
            this,
            [this]() {
                setStatusMessage(QStringLiteral("正在处理好友申请..."),
                                 QStringLiteral("info"));
                updateIncomingRequests(m_incomingRequests);
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::handleFriendRequestSucceeded,
            this,
            [this](const chatclient::dto::friendship::FriendRequestItemDto &request) {
                applyHandledIncomingRequest(request);
                setStatusMessage(
                    request.status == QStringLiteral("accepted")
                        ? QStringLiteral("已同意好友申请")
                        : QStringLiteral("已拒绝好友申请"),
                    QStringLiteral("success"));
                updateActionState();
            });
    connect(m_friendService,
            &chatclient::service::FriendService::handleFriendRequestFailed,
            this,
            [this](const QString &message) {
                setStatusMessage(message, QStringLiteral("error"));
                updateIncomingRequests(m_incomingRequests);
                updateActionState();
            });

    QTimer::singleShot(0, this, [this]() {
        loadOutgoingRequests();
        loadIncomingRequests();
    });
}

void AddFriendDialog::loadOutgoingRequests()
{
    QString errorMessage;
    if (!m_friendService->fetchOutgoingRequests(&errorMessage))
    {
        setStatusMessage(errorMessage, QStringLiteral("error"));
    }
}

void AddFriendDialog::loadIncomingRequests()
{
    QString errorMessage;
    if (!m_friendService->fetchIncomingRequests(&errorMessage))
    {
        setStatusMessage(errorMessage, QStringLiteral("error"));
    }
}

void AddFriendDialog::switchMode(const bool applyMode)
{
    if (m_applyModeButton)
    {
        m_applyModeButton->setChecked(applyMode);
    }
    if (m_newFriendsModeButton)
    {
        m_newFriendsModeButton->setChecked(!applyMode);
    }
    if (m_modeStack)
    {
        m_modeStack->setCurrentIndex(applyMode ? 0 : 1);
    }
    if (m_submitButton)
    {
        m_submitButton->setVisible(applyMode);
    }
}

void AddFriendDialog::updateSearchResult(
    const chatclient::dto::friendship::SearchUserResponseDto &response)
{
    m_currentSearchResult = response;

    if (!response.exists)
    {
        clearSearchResult(QStringLiteral("未找到该账号，请检查后重新搜索。"));
        setStatusMessage(QStringLiteral("未找到该用户"), QStringLiteral("info"));
        updateActionState();
        return;
    }

    m_searchPlaceholderLabel->setVisible(false);
    m_resultCard->setVisible(true);
    m_resultTitleLabel->setText(response.user.nickname);
    m_resultMetaLabel->setText(
        QStringLiteral("账号：%1\n用户 ID：%2")
            .arg(response.user.account, response.user.userId));

    const bool alreadyPending = std::any_of(
        m_outgoingRequests.cbegin(),
        m_outgoingRequests.cend(),
        [&response](const chatclient::dto::friendship::FriendRequestItemDto &item) {
            return item.peerUser.userId == response.user.userId &&
                   item.status == QStringLiteral("pending");
        });

    m_resultHintLabel->setText(
        alreadyPending
            ? QStringLiteral("你已经向该用户发送过待处理申请。")
            : QStringLiteral("确认无误后可以填写附言并发送好友申请。"));

    setStatusMessage(alreadyPending
                         ? QStringLiteral("该用户已有待处理好友申请")
                         : QStringLiteral("已找到目标用户，可继续发送好友申请"),
                     alreadyPending ? QStringLiteral("info")
                                    : QStringLiteral("success"));
    updateActionState();
}

void AddFriendDialog::clearSearchResult(const QString &placeholder)
{
    m_currentSearchResult = {};
    if (m_searchPlaceholderLabel)
    {
        m_searchPlaceholderLabel->setText(placeholder);
        m_searchPlaceholderLabel->setVisible(true);
    }
    if (m_resultCard)
    {
        m_resultCard->setVisible(false);
    }
    if (m_resultTitleLabel)
    {
        m_resultTitleLabel->clear();
    }
    if (m_resultMetaLabel)
    {
        m_resultMetaLabel->clear();
    }
    if (m_resultHintLabel)
    {
        m_resultHintLabel->clear();
    }
    updateActionState();
}

void AddFriendDialog::updateOutgoingRequests(
    const chatclient::dto::friendship::FriendRequestItems &requests)
{
    m_outgoingRequests = requests;

    if (!m_outgoingList)
    {
        return;
    }

    m_outgoingList->clear();

    if (requests.isEmpty())
    {
        if (m_outgoingEmptyHintLabel)
        {
            m_outgoingEmptyHintLabel->setText(
                QStringLiteral("当前还没有发出过好友申请。"));
            m_outgoingEmptyHintLabel->setVisible(true);
        }
        updateActionState();
        return;
    }

    if (m_outgoingEmptyHintLabel)
    {
        m_outgoingEmptyHintLabel->setVisible(false);
    }

    for (const auto &request : requests)
    {
        const QString title = QStringLiteral("%1 (%2)")
                                  .arg(request.peerUser.nickname,
                                       request.peerUser.account);
        const QString subtitle = QStringLiteral("状态：%1 · 发送于 %2")
                                     .arg(requestStatusText(request.status),
                                          formatTimestamp(request.createdAtMs));
        const QString detail =
            request.requestMessage.trimmed().isEmpty()
                ? QStringLiteral("未填写附言")
                : QStringLiteral("附言：%1").arg(request.requestMessage);
        createRichListItem(title, subtitle, detail, m_outgoingList);
    }

    updateActionState();
}

void AddFriendDialog::updateIncomingRequests(
    const chatclient::dto::friendship::FriendRequestItems &requests)
{
    m_incomingRequests = requests;

    if (!m_incomingList)
    {
        return;
    }

    m_incomingList->clear();

    if (requests.isEmpty())
    {
        if (m_incomingEmptyHintLabel)
        {
            m_incomingEmptyHintLabel->setText(
                QStringLiteral("当前还没有收到新的好友申请。"));
            m_incomingEmptyHintLabel->setVisible(true);
        }
        return;
    }

    if (m_incomingEmptyHintLabel)
    {
        m_incomingEmptyHintLabel->setVisible(false);
    }

    const bool actionsEnabled =
        !(m_friendService && m_friendService->isHandlingRequest()) &&
        !(m_friendService && m_friendService->isLoadingIncomingRequests());

    for (const auto &request : requests)
    {
        auto *item = new QListWidgetItem(m_incomingList);
        auto *widget = createIncomingRequestWidget(
            request,
            actionsEnabled,
            this,
            [this, request]() {
                QString errorMessage;
                if (!m_friendService->acceptFriendRequest(request.requestId,
                                                          &errorMessage))
                {
                    setStatusMessage(errorMessage, QStringLiteral("error"));
                }
            },
            [this, request]() {
                QString errorMessage;
                if (!m_friendService->rejectFriendRequest(request.requestId,
                                                          &errorMessage))
                {
                    setStatusMessage(errorMessage, QStringLiteral("error"));
                }
            });
        item->setSizeHint(widget->sizeHint());
        m_incomingList->setItemWidget(item, widget);
    }
}

void AddFriendDialog::updateActionState()
{
    const bool searching = m_friendService && m_friendService->isSearching();
    const bool loadingOutgoing =
        m_friendService && m_friendService->isLoadingOutgoingRequests();
    const bool sending = m_friendService && m_friendService->isSendingRequest();

    if (m_searchButton)
    {
        m_searchButton->setEnabled(!searching && !sending);
        m_searchButton->setText(searching ? QStringLiteral("搜索中...")
                                          : QStringLiteral("搜索用户"));
    }

    if (m_accountEdit)
    {
        m_accountEdit->setEnabled(!searching && !sending);
    }

    const bool hasSearchUser =
        m_currentSearchResult.exists &&
        !m_currentSearchResult.user.userId.trimmed().isEmpty();
    const bool alreadyPending = std::any_of(
        m_outgoingRequests.cbegin(),
        m_outgoingRequests.cend(),
        [this](const chatclient::dto::friendship::FriendRequestItemDto &item) {
            return item.peerUser.userId == m_currentSearchResult.user.userId &&
                   item.status == QStringLiteral("pending");
        });
    const bool canSubmit = hasSearchUser && !alreadyPending && !sending &&
                           !searching && !loadingOutgoing;

    if (m_resultHintLabel && hasSearchUser)
    {
        m_resultHintLabel->setText(
            alreadyPending
                ? QStringLiteral("你已经向该用户发送过待处理申请。")
                : QStringLiteral("确认无误后可以填写附言并发送好友申请。"));
    }

    if (m_verifyEdit)
    {
        m_verifyEdit->setEnabled(canSubmit);
    }
    if (m_submitButton)
    {
        m_submitButton->setEnabled(canSubmit);
        m_submitButton->setText(sending ? QStringLiteral("发送中...")
                                        : QStringLiteral("申请添加"));
    }
}

void AddFriendDialog::applyHandledIncomingRequest(
    const chatclient::dto::friendship::FriendRequestItemDto &request)
{
    for (auto &item : m_incomingRequests)
    {
        if (item.requestId == request.requestId)
        {
            item = request;
            updateIncomingRequests(m_incomingRequests);
            return;
        }
    }

    loadIncomingRequests();
}

void AddFriendDialog::setStatusMessage(const QString &text, const QString &tone)
{
    m_statusLabel->setProperty("statusTone", tone);
    m_statusLabel->setText(text);
    m_statusLabel->style()->unpolish(m_statusLabel);
    m_statusLabel->style()->polish(m_statusLabel);
}
