#include "CustomerServiceDialog.h"
#include "../network/ApiClient.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QIcon>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

// ── 颜色常量（与 MainWindow 保持一致）────────────────────────────────────────
static const QString BG_PRIMARY    = "#0B1220";
static const QString BG_SECONDARY  = "#1E293B";
static const QString BG_CARD       = "#141C2B";
static const QString CLR_ACCENT    = "#38BDF8";
static const QString CLR_TXT_PRI   = "#F1F5F9";
static const QString CLR_TXT_SEC   = "#94A3B8";
static const QString CLR_BORDER    = "#2A3A52";
static const QString CLR_USER_BG   = "#1D4ED8";   // 用户气泡蓝
static const QString CLR_BOT_BG    = "#1E293B";   // 客服气泡灰
static const QString CLR_THINKING  = "#334155";   // 思考中气泡

// ── 辅助：绘制关闭图标 ───────────────────────────────────────────────────────
static QIcon makeCsCloseIcon(const QColor& color, int sz = 14) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
    int m = int(sz * 0.22);
    p.drawLine(m, m, sz - m, sz - m);
    p.drawLine(sz - m, m, m, sz - m);
    p.end();
    return QIcon(pm);
}

// ══════════════════════════════════════════════════════════════════════════════
// CustomerServiceDialog
// ══════════════════════════════════════════════════════════════════════════════

CustomerServiceDialog::CustomerServiceDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(false);
    setFixedSize(720, 540);
    setupUi();
}

