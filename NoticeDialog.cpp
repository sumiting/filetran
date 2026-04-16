#include "NoticeDialog.h"
#include "../core/NoticeManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBrowser>
#include <QMouseEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>

// ── 颜色常量（与 MainWindow 保持一致）──────────────────────────────────
static const QString BG_DIALOG  = "#0B1220";
static const QString BG_HEADER  = "#0F1A2E";
static const QString BG_BODY    = "#141C2B";
static const QString CLR_ACCENT = "#38BDF8";
static const QString CLR_TXT    = "#F1F5F9";
static const QString CLR_SEC    = "#94A3B8";
static const QString CLR_BORDER = "#2A3A52";
static const QString CLR_CLOSE  = "#EF4444";

// ── 构造 ─────────────────────────────────────────────────────────────────

NoticeDialog::NoticeDialog(int noticeId,
                           const QString& title,
                           const QString& content,
                           QWidget* parent)
    : QDialog(parent)
    , m_noticeId(noticeId)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(false);
    setFixedWidth(480);

    setupUi(title, content);

    // 投影效果
    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setOffset(0, 4);
    shadow->setColor(QColor(0, 0, 0, 160));
    if (auto* w = findChild<QWidget*>("NoticeContainer"))
        w->setGraphicsEffect(shadow);
}

// ── UI 构建 ───────────────────────────────────────────────────────────────

void NoticeDialog::setupUi(const QString& title, const QString& content) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 16, 16, 16);

    // ── 卡片容器 ─────────────────────────────────────────────────────
    auto* card = new QWidget(this);
    card->setObjectName("NoticeContainer");
    card->setStyleSheet(QString(
        "#NoticeContainer {"
        "  background: %1;"
        "  border: 1px solid %2;"
        "  border-radius: 10px;"
        "}").arg(BG_DIALOG, CLR_BORDER));
    outer->addWidget(card);

    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(0, 0, 0, 0);
    cardLay->setSpacing(0);

    // ── 标题栏 ────────────────────────────────────────────────────────
    auto* header = new QWidget(card);
    header->setFixedHeight(48);
    header->setStyleSheet(QString(
        "background: %1;"
        "border-top-left-radius: 10px;"
        "border-top-right-radius: 10px;"
        "border-bottom: 1px solid %2;").arg(BG_HEADER, CLR_BORDER));
    cardLay->addWidget(header);

    auto* headerLay = new QHBoxLayout(header);
    headerLay->setContentsMargins(16, 0, 12, 0);

    // 图标 + 标题
    auto* iconLbl = new QLabel("📢", header);
    iconLbl->setStyleSheet("font-size: 16px; background: transparent;");

    auto* titleLbl = new QLabel(title, header);
    titleLbl->setStyleSheet(QString(
        "font-size: 14px; font-weight: 700; color: %1; background: transparent;").arg(CLR_TXT));
    titleLbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // 关闭按钮
    auto* closeBtn = new QPushButton("✕", header);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(QString(
        "QPushButton {"
        "  background: transparent; color: %1;"
        "  border: none; border-radius: 4px; font-size: 13px;"
        "}"
        "QPushButton:hover { background: rgba(239,68,68,0.18); }").arg(CLR_SEC));
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    headerLay->addWidget(iconLbl);
    headerLay->addSpacing(6);
    headerLay->addWidget(titleLbl);
    headerLay->addWidget(closeBtn);

    // ── 内容区 ────────────────────────────────────────────────────────
    auto* bodyWidget = new QWidget(card);
    bodyWidget->setStyleSheet(QString("background: %1;").arg(BG_BODY));
    cardLay->addWidget(bodyWidget);

    auto* bodyLay = new QVBoxLayout(bodyWidget);
    bodyLay->setContentsMargins(20, 16, 20, 16);

    auto* contentBrowser = new QTextBrowser(bodyWidget);
    contentBrowser->setPlainText(content);
    contentBrowser->setReadOnly(true);
    contentBrowser->setMinimumHeight(120);
    contentBrowser->setMaximumHeight(320);
    contentBrowser->setStyleSheet(QString(
        "QTextBrowser {"
        "  background: transparent; color: %1;"
        "  font-size: 13px; line-height: 1.6;"
        "  border: none; padding: 0;"
        "}"
        "QScrollBar:vertical {"
        "  background: %2; width: 6px; border-radius: 3px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: %3; border-radius: 3px; min-height: 20px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ).arg(CLR_TXT, BG_DIALOG, CLR_BORDER));
    contentBrowser->document()->setDocumentMargin(0);
    bodyLay->addWidget(contentBrowser);

    // ── 分隔线 ────────────────────────────────────────────────────────
    auto* sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QString("background: %1; border: none;").arg(CLR_BORDER));
    sep->setFixedHeight(1);
    cardLay->addWidget(sep);

    // ── 底部按钮栏 ────────────────────────────────────────────────────
    auto* footer = new QWidget(card);
    footer->setStyleSheet(QString(
        "background: %1;"
        "border-bottom-left-radius: 10px;"
        "border-bottom-right-radius: 10px;").arg(BG_HEADER));
    footer->setFixedHeight(56);
    cardLay->addWidget(footer);

    auto* footerLay = new QHBoxLayout(footer);
    footerLay->setContentsMargins(20, 0, 20, 0);
    footerLay->setSpacing(12);
    footerLay->addStretch();

    // 关闭（不标记已读）
    auto* laterBtn = new QPushButton("关闭", footer);
    laterBtn->setFixedSize(88, 34);
    laterBtn->setCursor(Qt::PointingHandCursor);
    laterBtn->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: %2;"
        "  border: 1px solid %3; border-radius: 6px; font-size: 13px;"
        "}"
        "QPushButton:hover { background: #1E293B; }").arg(BG_BODY, CLR_SEC, CLR_BORDER));
    connect(laterBtn, &QPushButton::clicked, this, &QDialog::reject);

    // 不再展示（标记已读）
    auto* dismissBtn = new QPushButton("不再展示", footer);
    dismissBtn->setFixedSize(100, 34);
    dismissBtn->setCursor(Qt::PointingHandCursor);
    dismissBtn->setStyleSheet(QString(
        "QPushButton {"
        "  background: %1; color: #0F1A2E;"
        "  border: none; border-radius: 6px;"
        "  font-size: 13px; font-weight: 600;"
        "}"
        "QPushButton:hover { background: %2; }").arg(CLR_ACCENT, CLR_ACCENT));
    connect(dismissBtn, &QPushButton::clicked, this, &NoticeDialog::onDismissClicked);

    footerLay->addWidget(laterBtn);
    footerLay->addWidget(dismissBtn);
}

// ── 槽函数 ────────────────────────────────────────────────────────────────

void NoticeDialog::onDismissClicked() {
    NoticeManager::instance().markRead(m_noticeId);
    accept();
}

// ── 拖动支持 ──────────────────────────────────────────────────────────────

void NoticeDialog::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}

void NoticeDialog::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        move(event->globalPosition().toPoint() - m_dragPos);
        event->accept();
    }
}

void NoticeDialog::mouseReleaseEvent(QMouseEvent* event) {
    m_isDragging = false;
    event->accept();
}
