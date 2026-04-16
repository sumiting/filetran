#include "ForgotPasswordDialog.h"
#include "../network/ApiClient.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QMouseEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPen>

// ── 扁平线条图标（与 LoginDialog 相同风格）──────────────────────────────

static QIcon fpMakeFlatIconClose(const QColor& color, int sz = 14) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 2, Qt::SolidLine, Qt::RoundCap);
    p.setPen(pen);
    int m = int(sz * 0.22);
    p.drawLine(m, m, sz - m, sz - m);
    p.drawLine(sz - m, m, m, sz - m);
    p.end();
    return QIcon(pm);
}

// ── 构造 ──────────────────────────────────────────────────────────────

ForgotPasswordDialog::ForgotPasswordDialog(QWidget* parent) : QDialog(parent) {
    setFixedSize(340, 420);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    m_codeTimer = new QTimer(this);
    m_codeTimer->setInterval(1000);
    connect(m_codeTimer, &QTimer::timeout, this, &ForgotPasswordDialog::onCodeTick);

    setupUi();
}

// ── UI 布局 ──────────────────────────────────────────────────────────

void ForgotPasswordDialog::setupUi() {
    setStyleSheet(
        "QDialog{ background:#0d1117; font-family:\"Microsoft YaHei\",\"Segoe UI\",sans-serif; }"
        "QLineEdit{"
        "  background:#161b22; border:1px solid #21262d;"
        "  border-radius:6px; padding:8px 10px; font-size:13px; color:#c9d1d9;"
        "}"
        "QLineEdit:focus{ border-color:#00d4ff; }"
    );

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ── 头部 ─────────────────────────────────────────────────
    auto* header = new QFrame(this);
    header->setObjectName("fpHeader");
    header->setFixedHeight(50);
    header->setStyleSheet(
        "QFrame#fpHeader{"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #0d1e35,stop:1 #0d1117);"
        "  border-bottom:1px solid #21262d;"
        "}"
    );

    auto* hdrLay = new QHBoxLayout(header);
    hdrLay->setContentsMargins(20, 0, 12, 0);

    auto* titleLbl = new QLabel("找回密码", header);
    titleLbl->setStyleSheet("font-size:15px; font-weight:700; color:#e6edf3;");

    auto* closeBtn = new QPushButton(header);
    closeBtn->setFixedSize(28, 28);
    closeBtn->setToolTip("关闭");
    closeBtn->setIcon(fpMakeFlatIconClose(QColor("#E2E8F0")));
    closeBtn->setIconSize(QSize(14, 14));
    closeBtn->setStyleSheet(
        "QPushButton{ background:transparent; border:none; border-radius:8px; padding:0; }"
        "QPushButton:hover{ background:#EF4444; }");
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    hdrLay->addWidget(titleLbl);
    hdrLay->addStretch();
    hdrLay->addWidget(closeBtn);
    root->addWidget(header);

    // ── 内容 ─────────────────────────────────────────────────
    auto* content = new QWidget(this);
    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(20, 16, 20, 20);
    lay->setSpacing(10);

    // 提示语
    auto* hint = new QLabel("向账号绑定邮箱发送验证码，验证后可设置新密码。", content);
    hint->setWordWrap(true);
    hint->setStyleSheet("color:#8b949e; font-size:12px;");
    lay->addWidget(hint);

    // 用户名
    m_usernameEdit = new QLineEdit(content);
    m_usernameEdit->setPlaceholderText("用户名");
    lay->addWidget(m_usernameEdit);

    // 验证码行
    auto* codeRow = new QHBoxLayout;
    codeRow->setSpacing(8);
    m_codeEdit = new QLineEdit(content);
    m_codeEdit->setPlaceholderText("邮箱验证码");
    m_sendCodeBtn = new QPushButton("发送验证码", content);
    m_sendCodeBtn->setStyleSheet(
        "QPushButton{ background:#1a2332; color:#00c6a7;"
        "  border:1px solid rgba(0,198,167,0.25); border-radius:6px;"
        "  padding:8px; font-size:13px; }"
        "QPushButton:hover{ background:#223040; }"
        "QPushButton:disabled{ color:#64748B; border-color:#2D3F55; }");
    m_sendCodeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_sendCodeBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onSendCodeClicked);
    codeRow->addWidget(m_codeEdit);
    codeRow->addWidget(m_sendCodeBtn);
    lay->addLayout(codeRow);

    // 新密码
    m_newPwdEdit = new QLineEdit(content);
    m_newPwdEdit->setPlaceholderText("新密码（至少8位）");
    m_newPwdEdit->setEchoMode(QLineEdit::Password);
    lay->addWidget(m_newPwdEdit);

    // 确认密码
    m_confirmPwdEdit = new QLineEdit(content);
    m_confirmPwdEdit->setPlaceholderText("确认新密码");
    m_confirmPwdEdit->setEchoMode(QLineEdit::Password);
    lay->addWidget(m_confirmPwdEdit);

    // 状态提示
    m_statusLabel = new QLabel("", content);
    m_statusLabel->setStyleSheet("color:#ff6b6b; font-size:12px;");
    m_statusLabel->setWordWrap(true);
    lay->addWidget(m_statusLabel);

    lay->addStretch();

    // 重置按钮
    m_resetBtn = new QPushButton("重置密码", content);
    m_resetBtn->setFixedHeight(40);
    m_resetBtn->setCursor(Qt::PointingHandCursor);
    m_resetBtn->setStyleSheet(
        "QPushButton{"
        "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0d6efd,stop:1 #00c6a7);"
        "  color:#fff; font-weight:700; font-size:14px;"
        "  border:none; border-radius:8px;"
        "}"
        "QPushButton:hover{ opacity:0.9; }"
        "QPushButton:disabled{ background:#2D3F55; color:#64748B; }");
    connect(m_resetBtn, &QPushButton::clicked, this, &ForgotPasswordDialog::onResetClicked);
    lay->addWidget(m_resetBtn);

    root->addWidget(content);
}

