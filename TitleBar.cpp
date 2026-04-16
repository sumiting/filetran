#include "TitleBar.h"
#include "../user/UserManager.h"

#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QMenu>
#include <QWidgetAction>
#include <QFrame>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QDebug>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QScreen>
#include <QJsonValue>

// ── 颜色常量 ─────────────────────────────────────────────────────────
static const QColor CLR_BG     (0x0F, 0x17, 0x2A);  // 主背景（与 main 全局主题一致）
static const QColor CLR_ACCENT (0x00, 0xB4, 0xD8);  // 青色强调色
static const QColor CLR_SEP    (0x1E, 0x29, 0x3B);  // 分隔线
// 与 CMake POST_BUILD 同步：<exe 目录>/resources/user_center.svg
static QString userIconSvgPath() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/resources/user_center.svg");
}

// ── 绘制账户图标（用户头像 + 外圆，更符合账号语义） ─────────────────────────

QIcon TitleBar::makeAccountIcon(int sz, const QColor& color) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    QPen pen(color, qMax(1.4, sz * 0.09), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const double cx = sz / 2.0;
    const double cy = sz / 2.0;
    const double ringR = sz * 0.46;

    // 外圆环
    p.drawEllipse(QPointF(cx, cy), ringR, ringR);

    // 头像（头）
    const double headR = sz * 0.16;
    p.drawEllipse(QPointF(cx, sz * 0.37), headR, headR);

    // 肩部弧线
    const QRectF shoulderRect(sz * 0.24, sz * 0.50, sz * 0.52, sz * 0.34);
    p.drawArc(shoulderRect, 30 * 16, 120 * 16);

    p.end();
    return QIcon(pm);
}

// ── 绘制窗口控制图标（简洁线条） ────────────────────────────────────────────

QIcon TitleBar::makeWinIcon(const QString& type, int sz, const QColor& color) {
    QPixmap pm(sz, sz);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);

    int m = sz / 4;
    if (type == "min") {
        // 水平横线（居中偏下）
        int y = sz * 11 / 16;
        p.drawLine(m, y, sz - m, y);
    } else if (type == "max") {
        // 空心矩形
        p.drawRect(QRectF(m, m, sz - 2*m, sz - 2*m));
    } else if (type == "restore") {
        // 两个错位矩形（还原图标）
        int off = sz / 6;
        p.drawRect(QRectF(m + off, m, sz - 2*m - off, sz - 2*m - off));
        pen.setWidth(1);
        p.setPen(pen);
        p.drawRect(QRectF(m, m + off, sz - 2*m - off, sz - 2*m - off));
    } else if (type == "close") {
        // 斜叉
        p.drawLine(m, m, sz - m, sz - m);
        p.drawLine(sz - m, m, m, sz - m);
    }
    p.end();
    return QIcon(pm);
}

static QIcon loadUserIconOrFallback() {
    const QString path = userIconSvgPath();
    if (QFile::exists(path)) {
        QIcon icon(path);
        if (!icon.isNull()) return icon;
    }
    return QIcon();
}

// ── 构造 ─────────────────────────────────────────────────────────────

TitleBar::TitleBar(QWidget* parent) : QWidget(parent) {
    setFixedHeight(40);
    setAttribute(Qt::WA_StyledBackground, true);
    setupUi();
}