void CustomerServiceDialog::setupUi()
{
    // ── 根容器 ───────────────────────────────────────────────────────────────
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* frame = new QWidget(this);
    frame->setObjectName("csFrame");
    frame->setStyleSheet(
        QString("QWidget#csFrame {"
                "  background: %1;"
                "  border: 1px solid %2;"
                "  border-radius: 12px;"
                "}").arg(BG_PRIMARY, CLR_BORDER)
    );
    root->addWidget(frame);

    auto* mainLayout = new QVBoxLayout(frame);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── 标题栏 ───────────────────────────────────────────────────────────────
    auto* titleBar = new QWidget(frame);
    titleBar->setFixedHeight(50);
    titleBar->setStyleSheet(
        QString("background: %1; border-radius: 12px 12px 0 0;").arg(BG_SECONDARY)
    );

    auto* titleLayout = new QHBoxLayout(titleBar);
    titleLayout->setContentsMargins(16, 0, 10, 0);
    titleLayout->setSpacing(10);

    // 图标
    auto* iconLbl = new QLabel(titleBar);
    iconLbl->setFixedSize(22, 22);
    iconLbl->setStyleSheet(
        QString("background: %1; border-radius: 5px; color: #fff; font-size: 12px;").arg(CLR_ACCENT)
    );
    iconLbl->setAlignment(Qt::AlignCenter);
    iconLbl->setText("💬");

    auto* titleLbl = new QLabel("FileTran 客服", titleBar);
    titleLbl->setStyleSheet(
        QString("color: %1; font-size: 14px; font-weight: 700;").arg(CLR_TXT_PRI)
    );

    auto* subLbl = new QLabel("在线客服", titleBar);
    subLbl->setStyleSheet(
        QString("color: %1; font-size: 11px;").arg(CLR_TXT_SEC)
    );

    auto* closeBtn = new QPushButton(titleBar);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setIcon(makeCsCloseIcon(QColor(CLR_TXT_SEC)));
    closeBtn->setIconSize(QSize(12, 12));
    closeBtn->setStyleSheet(
        "QPushButton { background: transparent; border: none; border-radius: 6px; }"
        "QPushButton:hover { background: #e74c3c; }"
    );
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::hide);

    titleLayout->addWidget(iconLbl);
    titleLayout->addWidget(titleLbl);
    titleLayout->addWidget(subLbl);
    titleLayout->addStretch();
    titleLayout->addWidget(closeBtn);

    mainLayout->addWidget(titleBar);

    // ── 分割线 ───────────────────────────────────────────────────────────────
    auto* divider = new QFrame(frame);
    divider->setFrameShape(QFrame::HLine);
    divider->setStyleSheet(QString("color: %1;").arg(CLR_BORDER));
    mainLayout->addWidget(divider);

    // ── 聊天区域 ─────────────────────────────────────────────────────────────
    m_scrollArea = new QScrollArea(frame);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setStyleSheet(
        QString("QScrollArea { background: %1; border: none; }"
                "QScrollBar:vertical { background: %2; width: 6px; border-radius: 3px; }"
                "QScrollBar::handle:vertical { background: %3; border-radius: 3px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                ).arg(BG_PRIMARY, BG_PRIMARY, CLR_BORDER)
    );

    m_bubbleContainer = new QWidget;
    m_bubbleContainer->setStyleSheet(QString("background: %1;").arg(BG_PRIMARY));
    m_bubbleLayout = new QVBoxLayout(m_bubbleContainer);
    m_bubbleLayout->setContentsMargins(16, 16, 16, 16);
    m_bubbleLayout->setSpacing(12);
    m_bubbleLayout->addStretch();

    m_scrollArea->setWidget(m_bubbleContainer);
    mainLayout->addWidget(m_scrollArea, 1);

    // ── 输入区域 ─────────────────────────────────────────────────────────────
    auto* inputBar = new QWidget(frame);
    inputBar->setFixedHeight(60);
    inputBar->setStyleSheet(
        QString("background: %1; border-radius: 0 0 12px 12px;").arg(BG_SECONDARY)
    );

    auto* inputLayout = new QHBoxLayout(inputBar);
    inputLayout->setContentsMargins(12, 10, 12, 10);
    inputLayout->setSpacing(10);

    m_input = new QLineEdit(inputBar);
    m_input->setPlaceholderText("输入您的问题，按 Enter 发送…");
    m_input->setStyleSheet(
        QString("QLineEdit {"
                "  background: %1; color: %2;"
                "  border: 1px solid %3; border-radius: 8px;"
                "  padding: 6px 12px; font-size: 13px;"
                "}"
                "QLineEdit:focus { border-color: %4; }"
                ).arg(BG_CARD, CLR_TXT_PRI, CLR_BORDER, CLR_ACCENT)
    );
    connect(m_input, &QLineEdit::returnPressed, this, &CustomerServiceDialog::onSendClicked);

    m_sendBtn = new QPushButton("发 送", inputBar);
    m_sendBtn->setFixedWidth(80);
    m_sendBtn->setStyleSheet(
        QString("QPushButton {"
                "  background: %1; color: #fff;"
                "  border: none; border-radius: 8px;"
                "  font-size: 13px; font-weight: 700; padding: 6px 0;"
                "}"
                "QPushButton:hover { background: #7DD3FC; }"
                "QPushButton:disabled { background: %2; color: %3; }"
                ).arg(CLR_ACCENT, BG_CARD, CLR_TXT_SEC)
    );
    connect(m_sendBtn, &QPushButton::clicked, this, &CustomerServiceDialog::onSendClicked);

    inputLayout->addWidget(m_input);
    inputLayout->addWidget(m_sendBtn);

    mainLayout->addWidget(inputBar);

    // ── 欢迎消息 ─────────────────────────────────────────────────────────────
    appendBubble(
        "您好！欢迎联系 FileTran 客服，很高兴为您服务！\n"
        "我可以解答关于以下内容的问题：\n"
        "• 支持的文件格式转换\n"
        "• 系统要求与安装问题\n"
        "• 会员套餐与价格\n"
        "• 退款政策\n"
        "• 常见使用问题\n\n"
        "请问有什么可以帮您？",
        false
    );

    m_input->setFocus();
}

// ── 添加气泡 ─────────────────────────────────────────────────────────────────

void CustomerServiceDialog::appendBubble(const QString& text, bool isUser, bool isThinking)
{
    // 气泡行容器
    auto* row = new QWidget(m_bubbleContainer);
    row->setStyleSheet("background: transparent;");
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    // 气泡文字标签
    auto* bubble = new QLabel(text, row);
    bubble->setWordWrap(true);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bubble->setMaximumWidth(480);

    QString bgColor   = isUser ? CLR_USER_BG  : (isThinking ? CLR_THINKING : CLR_BOT_BG);
    QString textColor = isThinking ? CLR_TXT_SEC : CLR_TXT_PRI;
    QString fontStyle = isThinking ? "font-style: italic;" : "";

    bubble->setStyleSheet(
        QString("QLabel {"
                "  background: %1; color: %2;"
                "  border-radius: 12px; padding: 10px 14px;"
                "  font-size: 13px; line-height: 1.6; %3"
                "  %4"
                "}").arg(
                    bgColor, textColor, fontStyle,
                    isUser ? "border-bottom-right-radius: 3px;" : "border-bottom-left-radius: 3px;"
                )
    );

    // 头像圆形标签
    auto* avatar = new QLabel(row);
    avatar->setFixedSize(32, 32);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        QString("QLabel {"
                "  background: %1; color: #fff;"
                "  border-radius: 16px; font-size: 12px; font-weight: 700;"
                "}").arg(isUser ? "#1D4ED8" : "#0369A1")
    );
    avatar->setText(isUser ? "我" : "客");

    if (isUser) {
        rowLayout->addStretch();
        rowLayout->addWidget(bubble);
        rowLayout->addWidget(avatar);
    } else {
        rowLayout->addWidget(avatar);
        rowLayout->addWidget(bubble);
        rowLayout->addStretch();
    }

    // 插入在 stretch 之前（stretch 始终在最后）
    int insertIdx = m_bubbleLayout->count() - 1;
    m_bubbleLayout->insertWidget(insertIdx, row);

    if (isThinking) {
        m_thinkingBubble = bubble;
    }

    scrollToBottom();
}