// ── 发送验证码 ────────────────────────────────────────────────────────

void ForgotPasswordDialog::onSendCodeClicked() {
    QString user = m_usernameEdit->text().trimmed();
    if (user.isEmpty()) {
        m_statusLabel->setStyleSheet("color:#ff6b6b; font-size:12px;");
        m_statusLabel->setText("请先输入用户名");
        return;
    }
    m_statusLabel->clear();
    m_sendCodeBtn->setEnabled(false);

    QJsonObject req;
    req["username"] = user;
    ApiClient::instance().post("auth/forgot-password/send-code/", req,
        [this](bool ok, const QJsonObject& resp, const QString& err) {
            handleSendCodeResponse(ok, resp, err);
        });
}

void ForgotPasswordDialog::handleSendCodeResponse(bool ok, const QJsonObject&, const QString& err) {
    if (!ok) {
        m_statusLabel->setStyleSheet("color:#ff6b6b; font-size:12px;");
        m_statusLabel->setText("发送失败: " + err);
        m_sendCodeBtn->setEnabled(true);
        return;
    }
    m_statusLabel->setStyleSheet("color:#34D399; font-size:12px;");
    m_statusLabel->setText("验证码已发送至注册邮箱");
    m_countdown = 60;
    m_codeTimer->start();
    onCodeTick();
}

void ForgotPasswordDialog::onCodeTick() {
    if (m_countdown <= 0) {
        m_codeTimer->stop();
        m_sendCodeBtn->setEnabled(true);
        m_sendCodeBtn->setText("发送验证码");
        return;
    }
    m_sendCodeBtn->setText(QString("%1s").arg(m_countdown));
    --m_countdown;
}

// ── 重置密码 ──────────────────────────────────────────────────────────

void ForgotPasswordDialog::onResetClicked() {
    QString user    = m_usernameEdit->text().trimmed();
    QString code    = m_codeEdit->text().trimmed();
    QString newPwd  = m_newPwdEdit->text();
    QString confPwd = m_confirmPwdEdit->text();

    m_statusLabel->setStyleSheet("color:#ff6b6b; font-size:12px;");

    if (user.isEmpty() || code.isEmpty() || newPwd.isEmpty()) {
        m_statusLabel->setText("请填写用户名、验证码和新密码");
        return;
    }
    if (newPwd.length() < 8) {
        m_statusLabel->setText("新密码至少8位");
        return;
    }
    if (newPwd != confPwd) {
        m_statusLabel->setText("两次新密码不一致");
        return;
    }

    m_resetBtn->setEnabled(false);
    m_resetBtn->setText("提交中...");
    m_statusLabel->clear();

    QJsonObject req;
    req["username"]     = user;
    req["code"]         = code;
    req["new_password"] = newPwd;
    ApiClient::instance().post("auth/forgot-password/reset/", req,
        [this](bool ok, const QJsonObject& resp, const QString& err) {
            handleResetResponse(ok, resp, err);
        });
}

void ForgotPasswordDialog::handleResetResponse(bool ok, const QJsonObject&, const QString& err) {
    m_resetBtn->setEnabled(true);
    m_resetBtn->setText("重置密码");
    if (!ok) {
        m_statusLabel->setStyleSheet("color:#ff6b6b; font-size:12px;");
        m_statusLabel->setText("重置失败: " + err);
        return;
    }
    QMessageBox::information(this, "成功", "密码已重置，请用新密码登录。");
    accept();
}

// ── 拖动 ─────────────────────────────────────────────────────────────

void ForgotPasswordDialog::mousePressEvent(QMouseEvent* e) {
    if (e->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPos = e->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}
void ForgotPasswordDialog::mouseMoveEvent(QMouseEvent* e) {
    if (m_isDragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPosition().toPoint() - m_dragPos);
}
void ForgotPasswordDialog::mouseReleaseEvent(QMouseEvent*) {
    m_isDragging = false;
}