void TitleBar::setupUi() {
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 0, 0, 0);
    lay->setSpacing(0);

    // ── Logo + 标题 ────────────────────────────────────────────────
    // 尝试从 png 加载图标，如失败则用绘制图标
    QPixmap logoPixmap(QCoreApplication::applicationDirPath() + QStringLiteral("/resources/logo.png"));
    m_logoLabel = new QLabel(this);
    m_logoLabel->setFixedSize(20, 20);
    if (!logoPixmap.isNull()) {
        m_logoLabel->setPixmap(
            logoPixmap.scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        m_logoLabel->setText("✦");
        m_logoLabel->setStyleSheet("color:#38BDF8; font-size:14px;");
        m_logoLabel->setAlignment(Qt::AlignCenter);
    }

    m_titleLabel = new QLabel("FileTran", this);
    m_titleLabel->setStyleSheet(
        "color:#E2E8F0; font-size:13px; font-weight:bold; margin-left:6px;");

    lay->addWidget(m_logoLabel);
    lay->addWidget(m_titleLabel);
    lay->addStretch();

    // ── 圆形头像/登录按钮 ───────────────────────────────────────────
    m_avatarBtn = new QPushButton(this);
    m_avatarBtn->setFixedSize(32, 32);
    m_avatarBtn->setCursor(Qt::PointingHandCursor);
    QIcon userIcon = loadUserIconOrFallback();
    m_avatarBtn->setIcon(userIcon.isNull() ? makeAccountIcon(18, Qt::white) : userIcon);
    m_avatarBtn->setIconSize(QSize(18, 18));
    m_avatarBtn->setStyleSheet(
        "QPushButton{"
        "  background:#38BDF8; border:none; border-radius:16px;"
        "  color:#fff; font-weight:bold; font-size:13px;"
        "}"
        "QPushButton:hover{ background:#0096B7; }"
        "QPushButton:pressed{ background:#007A96; }");
    m_avatarBtn->setToolTip("登录");
    connect(m_avatarBtn, &QPushButton::clicked, this, &TitleBar::onAvatarClicked);
    lay->addWidget(m_avatarBtn);
    lay->addSpacing(4);

    // ── 汉堡菜单 ────────────────────────────────────────────────────
    m_hamburgerBtn = new QPushButton(this);
    m_hamburgerBtn->setFixedSize(36, 40);
    m_hamburgerBtn->setCursor(Qt::PointingHandCursor);
    m_hamburgerBtn->setToolTip("菜单");
    // 用文字 ≡ 表示汉堡菜单
    m_hamburgerBtn->setText("≡");
    m_hamburgerBtn->setStyleSheet(
        "QPushButton{"
        "  border:none; background:transparent;"
        "  color:#94A3B8; font-size:18px; line-height:40px;"
        "}"
        "QPushButton:hover{ background:#1E293B; color:#E2E8F0; }");
    connect(m_hamburgerBtn, &QPushButton::clicked, this, &TitleBar::showHamburgerMenu);
    lay->addWidget(m_hamburgerBtn);

    // ── 分隔间距 ──────────────────────────────────────────────────
    lay->addSpacing(2);

    // ── 窗口控制按钮 ─────────────────────────────────────────────────
    static const QColor ICON_CLR("#94A3B8");
    const int BTN_W = 46;
    const int BTN_H = 40;

    m_minBtn = new QPushButton(this);
    m_minBtn->setFixedSize(BTN_W, BTN_H);
    m_minBtn->setIcon(makeWinIcon("min", 14, ICON_CLR));
    m_minBtn->setIconSize(QSize(14, 14));
    m_minBtn->setStyleSheet(
        "QPushButton{ border:none; background:transparent; }"
        "QPushButton:hover{ background:#2D3F55; }");
    m_minBtn->setCursor(Qt::PointingHandCursor);
    connect(m_minBtn, &QPushButton::clicked, this, &TitleBar::minimizeRequested);

    m_maxBtn = new QPushButton(this);
    m_maxBtn->setFixedSize(BTN_W, BTN_H);
    m_maxBtn->setIcon(makeWinIcon("max", 14, ICON_CLR));
    m_maxBtn->setIconSize(QSize(14, 14));
    m_maxBtn->setStyleSheet(
        "QPushButton{ border:none; background:transparent; }"
        "QPushButton:hover{ background:#2D3F55; }");
    m_maxBtn->setCursor(Qt::PointingHandCursor);
    connect(m_maxBtn, &QPushButton::clicked, this, &TitleBar::maximizeRequested);

    m_closeBtn = new QPushButton(this);
    m_closeBtn->setFixedSize(BTN_W, BTN_H);
    m_closeBtn->setIcon(makeWinIcon("close", 14, ICON_CLR));
    m_closeBtn->setIconSize(QSize(14, 14));
    m_closeBtn->setStyleSheet(
        "QPushButton{ border:none; background:transparent; }"
        "QPushButton:hover{ background:#E81123; }");
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    connect(m_closeBtn, &QPushButton::clicked, this, &TitleBar::closeRequested);

    lay->addWidget(m_minBtn);
    lay->addWidget(m_maxBtn);
    lay->addWidget(m_closeBtn);
}