void CustomerServiceDialog::scrollToBottom()
{
    QTimer::singleShot(30, this, [this]() {
        m_scrollArea->verticalScrollBar()->setValue(
            m_scrollArea->verticalScrollBar()->maximum()
        );
    });
}

void CustomerServiceDialog::setInputEnabled(bool enabled)
{
    m_input->setEnabled(enabled);
    m_sendBtn->setEnabled(enabled);
    if (enabled) m_input->setFocus();
}

// ── 发送消息 ─────────────────────────────────────────────────────────────────

void CustomerServiceDialog::onSendClicked()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty() || !m_sendBtn->isEnabled()) return;

    m_input->clear();
    setInputEnabled(false);

    appendBubble(text, true);

    // 添加到历史
    QJsonObject userMsg;
    userMsg["role"]    = "user";
    userMsg["content"] = text;
    m_history.append(userMsg);

    // 截断历史（保留最近 20 条 = 10 轮）
    while (m_history.size() > 20) {
        m_history.removeFirst();
    }

    appendBubble("正在思考…", false, true);

    QJsonObject payload;
    payload["message"] = text;
    // 上传历史时去掉刚追加的本条（服务端会把 message 本身当作最新 user 消息）
    QJsonArray histForServer;
    for (int i = 0; i < m_history.size() - 1; ++i) {
        histForServer.append(m_history[i]);
    }
    payload["history"] = histForServer;

    ApiClient::instance().post(
        "support/chat/",
        payload,
        [this](bool ok, const QJsonObject& resp, const QString& err) {
            handleChatReply(ok, resp, err);
        }
    );
}

void CustomerServiceDialog::handleChatReply(bool ok, const QJsonObject& resp, const QString& err)
{
    QString reply;
    if (ok) {
        reply = resp.value("reply").toString().trimmed();
        if (reply.isEmpty()) reply = "抱歉，暂时无法获取回复，请稍后再试。";
    } else {
        reply = QString("网络请求失败：%1\n请检查网络连接，或拨打客服热线 18205678858。")
                    .arg(err.isEmpty() ? "未知错误" : err);
    }

    // 更新"正在思考…"气泡内容
    if (m_thinkingBubble) {
        m_thinkingBubble->setText(reply);
        m_thinkingBubble->setStyleSheet(
            QString("QLabel {"
                    "  background: %1; color: %2;"
                    "  border-radius: 12px; padding: 10px 14px;"
                    "  font-size: 13px; line-height: 1.6;"
                    "  border-bottom-left-radius: 3px;"
                    "}").arg(CLR_BOT_BG, CLR_TXT_PRI)
        );
        m_thinkingBubble = nullptr;
    }

    // 将回复追加到历史
    QJsonObject assistantMsg;
    assistantMsg["role"]    = "assistant";
    assistantMsg["content"] = reply;
    m_history.append(assistantMsg);
    while (m_history.size() > 20) m_history.removeFirst();

    setInputEnabled(true);
    scrollToBottom();
}

// ── 拖动支持 ─────────────────────────────────────────────────────────────────

void CustomerServiceDialog::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() < 50) {
        m_isDragging = true;
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void CustomerServiceDialog::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void CustomerServiceDialog::mouseReleaseEvent(QMouseEvent* event)
{
    m_isDragging = false;
    QDialog::mouseReleaseEvent(event);
}

void CustomerServiceDialog::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        hide();
        event->accept();
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // 拦截 Enter：防止 QDialog::keyPressEvent 调用 accept() 关闭对话框。
        // QLineEdit 的 returnPressed 信号已在按键到达此处之前处理发送逻辑。
        event->accept();
    } else {
        QDialog::keyPressEvent(event);
    }
}