// ── 状态切换 ──────────────────────────────────────────────────────────

void TitleBar::setLoginState() {
    m_isLoggedIn = false;
    m_username.clear();
    m_userType.clear();
    m_userId.clear();
    m_membershipExpireText.clear();
    m_isLifetimeMember = false;
    m_avatarBtn->setText("");
    QIcon userIcon = loadUserIconOrFallback();
    m_avatarBtn->setIcon(userIcon.isNull() ? makeAccountIcon(18, Qt::white) : userIcon);
    m_avatarBtn->setIconSize(QSize(18, 18));
    m_avatarBtn->setStyleSheet(
        "QPushButton{"
        "  background:#38BDF8; border:none; border-radius:16px;"
        "  color:#fff; font-weight:bold; font-size:13px;"
        "}"
        "QPushButton:hover{ background:#0096B7; }"
        "QPushButton:pressed{ background:#007A96; }");
    m_avatarBtn->setToolTip("点击登录");
}

void TitleBar::setUserState(const QJsonObject& userInfo) {
    m_isLoggedIn = true;
    m_username   = userInfo.value("username").toString("U");
    m_userId     = QString::number(userInfo.value("id").toInt(0));
    m_membershipExpireText.clear();
    m_isLifetimeMember = false;

    // 从 membership.plan_type 读取会员类型（服务端返回格式）
    QJsonObject membership = userInfo.value("membership").toObject();
    m_userType = "normal";
    if (!membership.isEmpty()) {
        bool isActive = membership.value("is_active").toBool(false) ||
                        membership.value("is_lifetime").toBool(false);
        m_isLifetimeMember = membership.value("is_lifetime").toBool(false) ||
                             membership.value("plan_type").toString() == "lifetime";
        if (isActive) m_userType = membership.value("plan_type").toString("normal");
        // 服务端字段为 end_date（ISO 格式），兼容旧字段名
        QString rawDate = membership.value("end_date").toString(
            membership.value("expire_time").toString(
                membership.value("expires_at").toString(
                    membership.value("expire_at").toString())));
        // 将 ISO 日期/时间截取为 YYYY-MM-DD 展示
        if (!rawDate.isEmpty())
            m_membershipExpireText = rawDate.left(10);
        else
            m_membershipExpireText.clear();
    }

    bool isPaid  = (m_userType != "normal" && m_userType != "guest");
    QColor avatarBg = isPaid ? QColor("#F59E0B") : CLR_ACCENT;

    QString initial = m_username.left(1).toUpper();
    m_avatarBtn->setIcon(QIcon());
    m_avatarBtn->setText(initial);
    m_avatarBtn->setIconSize(QSize(0, 0));
    m_avatarBtn->setStyleSheet(QString(
        "QPushButton{"
        "  background:%1; border:none; border-radius:16px;"
        "  color:#fff; font-weight:bold; font-size:14px;"
        "}"
        "QPushButton:hover{ background:%2; }"
        "QPushButton:pressed{ background:%2; }").arg(
            avatarBg.name(),
            avatarBg.darker(120).name()));
    m_avatarBtn->setToolTip(m_username);
}

void TitleBar::setMaximized(bool maximized) {
    m_maximized = maximized;
    static const QColor ICON_CLR("#94A3B8");
    m_maxBtn->setIcon(makeWinIcon(maximized ? "restore" : "max", 14, ICON_CLR));
}

// ── 头像按钮点击 ──────────────────────────────────────────────────────

void TitleBar::onAvatarClicked() {
    if (m_isLoggedIn) {
        UserManager::instance().fetchProfile(); // 后台静默刷新，保证下次打开时数据最新
        showUserDropdown();
    } else {
        emit loginClicked();
    }
}

// ── 汉堡 / 用户下拉菜单 ───────────────────────────────────────────────

void TitleBar::showHamburgerMenu() {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu{ background:#1E293B; border:1px solid #2D3F55; color:#E2E8F0;"
        "  border-radius:6px; padding:4px; }"
        "QMenu::item{ padding:8px 20px; border-radius:4px; font-size:13px; }"
        "QMenu::item:selected{ background:#2D3F55; }"
        "QMenu::separator{ height:1px; background:#2D3F55; margin:4px 8px; }");

    menu.addAction("软件更新",  [this]{ emit menuActionTriggered("update"); });
    menu.addAction("意见反馈",  [this]{ emit menuActionTriggered("feedback"); });
    menu.addAction("联系客服",  [this]{ emit menuActionTriggered("customer_service"); });
    menu.addSeparator();
    menu.addAction("关于软件",  [this]{ emit menuActionTriggered("about"); });
    menu.addSeparator();
    menu.addAction("退出",      [this]{ emit menuActionTriggered("exit"); });

    QPoint pos = m_hamburgerBtn->mapToGlobal(
        QPoint(0, m_hamburgerBtn->height()));
    menu.exec(pos);
}

void TitleBar::showUserDropdown() {
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu{ background:#0F172A; border:1px solid #1E293B; border-radius:10px; padding:0; }");

    static const QMap<QString, QString> typeNames = {
        {"normal",    "普通用户"}, {"guest",     "普通用户"},
        {"monthly",   "月度会员"}, {"quarterly", "季度会员"},
        {"yearly",    "年度会员"}, {"lifetime",  "终身会员"},
    };
    static const QMap<QString, int> deviceLimits = {
        {"normal",    1}, {"guest",    1},
        {"monthly",   1}, {"quarterly", 2},
        {"yearly",    3}, {"lifetime",  3},
    };

    // 每次打开下拉菜单时从 UserManager 实时读取，避免因 fetchProfile 尚未触发
    // setUserState 而显示滞后的缓存数据
    const QJsonObject liveUser       = UserManager::instance().currentUser();
    const QJsonObject liveMembership = liveUser.value("membership").toObject();

    // 解析会员状态（与 setUserState 逻辑保持一致）
    QString liveUserType;
    bool    liveIsLifetime = false;
    QString liveExpireText;
    if (!liveMembership.isEmpty()) {
        const bool isActive = liveMembership.value("is_active").toBool(false)
                           || liveMembership.value("is_lifetime").toBool(false);
        liveIsLifetime = liveMembership.value("is_lifetime").toBool(false)
                      || liveMembership.value("plan_type").toString() == "lifetime";
        if (isActive) liveUserType = liveMembership.value("plan_type").toString("normal");
        const QString rawDate = liveMembership.value("end_date").toString(
            liveMembership.value("expire_time").toString(
                liveMembership.value("expires_at").toString(
                    liveMembership.value("expire_at").toString())));
        if (!rawDate.isEmpty()) liveExpireText = rawDate.left(10);
    }
    if (liveUserType.isEmpty()) liveUserType = "normal";

    // 若 UserManager 尚无数据（liveUser 为空），回退到 TitleBar 缓存值
    const QString activeUserType   = liveUser.isEmpty() ? m_userType           : liveUserType;
    const bool    activeIsLifetime = liveUser.isEmpty() ? m_isLifetimeMember   : liveIsLifetime;
    const QString activeExpire     = liveUser.isEmpty() ? m_membershipExpireText : liveExpireText;
    const QString activeUsername   = liveUser.isEmpty() ? m_username
                                    : liveUser.value("username").toString(m_username);
    const QString activeUserId     = liveUser.isEmpty() ? m_userId
                                    : QString::number(liveUser.value("id").toInt(0));

    const QString typeName   = typeNames.value(activeUserType, "普通用户");
    const int     devLimit   = deviceLimits.value(activeUserType, 1);
    const bool needOpenVip   = (activeUserType == "normal" || activeUserType == "guest");
    const bool canUpgrade    = (!needOpenVip && !activeIsLifetime);
    const QString expireText = activeIsLifetime
        ? QStringLiteral("永久")
        : (activeExpire.isEmpty() ? QStringLiteral("--") : activeExpire);

    auto* panel = new QFrame(&menu);
    panel->setFixedWidth(320);   // 高度由内容自动决定，不写死
    panel->setStyleSheet(
        "QFrame{"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #131E2E,stop:1 #0D1525);"
        "  border:none; border-radius:10px; color:#E2E8F0;"
        "}"
        "QLabel#udName{ color:#F1F5F9; font-size:15px; font-weight:700; }"
        "QLabel#udMeta{ color:#64748B; font-size:12px; }"
        "QLabel#udTypeBadge{"
        "  color:#38BDF8; font-size:13px; font-weight:600;"
        "  background:rgba(56,189,248,0.12); border:1px solid rgba(56,189,248,0.3);"
        "  border-radius:4px; padding:2px 8px;"
        "}"
        "QLabel#udTypeBadgePro{"
        "  color:#FBBF24; font-size:13px; font-weight:600;"
        "  background:rgba(251,191,36,0.12); border:1px solid rgba(251,191,36,0.3);"
        "  border-radius:4px; padding:2px 8px;"
        "}"
        "QFrame#udCard{ background:#1A2332; border:1px solid #253245; border-radius:8px; }"
        "QLabel#udCardKey{ color:#64748B; font-size:13px; }"
        "QLabel#udCardVal{ color:#CBD5E1; font-size:13px; font-weight:500; }"
        "QLabel#udRightsTitle{ color:#94A3B8; font-size:12px; }"
        "QLabel#udRights{ color:#94A3B8; font-size:12px; }"
        "QPushButton#udVipBtn{"
        "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #F59E0B,stop:1 #F97316);"
        "  color:#fff; border:none; border-radius:5px;"
        "  font-size:12px; font-weight:700; padding:4px 12px;"
        "}"
        "QPushButton#udVipBtn:hover{ background:#D97706; }"
        "QPushButton#udLogout{"
        "  color:#94A3B8; border:1px solid #334155; background:transparent;"
        "  font-size:12px; border-radius:4px; padding:3px 10px;"
        "}"
        "QPushButton#udLogout:hover{ color:#E2E8F0; border-color:#60A5FA; }");

    auto* root = new QVBoxLayout(panel);
    root->setContentsMargins(14, 14, 14, 14);
    root->setSpacing(10);

    // ── 顶行：头像 + 用户名/ID + 退出 ─────────────────────────────────
    auto* topRow = new QHBoxLayout();
    topRow->setSpacing(8);

    auto* avatar = new QLabel(panel);
    avatar->setFixedSize(44, 44);
    avatar->setAlignment(Qt::AlignCenter);
    avatar->setStyleSheet(
        "background:#334155; border-radius:22px;"
        "color:#ffffff; font-size:17px; font-weight:700;");
    avatar->setText(activeUsername.isEmpty() ? "U" : activeUsername.left(1).toUpper());
    topRow->addWidget(avatar);

    auto* userCol = new QVBoxLayout();
    userCol->setSpacing(2);
    userCol->setContentsMargins(0, 0, 0, 0);
    auto* nameLbl = new QLabel(activeUsername, panel);
    nameLbl->setObjectName("udName");
    auto* idLbl = new QLabel(QString("ID: %1").arg(activeUserId.isEmpty() ? "--" : activeUserId), panel);
    idLbl->setObjectName("udMeta");
    userCol->addWidget(nameLbl);
    userCol->addWidget(idLbl);
    topRow->addLayout(userCol, 1);

    // 退出按钮：小边框，明显可见
    auto* logoutBtn = new QPushButton("退出登录", panel);
    logoutBtn->setObjectName("udLogout");
    logoutBtn->setCursor(Qt::PointingHandCursor);
    QObject::connect(logoutBtn, &QPushButton::clicked, &menu, [this, &menu]{
        menu.close();
        emit logoutRequested();
    });
    topRow->addWidget(logoutBtn, 0, Qt::AlignVCenter);
    root->addLayout(topRow);

    // ── 分割线 ─────────────────────────────────────────────────────────
    auto* sep = new QFrame(panel);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame{ background:#1E293B; border:none; max-height:1px; }");
    root->addWidget(sep);

    // ── 会员信息卡片 ────────────────────────────────────────────────────
    // 卡片内边距与 root 水平边距相同(0)，保证内容左对齐
    auto* card = new QFrame(panel);
    card->setObjectName("udCard");
    auto* cardLay = new QVBoxLayout(card);
    cardLay->setContentsMargins(12, 9, 12, 9);
    cardLay->setSpacing(7);

    // 行1：类型 badge ＋ 升级按钮（右对齐、紧凑）
    auto* cardRow1 = new QHBoxLayout();
    cardRow1->setSpacing(6);
    auto* typeBadge = new QLabel(typeName, card);
    typeBadge->setObjectName(m_isLifetimeMember ? "udTypeBadgePro" : "udTypeBadge");
    cardRow1->addWidget(typeBadge);
    cardRow1->addStretch();
    if (needOpenVip || canUpgrade) {
        auto* vipBtn = new QPushButton(needOpenVip ? "立即开通" : "升级套餐", card);
        vipBtn->setObjectName("udVipBtn");
        vipBtn->setCursor(Qt::PointingHandCursor);
        vipBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        QObject::connect(vipBtn, &QPushButton::clicked, &menu, [this, &menu]{
            menu.close();
            emit upgradeRequested();
        });
        cardRow1->addWidget(vipBtn);
    }
    cardLay->addLayout(cardRow1);

    // 行2：有效期 ＋ 可绑定设备（同一行，key 灰色 val 亮色）
    auto* cardRow2 = new QHBoxLayout();
    cardRow2->setSpacing(4);
    auto* expireKeyLbl = new QLabel("有效期至", card);
    expireKeyLbl->setObjectName("udCardKey");
    auto* expireValLbl = new QLabel(expireText, card);
    expireValLbl->setObjectName("udCardVal");
    cardRow2->addWidget(expireKeyLbl);
    cardRow2->addWidget(expireValLbl);
    cardRow2->addSpacing(14);
    auto* devKeyLbl = new QLabel("可绑定设备", card);
    devKeyLbl->setObjectName("udCardKey");
    auto* devValLbl = new QLabel(QString("%1 台").arg(devLimit), card);
    devValLbl->setObjectName("udCardVal");
    cardRow2->addWidget(devKeyLbl);
    cardRow2->addWidget(devValLbl);
    cardRow2->addStretch();
    cardLay->addLayout(cardRow2);

    root->addWidget(card);

    // ── 权益区（与卡片左对齐：margin 均为 12px）────────────────────────
    QString rightsTitleText;
    if (m_isLifetimeMember) {
        rightsTitleText = "✨ 尊贵的终身会员，您已享受全部会员权益";
    } else if (canUpgrade) {
        rightsTitleText = "升级会员使用全部功能，尊享特权";
    } else {
        rightsTitleText = "开通会员使用全部功能，尊享特权";
    }
    auto* rightsTitle = new QLabel(rightsTitleText, panel);
    rightsTitle->setObjectName("udRightsTitle");
    rightsTitle->setWordWrap(true);
    root->addWidget(rightsTitle);

    auto* rights = new QLabel(
        "∞ 无限制次数  📑 多格式互转  🔄 高清转换\n"
        "🚫 免除广告  ☎ 专属客服  🖥 绑定多台设备",
        panel);
    rights->setObjectName("udRights");
    rights->setWordWrap(true);
    root->addWidget(rights);

    auto* wa = new QWidgetAction(&menu);
    wa->setDefaultWidget(panel);
    menu.addAction(wa);

    QPoint pos = m_avatarBtn->mapToGlobal(QPoint(0, m_avatarBtn->height() + 4));
    menu.exec(pos);
}

// ── paintEvent ────────────────────────────────────────────────────────

void TitleBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    // 参考 PyQt：顶部轻微渐变背景（#1E293B -> #0F172A）
    QLinearGradient grad(0, 0, 0, height());
    grad.setColorAt(0.0, QColor("#1E293B"));
    grad.setColorAt(1.0, QColor("#0F172A"));
    p.fillRect(rect(), grad);

    // 参考 PyQt：底部分隔线（轻微青色透明）
    p.setPen(QPen(QColor("#38BDF820"), 1));
    p.drawLine(0, height() - 1, width(), height() - 1);
}

// ── 窗口拖动 ─────────────────────────────────────────────────────────

void TitleBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragPos = event->globalPosition().toPoint() - window()->frameGeometry().topLeft();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent* event) {
    if (m_isDragging && (event->buttons() & Qt::LeftButton)) {
        if (m_maximized) {
            emit maximizeRequested();
            m_isDragging = false;
            return;
        }
        window()->move(event->globalPosition().toPoint() - m_dragPos);
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent*) {
    m_isDragging = false;
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        emit maximizeRequested();
}
