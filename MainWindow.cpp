#include "MainWindow.h"
#include "TitleBar.h"
#include "LoginDialog.h"
#include "MembershipDialog.h"
#include "OcrModelDownloadDialog.h"
#include "CustomerServiceDialog.h"
#include "NoticeDialog.h"
#include "../core/ConversionEngine.h"
#include "../core/ConfigManager.h"
#include "../core/FileProcessor.h"
#include "../core/OcrModelHelper.h"
#include "../core/UpdateManager.h"
#include "../core/NoticeManager.h"
#include "../user/UserManager.h"
#include "../ocr/DocxMerger.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileDialog>
#include <QStandardPaths>
#include <QMessageBox>
#include <QStatusBar>
#include <QHeaderView>
#include <QDesktopServices>
#include <QUrl>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QProgressBar>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QCheckBox>
#include <QAbstractButton>
#include <QFrame>
#include <QJsonObject>
#include <QDebug>
#include <QVariantAnimation>
#include <QPaintEvent>
#include <QStyle>
#include <QTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QSizePolicy>
#include <QSizeGrip>
#include <cmath>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <Windows.h>
#  include <windowsx.h>
#endif

// ── 颜色/样式常量（科技蓝 + 极简深灰）────────────────────────────────
static const QString BG_PRIMARY   = "#0B1220";
static const QString BG_SECONDARY = "#1E293B";
static const QString BG_CARD      = "#141C2B";
static const QString CLR_ACCENT   = "#38BDF8";
static const QString CLR_ACCENT_H = "#7DD3FC";
static const QString CLR_TXT_PRI  = "#F1F5F9";
static const QString CLR_TXT_SEC  = "#94A3B8";
static const QString CLR_BORDER   = "#2A3A52";

// ── 转换类型输入扩展名映射 ─────────────────────────────────────────────
static const QMap<QString, QStringList> INPUT_EXTS = {
    {"PDF转Word",   {".pdf"}},
    {"PDF转Excel",  {".pdf"}},
    {"PDF转图片",   {".pdf"}},
    {"PDF转PPT",    {".pdf"}},
    {"Word转PDF",   {".docx", ".doc"}},
    {"Word转Excel", {".docx", ".doc"}},
    {"Word转PPT",   {".docx", ".doc"}},
    {"Word转TXT",   {".docx", ".doc"}},
    {"Word转图片",  {".docx", ".doc"}},
    {"Word转EPUB",  {".docx", ".doc"}},
    {"Excel转PDF",  {".xlsx", ".xls"}},
    {"Excel转Word", {".xlsx", ".xls"}},
    {"Excel转TXT",  {".xlsx", ".xls"}},
    {"PPT转PDF",    {".pptx", ".ppt"}},
    {"PPT转Word",   {".pptx", ".ppt"}},
    {"PPT转图片",   {".pptx", ".ppt"}},
    {"TXT转PDF",    {".txt"}},
    {"TXT转Word",   {".txt"}},
    {"图片转Word",  {".jpg", ".jpeg", ".png", ".bmp"}},
    {"图片转PDF",   {".jpg", ".jpeg", ".png", ".bmp"}},
    {"图片转PPT",   {".jpg", ".jpeg", ".png", ".bmp"}},
    {"图片转Excel", {".jpg", ".jpeg", ".png", ".bmp"}},
    {"HTML转Excel", {".html", ".htm"}},
};

// ── 导航分组结构 ──────────────────────────────────────────────────────
struct NavGroup {
    QString category;
    QStringList items;
};
static const QList<NavGroup> NAV_GROUPS = {
    {"PDF 转其他",  {"PDF转Word", "PDF转Excel", "PDF转图片", "PDF转PPT"}},
    {"Word 转其他", {"Word转PDF", "Word转Excel", "Word转PPT", "Word转TXT", "Word转图片", "Word转EPUB"}},
    {"图片 转其他", {"图片转Word", "图片转PDF", "图片转PPT", "图片转Excel"}},
    {"Excel 转其他",{"Excel转PDF", "Excel转Word", "Excel转TXT"}},
    {"PPT 转其他",  {"PPT转PDF", "PPT转Word", "PPT转图片"}},
    {"TXT 转其他",  {"TXT转PDF", "TXT转Word"}},
    {"HTML 转其他", {"HTML转Excel"}},
};

// ═══════════════════════════════════════════════════════════════════════
// NavListDelegate
// ═══════════════════════════════════════════════════════════════════════

NavListDelegate::NavListDelegate(QListWidget* list)
    : QStyledItemDelegate(list), m_list(list) {}

QSize NavListDelegate::sizeHint(const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    bool isCat = idx.data(Qt::UserRole + 1).toBool();
    return QSize(opt.rect.width(), isCat ? 44 : 36);
}

void NavListDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    p->save();
    p->setRenderHint(QPainter::Antialiasing);

    bool isCat    = idx.data(Qt::UserRole + 1).toBool();
    bool selected = (opt.state & QStyle::State_Selected) && !isCat;
    bool hover    = (opt.state & QStyle::State_MouseOver) && !isCat;
    QString text  = idx.data(Qt::DisplayRole).toString();

    if (isCat) {
        QLinearGradient grad(opt.rect.topLeft(), opt.rect.bottomLeft());
        grad.setColorAt(0.0, QColor(15, 23, 42, 240));
        grad.setColorAt(1.0, QColor(30, 41, 59, 200));
        p->fillRect(opt.rect, grad);
        p->setPen(QPen(QColor(51, 65, 85, 180), 1));
        p->drawLine(opt.rect.bottomLeft(), opt.rect.bottomRight());
        QFont f = opt.font;
        f.setPixelSize(15);
        f.setBold(true);
        p->setFont(f);
        p->setPen(QColor(QStringLiteral("#38BDF8")));
        p->drawText(opt.rect.adjusted(14, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    } else {
        double phase = 0.0;
        if (m_list)
            phase = m_list->property("navPulsePhase").toDouble();
        double pulse = 0.55 + 0.45 * (0.5 + 0.5 * std::sin(phase));

        if (selected) {
            p->fillRect(opt.rect, QColor(56, 189, 248, int(28 + 22 * pulse)));
            QColor bar(56, 189, 248);
            bar.setAlphaF(0.55 + 0.35 * pulse);
            p->fillRect(QRect(opt.rect.left(), opt.rect.top(), 4, opt.rect.height()), bar);
            QColor glow(56, 189, 248, int(40 * pulse));
            p->fillRect(QRect(opt.rect.left() + 4, opt.rect.top(), 6, opt.rect.height()), glow);
            p->setPen(QColor("#E0F2FE"));
        } else if (hover) {
            p->fillRect(opt.rect, QColor(255, 255, 255, 18));
            p->setPen(QColor("#CBD5E1"));
        } else {
            p->fillRect(opt.rect, QColor(19, 28, 46, 200));
            p->setPen(QColor("#94A3B8"));
        }

        QFont f = opt.font;
        f.setPixelSize(13);
        f.setBold(false);
        p->setFont(f);
        const int indent = 28;
        p->drawText(opt.rect.adjusted(indent + 8, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
    p->restore();
}

// ═══════════════════════════════════════════════════════════════════════
// AnimatedToggle（iOS 风格滑动开关）
// ═══════════════════════════════════════════════════════════════════════
class AnimatedToggle : public QCheckBox {
public:
    explicit AnimatedToggle(QWidget* parent = nullptr)
        : QCheckBox(parent), m_offset(2.0)
    {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(44, 24);
        auto* anim = new QVariantAnimation(this);
        anim->setDuration(150);
        anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
            m_offset = v.toDouble();
            update();
        });
        connect(this, &QCheckBox::toggled, this, [this, anim](bool checked) {
            anim->stop();
            anim->setStartValue(m_offset);
            anim->setEndValue(checked ? 22.0 : 2.0);
            anim->start();
        });
        m_offset = isChecked() ? 22.0 : 2.0;
    }

protected:
    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e && e->button() == Qt::LeftButton) {
            setChecked(!isChecked());
            e->accept();
            return;
        }
        QCheckBox::mouseReleaseEvent(e);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const QRectF track(1, 1, width() - 2, height() - 2);
        const QColor offBg("#334155");
        const QColor onBg("#38BDF8");
        p.setPen(Qt::NoPen);
        p.setBrush(isChecked() ? onBg : offBg);
        p.drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);

        p.setBrush(QColor("#FFFFFF"));
        p.drawEllipse(QRectF(m_offset, 2, 20, 20));
    }

private:
    double m_offset;
};

// ═══════════════════════════════════════════════════════════════════════
// MainWindow
// ═══════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAcceptDrops(true);
    resize(1280, 800);
    setMinimumSize(960, 560);

    m_fileProcessor = new FileProcessor(this);
    connect(m_fileProcessor, &FileProcessor::filesReady,
            this, &MainWindow::onFilesReady);

    setupUi();

    // ── 连接 UserManager ──────────────────────────────────────────
    connect(&UserManager::instance(), &UserManager::autoLoginSuccess,
            this, &MainWindow::onAutoLoginSuccess);
    connect(&UserManager::instance(), &UserManager::profileUpdated,
            this, &MainWindow::onProfileUpdated);
    connect(&UserManager::instance(), &UserManager::loggedOut,
            this, &MainWindow::onLoggedOut);

    // ── 连接 ConversionEngine ─────────────────────────────────────
    connect(&ConversionEngine::instance(), &ConversionEngine::fileCompleted,
            this, &MainWindow::onFileCompleted, Qt::QueuedConnection);
    connect(&ConversionEngine::instance(), &ConversionEngine::allTasksFinished,
            this, &MainWindow::onAllFinished, Qt::QueuedConnection);
    connect(&ConversionEngine::instance(), &ConversionEngine::progressUpdated,
            m_progressBar, &QProgressBar::setValue, Qt::QueuedConnection);
    connect(&ConversionEngine::instance(), &ConversionEngine::ocrModelsMissing,
            this, &MainWindow::onOcrModelsMissing, Qt::QueuedConnection);  // no args

    // ── 连接 UpdateManager ────────────────────────────────────────
    connect(&UpdateManager::instance(), &UpdateManager::updateAvailable,
            this, &MainWindow::onUpdateAvailable);
    connect(&UpdateManager::instance(), &UpdateManager::downloadCompleted,
            this, &MainWindow::onDownloadCompleted);
    connect(&UpdateManager::instance(), &UpdateManager::installCompleted,
            this, &MainWindow::onInstallCompleted);

    // ── 连接 NoticeManager ────────────────────────────────────────
    connect(&NoticeManager::instance(), &NoticeManager::noticeAvailable,
            this, &MainWindow::onNoticeAvailable);

    // 启动自动登录
    UserManager::instance().tryAutoLogin();

    // 启动后延迟 1.5 秒拉取系统公告（避免阻塞窗口首次渲染）
    QTimer::singleShot(1500, this, [] {
        NoticeManager::instance().checkNotice();
    });
}

MainWindow::~MainWindow() {}

// ─────────────────────────────────────────────────────────────────────
// setupUi
// ─────────────────────────────────────────────────────────────────────

void MainWindow::setupUi() {
    auto* central = new QWidget(this);
    central->setObjectName("CentralWidget");
    central->setStyleSheet(
        QString("#CentralWidget{background:%1;border-radius:8px;}").arg(BG_PRIMARY));
    setCentralWidget(central);

    auto* rootLay = new QVBoxLayout(central);
    rootLay->setContentsMargins(0, 0, 0, 0);
    rootLay->setSpacing(0);

    // ── 1. 标题栏 ─────────────────────────────────────────────────
    m_titleBar = new TitleBar(central);
    rootLay->addWidget(m_titleBar);

    connect(m_titleBar, &TitleBar::loginClicked,     this, &MainWindow::onLoginClicked);
    connect(m_titleBar, &TitleBar::logoutRequested,  this, &MainWindow::onLogoutRequested);
    connect(m_titleBar, &TitleBar::upgradeRequested, this, &MainWindow::onUpgradeRequested);
    connect(m_titleBar, &TitleBar::menuActionTriggered, this, &MainWindow::onMenuAction);
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &MainWindow::onMinimize);
    connect(m_titleBar, &TitleBar::maximizeRequested, this, &MainWindow::onMaximize);
    connect(m_titleBar, &TitleBar::closeRequested,
            this, [this]{ close(); });

    // ── 2. 主体（左导航 + 右内容） ────────────────────────────────
    auto* bodyLay = new QHBoxLayout;
    bodyLay->setContentsMargins(0, 0, 0, 0);
    bodyLay->setSpacing(0);
    rootLay->addLayout(bodyLay, 1);

    // ── 2a. 左侧导航（侧栏渐变 + 高光线，列表透明叠在上方）──────────────
    auto* navWrap = new QWidget(central);
    navWrap->setObjectName(QStringLiteral("NavSidebar"));
    navWrap->setFixedWidth(228);
    navWrap->setStyleSheet(
        QStringLiteral("QWidget#NavSidebar{"
                       "background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                       "stop:0 #0a1020,stop:0.45 #111b2e,stop:1 #162536);"
                       "border-right:1px solid rgba(56,189,248,0.22);"
                       "}"));
    auto* navVL = new QVBoxLayout(navWrap);
    navVL->setContentsMargins(4, 10, 2, 10);
    navVL->setSpacing(0);

    m_navList = new QListWidget(navWrap);
    m_navList->setFocusPolicy(Qt::NoFocus);
    m_navList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navList->setMouseTracking(true);
    m_navList->setItemDelegate(new NavListDelegate(m_navList));
    m_navList->setStyleSheet(
        QStringLiteral("QListWidget{background:transparent;border:none;outline:none;}"
                       "QListWidget::item{border:none;}"));
    m_navList->setProperty("navPulsePhase", 0.0);
    buildNavList();
    connect(m_navList, &QListWidget::itemClicked, this, &MainWindow::onNavItemClicked);
    navVL->addWidget(m_navList);

    bodyLay->addWidget(navWrap);

    m_navPulseTimer = new QTimer(this);
    connect(m_navPulseTimer, &QTimer::timeout, this, &MainWindow::onNavPulseTick);
    m_navPulseTimer->start(48);

    // ── 2b. 右侧内容区 ────────────────────────────────────────────
    auto* rightWidget = new QWidget(central);
    rightWidget->setStyleSheet(QString("background:%1;").arg(BG_PRIMARY));
    auto* rightLay = new QVBoxLayout(rightWidget);
    rightLay->setContentsMargins(16, 12, 16, 12);
    rightLay->setSpacing(10);
    bodyLay->addWidget(rightWidget, 1);

    // ── 顶部按钮条 ────────────────────────────────────────────────
    auto* btnBar = new QWidget(rightWidget);
    btnBar->setStyleSheet("background:transparent;");
    auto* btnBarLay = new QHBoxLayout(btnBar);
    btnBarLay->setContentsMargins(0, 0, 0, 0);
    btnBarLay->setSpacing(8);

    QStyle* appSty = style();
    auto mkBarBtn = [&](const QString& text, const QString& tip,
                        QStyle::StandardPixmap spix,
                        const QString& borderColor, const QString& hoverGrad0) {
        auto* b = new QPushButton(text, btnBar);
        b->setIcon(appSty->standardIcon(spix));
        b->setIconSize(QSize(16, 16));
        b->setToolTip(tip);
        b->setCursor(Qt::PointingHandCursor);
        b->setFixedHeight(30);
        b->setStyleSheet(
            QString("QPushButton{"
                    "  background:rgba(30,41,59,0.85); color:%1;"
                    "  border:1px solid %2;"
                    "  border-radius:6px; font-size:13px; font-weight:500;"
                    "  padding:0 10px; text-align:left;"
                    "}"
                    "QPushButton:hover{"
                    "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
                    "    stop:0 %3,stop:1 rgba(30,41,59,0.95));"
                    "  border-color:%4; color:#E0F2FE;"
                    "}"
                    "QPushButton:pressed{ background:#0A0F1E; }")
                    .arg(CLR_ACCENT_H, borderColor, hoverGrad0,
                         CLR_ACCENT));
        return b;
    };

    auto* addFilesBtn = mkBarBtn(QStringLiteral("添加文件"),
                                 QStringLiteral("添加单个文件 (Ctrl+O)"),
                                 QStyle::SP_DialogOpenButton,
                                 QStringLiteral("rgba(56,189,248,0.35)"),
                                 QStringLiteral("rgba(56,189,248,0.32)"));
    auto* addFolderBtn = mkBarBtn(QStringLiteral("添加文件夹"),
                                  QStringLiteral("递归扫描文件夹 (Ctrl+Shift+O)"),
                                  QStyle::SP_DirOpenIcon,
                                  QStringLiteral("rgba(245,158,11,0.4)"),
                                  QStringLiteral("rgba(245,158,11,0.22)"));
    auto* clearBtn = mkBarBtn(QStringLiteral("清空列表"),
                              QStringLiteral("清除所有文件"),
                              QStyle::SP_TrashIcon,
                              QStringLiteral("rgba(248,113,113,0.45)"),
                              QStringLiteral("rgba(248,113,113,0.2)"));

    connect(addFilesBtn,  &QPushButton::clicked, this, &MainWindow::onAddFiles);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::onAddFolder);
    connect(clearBtn,     &QPushButton::clicked, this, &MainWindow::onClearFiles);

    auto* hpWrap = new QWidget(btnBar);
    auto* hpLay = new QHBoxLayout(hpWrap);
    hpLay->setContentsMargins(4, 0, 4, 0);
    hpLay->setSpacing(6);
    auto* hpTextBtn = new QPushButton("高精度识别", hpWrap);
    hpTextBtn->setCursor(Qt::PointingHandCursor);
    hpTextBtn->setFlat(true);
    hpTextBtn->setStyleSheet(
        "QPushButton{ color:#94A3B8; font-size:13px; border:none; background:transparent; padding:0 2px; }"
        "QPushButton:hover{ color:#CBD5E1; }");
    m_highPrecisionCb = new AnimatedToggle(hpWrap);
    m_highPrecisionCb->setChecked(ConfigManager::instance().highPrecisionOcrEnabled());
    hpWrap->hide();
    connect(m_highPrecisionCb, &QCheckBox::toggled, this, &MainWindow::onHighPrecisionToggled);
    connect(hpTextBtn, &QPushButton::clicked, this, [this]() {
        if (m_highPrecisionCb) m_highPrecisionCb->setChecked(!m_highPrecisionCb->isChecked());
    });
    hpLay->addWidget(hpTextBtn);
    hpLay->addWidget(m_highPrecisionCb);

    // ── 合并为一个文件 开关 ─────────────────────────────────────────
    auto* mergeWrap = new QWidget(btnBar);
    auto* mergeLay  = new QHBoxLayout(mergeWrap);
    mergeLay->setContentsMargins(4, 0, 4, 0);
    mergeLay->setSpacing(6);
    auto* mergeTextBtn = new QPushButton("合并为一个文件", mergeWrap);
    mergeTextBtn->setCursor(Qt::PointingHandCursor);
    mergeTextBtn->setFlat(true);
    mergeTextBtn->setStyleSheet(
        "QPushButton{ color:#94A3B8; font-size:13px; border:none; background:transparent; padding:0 2px; }"
        "QPushButton:hover{ color:#CBD5E1; }");
    m_mergeFileCb = new AnimatedToggle(mergeWrap);
    m_mergeFileCb->setChecked(false);
    mergeWrap->hide();
    connect(m_mergeFileCb, &QCheckBox::toggled, this, &MainWindow::onMergeFileToggled);
    connect(mergeTextBtn, &QPushButton::clicked, this, [this]() {
        if (m_mergeFileCb) m_mergeFileCb->setChecked(!m_mergeFileCb->isChecked());
    });
    mergeLay->addWidget(mergeTextBtn);
    mergeLay->addWidget(m_mergeFileCb);

    m_fileCountLabel = new QLabel("文件: 0", btnBar);
    m_fileCountLabel->setStyleSheet(
        QString("color:%1; font-size:13px; font-weight:500;"
                "background:rgba(30,41,59,0.85); border:1px solid rgba(56,189,248,0.28);"
                "border-radius:6px; padding:3px 10px;")
            .arg(CLR_ACCENT_H));

    btnBarLay->addWidget(addFilesBtn);
    btnBarLay->addWidget(addFolderBtn);
    btnBarLay->addWidget(clearBtn);
    btnBarLay->addWidget(hpWrap);
    btnBarLay->addWidget(mergeWrap);
    btnBarLay->addStretch();
    btnBarLay->addWidget(m_fileCountLabel);
    rightLay->addWidget(btnBar);

    // ── 文件列表区域标题 ────────────────────────────────────────
    auto* listHeader = new QWidget(rightWidget);
    listHeader->setStyleSheet("background:transparent;");
    auto* lhLay = new QHBoxLayout(listHeader);
    lhLay->setContentsMargins(0, 2, 0, 2);
    lhLay->setSpacing(8);
    // 左侧青色竖线
    auto* accent = new QFrame(listHeader);
    accent->setFixedSize(3, 16);
    accent->setStyleSheet(QStringLiteral("background:%1; border-radius:1px;").arg(CLR_ACCENT));
    auto* listTitle = new QLabel(QStringLiteral("文件列表"), listHeader);
    listTitle->setStyleSheet(
        QStringLiteral("color:#CBD5E1; font-size:13px; font-weight:600;"));
    lhLay->addWidget(accent);
    lhLay->addWidget(listTitle);
    lhLay->addStretch();
    rightLay->addWidget(listHeader);

    // ── 文件表格 ──────────────────────────────────────────────────
    buildFileTable();
    rightLay->addWidget(m_fileTable, 1);

    // ── 进度条 ────────────────────────────────────────────────────
    m_progressBar = new QProgressBar(rightWidget);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFixedHeight(6);
    m_progressBar->setTextVisible(false);
    m_progressBar->setStyleSheet(
        QString("QProgressBar{background:%1;border-radius:3px;border:none;}"
                "QProgressBar::chunk{background:%2;border-radius:3px;}")
                .arg(BG_SECONDARY, CLR_ACCENT));
    m_progressBar->hide();
    rightLay->addWidget(m_progressBar);

    // ── 底部配置条 ────────────────────────────────────────────────
    auto* cfgBar = new QWidget(rightWidget);
    cfgBar->setStyleSheet(
        QStringLiteral("background:%1; border-top:1px solid #2A3A52;").arg(BG_CARD));
    auto* cfgLay = new QHBoxLayout(cfgBar);
    cfgLay->setContentsMargins(12, 8, 8, 8);
    cfgLay->setSpacing(6);

    // ⚙ 输出目录 标签
    auto* outLabel = new QLabel(QStringLiteral("输出目录"), cfgBar);
    outLabel->setStyleSheet(QStringLiteral("color:#64748B; font-size:13px; margin-right:4px;"));

    // 单选按钮通用样式
    const QString rbStyle =
        "QRadioButton{ color:#CBD5E1; font-size:13px; spacing:5px; }"
        "QRadioButton::indicator{"
        "  width:14px; height:14px; border-radius:7px;"
        "  border:2px solid #2D3F55; background:#0B1220;"
        "}"
        "QRadioButton::indicator:checked{"
        "  background:#38BDF8; border-color:#38BDF8;"
        "}"
        "QRadioButton::indicator:hover{ border-color:#38BDF8; }";

    m_outputGroup = new QButtonGroup(this);
    m_sameDirRb = new QRadioButton("同级目录", cfgBar);
    m_desktopRb = new QRadioButton("桌面",     cfgBar);
    m_customRb  = new QRadioButton("选择目录", cfgBar);
    for (auto* rb : {m_sameDirRb, m_desktopRb, m_customRb})
        rb->setStyleSheet(rbStyle);
    m_desktopRb->setChecked(true);
    m_outputGroup->addButton(m_sameDirRb, 0);
    m_outputGroup->addButton(m_desktopRb, 1);
    m_outputGroup->addButton(m_customRb,  2);
    connect(m_outputGroup, &QButtonGroup::idClicked,
            this, &MainWindow::onOutputModeChanged);

    // 选择目录的浏览按钮（仅在选中"选择目录"时可见）
    m_browseBtn = new QPushButton(cfgBar);
    m_browseBtn->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
    m_browseBtn->setIconSize(QSize(16, 16));
    m_browseBtn->setFixedSize(28, 28);
    m_browseBtn->setCursor(Qt::PointingHandCursor);
    m_browseBtn->setToolTip("选择输出目录");
    m_browseBtn->setStyleSheet(
        "QPushButton{ background:transparent; border:none; font-size:13px; }"
        "QPushButton:hover{ background:#1E293B; border-radius:6px; }");
    m_browseBtn->hide();
    connect(m_browseBtn, &QPushButton::clicked, this, &MainWindow::onBrowseOutputDir);

    // 持续显示当前输出路径（始终可见）
    m_outputPathLabel = new QLabel(cfgBar);
    m_outputPathLabel->setStyleSheet("color:#8B949E; font-size:12px;");
    m_outputPathLabel->setMinimumWidth(120);

    // 兼容保留（不显示）
    m_customDirLabel = new QLabel(cfgBar);
    m_customDirLabel->hide();

    cfgLay->addWidget(outLabel);
    cfgLay->addSpacing(4);
    cfgLay->addWidget(m_sameDirRb);
    cfgLay->addWidget(m_desktopRb);
    cfgLay->addWidget(m_customRb);
    cfgLay->addWidget(m_browseBtn);
    cfgLay->addSpacing(6);
    cfgLay->addWidget(m_outputPathLabel);
    cfgLay->addStretch();

    // 取消转换（次要按钮，转换时才启用）
    m_cancelBtn = new QPushButton(QStringLiteral("取消"), cfgBar);
    m_cancelBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    m_cancelBtn->setIconSize(QSize(16, 16));
    m_cancelBtn->setFixedHeight(32);
    m_cancelBtn->setMinimumWidth(84);
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setStyleSheet(
        "QPushButton{"
        "  background:transparent; color:#FCA5A5;"
        "  border:1px solid rgba(248,113,113,0.55); border-radius:6px;"
        "  font-size:13px; font-weight:500;"
        "}"
        "QPushButton:hover{ background:rgba(248,113,113,0.15); }"
        "QPushButton:disabled{"
        "  border-color:#2D3F55; color:#475569;"
        "}");
    connect(m_cancelBtn, &QPushButton::clicked, this, &MainWindow::onCancelConversion);

    // 开始转换（主要按钮，渐变）
    m_startBtn = new QPushButton(QStringLiteral("开始转换"), cfgBar);
    m_startBtn->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
    m_startBtn->setIconSize(QSize(16, 16));
    m_startBtn->setFixedHeight(32);
    m_startBtn->setMinimumWidth(124);
    m_startBtn->setCursor(Qt::PointingHandCursor);
    m_startBtn->setStyleSheet(
        "QPushButton{"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #38BDF8,stop:1 #0284C7);"
        "  color:#fff; border:none; border-radius:6px;"
        "  font-size:13px; font-weight:600;"
        "}"
        "QPushButton:hover{"
        "  background:qlineargradient(x1:0,y1:0,x2:0,y2:1,"
        "    stop:0 #7DD3FC,stop:1 #0369A1);"
        "}"
        "QPushButton:disabled{ background:#1E293B; color:#475569; }");
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onStartConversion);

    cfgLay->addWidget(m_cancelBtn);
    cfgLay->addSpacing(4);
    cfgLay->addWidget(m_startBtn);
    rightLay->addWidget(cfgBar);

    // 初始化输出路径显示（默认桌面）
    onOutputModeChanged(1);

    // ── 状态栏 ────────────────────────────────────────────────────
    m_statusLabel = new QLabel("就绪", this);
    m_statusLabel->setStyleSheet(
        QString("color:%1;font-size:12px;padding:2px 8px;").arg(CLR_TXT_SEC));
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->setStyleSheet(
        QStringLiteral("QStatusBar{background:#0A0F18;border-top:1px solid %1;}")
                .arg(CLR_BORDER));

    auto* companyLabel = new QLabel("安徽函韵科技有限公司", this);
    companyLabel->setStyleSheet(
        QString("color:%1;font-size:11px;padding:2px 8px;").arg(CLR_TXT_SEC));
#ifndef Q_OS_WIN
    {
        auto* sg = new QSizeGrip(statusBar());
        sg->setStyleSheet(QStringLiteral("background:transparent;"));
        statusBar()->addPermanentWidget(sg, 0);
    }
#endif
    statusBar()->addPermanentWidget(companyLabel);

    // 默认选中第一个子项
    selectFirstSubItem();
}

void MainWindow::buildNavList() {
    for (const NavGroup& g : NAV_GROUPS) {
        // 分组标题
        auto* catItem = new QListWidgetItem(g.category, m_navList);
        catItem->setData(Qt::UserRole + 1, true);  // isCategory = true
        catItem->setFlags(Qt::ItemIsEnabled);       // 不可选中
        m_navList->addItem(catItem);

        // 子项
        for (const QString& sub : g.items) {
            auto* item = new QListWidgetItem(sub, m_navList);
            item->setData(Qt::UserRole + 1, false);
            item->setData(Qt::UserRole,     sub); // 存储转换类型
            m_navList->addItem(item);
        }
    }
}

void MainWindow::buildFileTable() {
    m_fileTable = new QTableWidget(0, 9, this);
    m_fileTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_fileTable->setHorizontalHeaderLabels(
        {"#", "✓", "文件名", "大小", "状态", "原文件", "转换文件", "目录", "删除"});

    // 列宽
    m_fileTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_fileTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_fileTable->setColumnWidth(0, 40);
    m_fileTable->setColumnWidth(1, 36);
    m_fileTable->setColumnWidth(3, 80);
    m_fileTable->setColumnWidth(4, 72);
    m_fileTable->setColumnWidth(5, 58);
    m_fileTable->setColumnWidth(6, 68);
    m_fileTable->setColumnWidth(7, 40);
    m_fileTable->setColumnWidth(8, 44);

    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileTable->verticalHeader()->hide();
    m_fileTable->setShowGrid(true);           // 显示网格线
    m_fileTable->setAlternatingRowColors(true);
    m_fileTable->setFocusPolicy(Qt::StrongFocus);
    m_fileTable->setSortingEnabled(false);
    m_fileTable->horizontalHeader()->setHighlightSections(false);
    m_fileTable->horizontalHeader()->setMinimumSectionSize(36);

    m_fileTable->setStyleSheet(
        "QTableWidget{"
        "  background:#141C2B; alternate-background-color:#182235;"
        "  border:1px solid #2A3A52; color:#F8FAFC;"
        "  gridline-color:#2D3A4F; outline:none; font-size:13px;"
        "}"
        "QTableWidget::item{"
        "  padding:4px 6px; border:none;"
        "  border-bottom:1px solid #2A3548;"
        "}"
        "QTableWidget::item:selected{ background:rgba(56,189,248,0.22); color:#F8FAFC; }"
        "QTableWidget::item:hover:!selected{ background:rgba(56,189,248,0.10); }"
        "QTableWidget::indicator{ width:0; height:0; border:none; }"
        "QHeaderView::section{"
        "  background:#0F172A; color:#38BDF8;"
        "  border:none; border-bottom:1px solid rgba(56,189,248,0.45);"
        "  border-right:1px solid #2D3A4F;"
        "  padding:6px 4px; font-size:13px; font-weight:600;"
        "}"
        "QHeaderView::section:last{ border-right:none; }"
        "QScrollBar:vertical{"
        "  background:#0B1220; width:8px; border-radius:4px;"
        "}"
        "QScrollBar::handle:vertical{"
        "  background:#475569; border-radius:4px; min-height:20px;"
        "}"
        "QScrollBar::handle:vertical:hover{ background:#38BDF8; }"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{ height:0; }"
    );

    connect(m_fileTable, &QTableWidget::cellClicked,
            this, &MainWindow::onCellClicked);
}

void MainWindow::onNavPulseTick() {
    if (!m_navList) return;
    double ph = m_navList->property("navPulsePhase").toDouble() + 0.11;
    if (ph > 6.283185307179586) // 2*pi
        ph -= 6.283185307179586;
    m_navList->setProperty("navPulsePhase", ph);
    QListWidgetItem* cur = m_navList->currentItem();
    if (!cur || cur->data(Qt::UserRole + 1).toBool())
        return;
    QRect r = m_navList->visualItemRect(cur);
    m_navList->viewport()->update(r.adjusted(-12, -2, 12, 2));
}

// ─────────────────────────────────────────────────────────────────────
// 导航
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onNavItemClicked(QListWidgetItem* item) {
    if (!item) return;
    bool isCat = item->data(Qt::UserRole + 1).toBool();
    if (isCat) return;

    const QString action = item->data(Qt::UserRole).toString();

    // 显式单选，确保上一个转换项高亮立即清除
    m_navList->clearSelection();
    item->setSelected(true);
    m_navList->setCurrentItem(item);

    m_currentConvType = action;
    m_statusLabel->setText(QString("当前模式: %1").arg(m_currentConvType));
    updateHighPrecisionVisibility();
    updateMergeVisibility();
    m_navList->viewport()->update();
}

void MainWindow::onCustomerServiceClicked() {
    if (!m_customerServiceDialog) {
        m_customerServiceDialog = new CustomerServiceDialog(this);
    }
    // 居中显示于主窗口
    QRect mainGeo = geometry();
    int x = mainGeo.x() + (mainGeo.width()  - m_customerServiceDialog->width())  / 2;
    int y = mainGeo.y() + (mainGeo.height() - m_customerServiceDialog->height()) / 2;
    m_customerServiceDialog->move(x, y);
    m_customerServiceDialog->show();
    m_customerServiceDialog->raise();
    m_customerServiceDialog->activateWindow();
}

void MainWindow::selectFirstSubItem() {
    for (int i = 0; i < m_navList->count(); ++i) {
        auto* item = m_navList->item(i);
        if (!item->data(Qt::UserRole + 1).toBool()) {
            m_navList->setCurrentItem(item);
            onNavItemClicked(item);
            break;
        }
    }
}

bool MainWindow::isImageOcrType() const {
    return m_currentConvType == "图片转Word" || m_currentConvType == "图片转Excel";
}

void MainWindow::updateHighPrecisionVisibility() {
    if (!m_highPrecisionCb) return;
    QWidget* wrap = m_highPrecisionCb->parentWidget();
    const bool visible = isImageOcrType();
    if (wrap) wrap->setVisible(visible);

    // 每次切换到图片转 Word/Excel 时，高精度开关默认重置为关闭
    if (visible && m_highPrecisionCb->isChecked()) {
        QSignalBlocker blocker(m_highPrecisionCb);
        m_highPrecisionCb->setChecked(false);
        ConfigManager::instance().setHighPrecisionOcrEnabled(false);
        if (m_statusLabel) m_statusLabel->setText("高精度识别: 已关闭");
    }
}

void MainWindow::updateMergeVisibility() {
    if (!m_mergeFileCb) return;
    QWidget* wrap = m_mergeFileCb->parentWidget();
    // 仅在"图片转Word"模式下显示（图片转Excel 不支持合并）
    const bool visible = (m_currentConvType == "图片转Word");
    if (wrap) wrap->setVisible(visible);

    // 切换离开"图片转Word"时复位开关
    if (!visible && m_mergeFileCb->isChecked()) {
        QSignalBlocker blocker(m_mergeFileCb);
        m_mergeFileCb->setChecked(false);
    }
}

void MainWindow::onHighPrecisionToggled(bool checked) {
    // 游客或普通用户开启时，拦截并引导开通会员
    if (checked && !UserManager::instance().isPaidUser()) {
        // 先静默复位开关，避免信号递归
        if (m_highPrecisionCb) {
            QSignalBlocker blocker(m_highPrecisionCb);
            m_highPrecisionCb->setChecked(false);
        }
        ConfigManager::instance().setHighPrecisionOcrEnabled(false);
        if (m_statusLabel && isImageOcrType())
            m_statusLabel->setText("高精度识别: 已关闭");

        QMessageBox msg(this);
        msg.setWindowTitle("高精度识别（会员专属）");
        msg.setIcon(QMessageBox::Information);
        msg.setText(
            "高精度模型识别为<b>会员专属</b>功能，<br>"
            "登录账号并开通会员后即可使用。"
        );
        msg.setTextFormat(Qt::RichText);
        auto* btnReg   = msg.addButton("立即注册", QMessageBox::ActionRole);
        auto* btnLogin = msg.addButton("登录账号", QMessageBox::ActionRole);
                         msg.addButton("取消",     QMessageBox::RejectRole);
        msg.setDefaultButton(btnLogin);
        msg.exec();

        // 登录成功后若已是付费会员，自动开启开关
        // 登录成功后若已是付费会员，再检测 server 模型
        auto onLoginDone = [this](const QJsonObject& user,
                                   const QString& access,
                                   const QString& refresh) {
            UserManager::instance().onLoginSuccess(user, access, refresh);
            if (UserManager::instance().isPaidUser() && m_highPrecisionCb) {
                // 检测 server 模型是否存在，缺少则弹下载对话框
                if (!OcrModelHelper::serverModelsExist()) {
                    OcrModelDownloadDialog dlg(this, /*serverOnly=*/true);
                    dlg.exec();
                }
                if (OcrModelHelper::serverModelsExist()) {
                    m_highPrecisionCb->setChecked(true);
                    ConfigManager::instance().setHighPrecisionOcrEnabled(true);
                    if (m_statusLabel && isImageOcrType())
                        m_statusLabel->setText("高精度识别: 已开启");
                }
            }
        };

        QAbstractButton* clicked = msg.clickedButton();
        if (clicked == btnReg) {
            LoginDialog dlg(this);
            dlg.openOnRegisterPage();
            connect(&dlg, &LoginDialog::loginSuccess, this, onLoginDone);
            dlg.exec();
        } else if (clicked == btnLogin) {
            LoginDialog dlg(this);
            connect(&dlg, &LoginDialog::loginSuccess, this, onLoginDone);
            dlg.exec();
        }
        return;
    }

    // 已是付费会员：检测 server 模型，缺少则引导下载
    if (checked && !OcrModelHelper::serverModelsExist()) {
        // 先复位开关
        if (m_highPrecisionCb) {
            QSignalBlocker blocker(m_highPrecisionCb);
            m_highPrecisionCb->setChecked(false);
        }
        ConfigManager::instance().setHighPrecisionOcrEnabled(false);
        if (m_statusLabel && isImageOcrType())
            m_statusLabel->setText("高精度识别: 已关闭");

        OcrModelDownloadDialog dlg(this, /*serverOnly=*/true);
        dlg.exec();

        // 下载完成后若模型已就绪，自动开启
        if (OcrModelHelper::serverModelsExist() && m_highPrecisionCb) {
            QSignalBlocker blocker(m_highPrecisionCb);
            m_highPrecisionCb->setChecked(true);
            ConfigManager::instance().setHighPrecisionOcrEnabled(true);
            if (m_statusLabel && isImageOcrType())
                m_statusLabel->setText("高精度识别: 已开启");
        }
        return;
    }

    ConfigManager::instance().setHighPrecisionOcrEnabled(checked);
    if (m_statusLabel && isImageOcrType()) {
        m_statusLabel->setText(checked ? "高精度识别: 已开启" : "高精度识别: 已关闭");
    }
}

void MainWindow::onMergeFileToggled(bool checked) {
    // 游客或普通用户开启时，拦截并引导开通会员
    if (checked && !UserManager::instance().isPaidUser()) {
        if (m_mergeFileCb) {
            QSignalBlocker blocker(m_mergeFileCb);
            m_mergeFileCb->setChecked(false);
        }
        if (m_statusLabel) m_statusLabel->setText("合并为一个文件: 已关闭");

        QMessageBox msg(this);
        msg.setWindowTitle("合并为一个文件（会员专属）");
        msg.setIcon(QMessageBox::Information);
        msg.setText(
            "将多张图片的识别结果<b>合并为一个 Word 文件</b>是<b>会员专属</b>功能，<br>"
            "登录账号并开通会员后即可使用。"
        );
        msg.setTextFormat(Qt::RichText);
        auto* btnReg   = msg.addButton("立即注册", QMessageBox::ActionRole);
        auto* btnLogin = msg.addButton("登录账号", QMessageBox::ActionRole);
                         msg.addButton("取消",     QMessageBox::RejectRole);
        msg.setDefaultButton(btnLogin);
        msg.exec();

        auto onLoginDone = [this](const QJsonObject& user,
                                   const QString& access,
                                   const QString& refresh) {
            UserManager::instance().onLoginSuccess(user, access, refresh);
            if (UserManager::instance().isPaidUser() && m_mergeFileCb) {
                m_mergeFileCb->setChecked(true);
                if (m_statusLabel) m_statusLabel->setText("合并为一个文件: 已开启");
            }
        };

        QAbstractButton* clicked = msg.clickedButton();
        if (clicked == btnReg) {
            LoginDialog dlg(this);
            dlg.openOnRegisterPage();
            connect(&dlg, &LoginDialog::loginSuccess, this, onLoginDone);
            dlg.exec();
        } else if (clicked == btnLogin) {
            LoginDialog dlg(this);
            connect(&dlg, &LoginDialog::loginSuccess, this, onLoginDone);
            dlg.exec();
        }
        return;
    }

    if (m_statusLabel) {
        m_statusLabel->setText(checked ? "合并为一个文件: 已开启" : "合并为一个文件: 已关闭");
    }
}

// ─────────────────────────────────────────────────────────────────────
// 文件操作
// ─────────────────────────────────────────────────────────────────────

QStringList MainWindow::acceptedExtForCurrentType() const {
    return INPUT_EXTS.value(m_currentConvType);
}

void MainWindow::onAddFiles() {
    QStringList exts = acceptedExtForCurrentType();
    QString filter = "支持的文件 (";
    if (exts.isEmpty()) {
        filter = "所有支持文件 (*.pdf *.docx *.doc *.xlsx *.xls *.pptx *.ppt "
                 "*.txt *.jpg *.jpeg *.png *.bmp *.html *.htm)";
    } else {
        for (const QString& e : exts) filter += "*" + e + " ";
        filter += ")";
    }
    QStringList files = QFileDialog::getOpenFileNames(
        this, "添加文件", QString(), filter);
    if (!files.isEmpty()) {
        QVector<QFileInfo> batch;
        for (const QString& f : files) batch.append(QFileInfo(f));
        onFilesReady(batch);
    }
}

void MainWindow::onAddFolder() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "选择文件夹", QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if (dir.isEmpty()) return;
    m_fileProcessor->scanPaths({dir}, acceptedExtForCurrentType());
    m_statusLabel->setText("正在扫描文件夹...");
}

void MainWindow::onClearFiles() {
    if (m_isConverting) {
        QMessageBox::warning(this, "提示", "正在转换中，暂不允许清空列表");
        return;
    }
    m_fileTable->setRowCount(0);
    m_fileRowMap.clear();
    m_rowOutputMap.clear();
    m_fileCountLabel->setText("文件: 0");
}

bool MainWindow::fileAlreadyAdded(const QString& path) const {
    return m_fileRowMap.contains(path);
}

QString MainWindow::formatFileSize(qint64 bytes) const {
    if (bytes < 1024)            return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024)     return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 1) + " GB";
}

void MainWindow::addFileRow(const QFileInfo& fi) {
    if (fileAlreadyAdded(fi.absoluteFilePath())) return;

    int row = m_fileTable->rowCount();
    m_fileTable->insertRow(row);
    m_fileTable->setRowHeight(row, 36);

    // Col 0: #
    auto* numItem = new QTableWidgetItem(QString::number(row + 1));
    numItem->setTextAlignment(Qt::AlignCenter);
    numItem->setForeground(QColor(CLR_TXT_SEC));
    m_fileTable->setItem(row, 0, numItem);

    // Col 1: 选中标记（用文字 ✓ 替代原生 checkbox）
    auto* cbItem = new QTableWidgetItem("✓");
    cbItem->setCheckState(Qt::Checked);
    cbItem->setTextAlignment(Qt::AlignCenter);
    cbItem->setForeground(QColor(QStringLiteral("#38BDF8")));
    m_fileTable->setItem(row, 1, cbItem);

    // Col 2: filename
    auto* nameItem = new QTableWidgetItem(fi.fileName());
    nameItem->setData(Qt::UserRole, fi.absoluteFilePath()); // 存储完整路径
    nameItem->setToolTip(fi.absoluteFilePath());
    m_fileTable->setItem(row, 2, nameItem);

    // Col 3: size
    auto* sizeItem = new QTableWidgetItem(formatFileSize(fi.size()));
    sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sizeItem->setForeground(QColor(CLR_TXT_SEC));
    m_fileTable->setItem(row, 3, sizeItem);

    // Col 4: status
    auto* stItem = new QTableWidgetItem("待转换");
    stItem->setTextAlignment(Qt::AlignCenter);
    stItem->setForeground(QColor(QStringLiteral("#38BDF8")));
    m_fileTable->setItem(row, 4, stItem);

    // Col 5/6/7/8: 动作图标
    auto mkIconItem = [](const QString& icon, const QColor& c) {
        auto* it = new QTableWidgetItem(icon);
        it->setTextAlignment(Qt::AlignCenter);
        it->setForeground(c);
        it->setToolTip(icon);
        return it;
    };
    // 原文件：打开上传文件
    m_fileTable->setItem(row, 5, mkIconItem("📄", QColor("#CBD5E1")));
    // 转换文件：转换前置灰，成功后点亮
    m_fileTable->setItem(row, 6, mkIconItem("✅", QColor("#475569")));
    // 目录图标：琥珀黄文件夹
    m_fileTable->setItem(row, 7, mkIconItem("📁", QColor("#F59E0B")));
    // 删除：灰色 ×（hover 变红由整行高亮体现）
    m_fileTable->setItem(row, 8, mkIconItem("✕",  QColor("#64748B")));

    m_fileRowMap[fi.absoluteFilePath()] = row;
    m_fileCountLabel->setText(QString("文件: %1").arg(m_fileTable->rowCount()));
}

void MainWindow::onFilesReady(const QVector<QFileInfo>& batch) {
    for (const QFileInfo& fi : batch) addFileRow(fi);
}

// ─────────────────────────────────────────────────────────────────────
// 表格点击（打开文件/目录/删除行）
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onCellClicked(int row, int col) {
    if (col == 1) {
        // 切换选中标记
        auto* cb = m_fileTable->item(row, 1);
        if (cb) {
            bool nowChecked = (cb->checkState() == Qt::Checked);
            if (nowChecked) {
                cb->setCheckState(Qt::Unchecked);
                cb->setText("○");
                cb->setForeground(QColor("#475569"));
            } else {
                cb->setCheckState(Qt::Checked);
                cb->setText("✓");
                cb->setForeground(QColor(QStringLiteral("#38BDF8")));
            }
        }
        return;
    }
    if (col == 5) {
        // 原文件：打开上传文件
        auto* nameItem = m_fileTable->item(row, 2);
        if (nameItem) {
            QString srcPath = nameItem->data(Qt::UserRole).toString();
            if (!srcPath.isEmpty() && QFile::exists(srcPath)) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(srcPath));
            }
        }
    } else if (col == 6) {
        // 转换文件：仅打开转换后文件
        QString outPath = m_rowOutputMap.value(row);
        if (!outPath.isEmpty() && QFile::exists(outPath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(outPath));
        } else {
            QMessageBox::information(this, "提示", "该文件尚未生成转换结果");
        }
    } else if (col == 7) {
        QString outPath = m_rowOutputMap.value(row);
        QString dir;
        if (!outPath.isEmpty()) {
            dir = QFileInfo(outPath).absolutePath();
        } else {
            auto* nameItem = m_fileTable->item(row, 2);
            if (nameItem) dir = QFileInfo(nameItem->data(Qt::UserRole).toString()).absolutePath();
        }
        if (!dir.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    } else if (col == 8) {
        if (m_isConverting) return;
        // 删除行，更新 map
        auto* nameItem = m_fileTable->item(row, 2);
        if (nameItem) m_fileRowMap.remove(nameItem->data(Qt::UserRole).toString());
        m_rowOutputMap.remove(row);
        m_fileTable->removeRow(row);
        // 重新编号
        for (int r = 0; r < m_fileTable->rowCount(); ++r) {
            if (auto* ni = m_fileTable->item(r, 0)) ni->setText(QString::number(r + 1));
        }
        // 重建 rowMap（行号变了）
        m_fileRowMap.clear();
        m_rowOutputMap.clear();
        for (int r = 0; r < m_fileTable->rowCount(); ++r) {
            auto* ni = m_fileTable->item(r, 2);
            if (ni) m_fileRowMap[ni->data(Qt::UserRole).toString()] = r;
        }
        m_fileCountLabel->setText(QString("文件: %1").arg(m_fileTable->rowCount()));
    }
}

// ─────────────────────────────────────────────────────────────────────
// 拖放
// ─────────────────────────────────────────────────────────────────────

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event) {
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls())
        paths << url.toLocalFile();
    if (!paths.isEmpty())
        m_fileProcessor->scanPaths(paths, acceptedExtForCurrentType());
}

// ─────────────────────────────────────────────────────────────────────
// 输出路径
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onOutputModeChanged(int id) {
    m_outputMode = id;
    m_browseBtn->setVisible(id == 2);

    switch (id) {
    case 0: // 同级目录
        m_outputPathLabel->setText("（与源文件同级目录）");
        break;
    case 1: // 桌面
        m_outputPathLabel->setText(
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
        break;
    case 2: // 自定义
        if (m_customOutputDir.isEmpty()) {
            // 首次选择时立即弹出目录选择对话框
            onBrowseOutputDir();
        } else {
            m_outputPathLabel->setText(m_customOutputDir);
        }
        break;
    }
}

void MainWindow::onBrowseOutputDir() {
    QString dir = QFileDialog::getExistingDirectory(
        this, "选择输出目录", m_customOutputDir);
    if (!dir.isEmpty()) {
        m_customOutputDir = dir;
        m_outputPathLabel->setText(dir);
        m_outputPathLabel->setToolTip(dir);
    } else if (m_customOutputDir.isEmpty()) {
        // 取消选择且之前无路径 → 回退到桌面模式
        m_desktopRb->setChecked(true);
        onOutputModeChanged(1);
    }
}

QString MainWindow::outputDirForFile(const QString& filePath) const {
    switch (m_outputMode) {
        case 0: return "";  // 空 = ConversionEngine 使用源文件目录
        case 1: return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        case 2: return m_customOutputDir;
        default: return "";
    }
}

// ─────────────────────────────────────────────────────────────────────
// 转换流程
// ─────────────────────────────────────────────────────────────────────

QStringList MainWindow::checkedFilePaths() const {
    QStringList result;
    for (int r = 0; r < m_fileTable->rowCount(); ++r) {
        auto* cb = m_fileTable->item(r, 1);
        if (cb && cb->checkState() == Qt::Checked) {
            auto* ni = m_fileTable->item(r, 2);
            if (ni) result << ni->data(Qt::UserRole).toString();
        }
    }
    return result;
}

void MainWindow::onStartConversion() {
    if (m_isConverting) return;

    if (m_currentConvType.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先在左侧导航栏选择转换类型");
        return;
    }

    QStringList files = checkedFilePaths();
    if (files.isEmpty()) {
        QMessageBox::warning(this, "提示", "请添加并勾选至少一个文件");
        return;
    }

    if (m_outputMode == 2 && m_customOutputDir.isEmpty()) {
        QMessageBox::warning(this, "提示", "请先选择输出目录");
        return;
    }

    // 扩展名校验
    QStringList accepted = acceptedExtForCurrentType();
    if (!accepted.isEmpty()) {
        for (const QString& fp : files) {
            QString ext = "." + QFileInfo(fp).suffix().toLower();
            if (!accepted.contains(ext)) {
                QMessageBox::warning(this, "格式不匹配",
                    QString("文件 %1 的格式与转换类型 [%2] 不匹配\n"
                            "该转换类型接受: %3")
                    .arg(QFileInfo(fp).fileName(), m_currentConvType, accepted.join(" ")));
                return;
            }
        }
    }

    // 额度校验
    if (!UserManager::instance().consumeDailyQuota(files.size())) {
        int rem = UserManager::instance().remainingQuota();

        if (!UserManager::instance().isLoggedIn()) {
            // ── 游客额度已满：引导注册或登录，不直接弹升级界面 ──────────
            QMessageBox msg(this);
            msg.setWindowTitle("今日免费次数已用完");
            msg.setIcon(QMessageBox::Information);
            msg.setText(
                QString("您今日的 <b>3 次</b>免费转换已全部用完。<br>"
                        "开通会员可享受<b>无限次</b>转换。")
            );
            msg.setTextFormat(Qt::RichText);
            auto* btnReg   = msg.addButton("立即注册", QMessageBox::ActionRole);
            auto* btnLogin = msg.addButton("登录账号", QMessageBox::ActionRole);
                             msg.addButton("取消",    QMessageBox::RejectRole);
            msg.setDefaultButton(btnReg);
            msg.exec();

            auto openLogin = [this](const QJsonObject& user,
                                    const QString& access,
                                    const QString& refresh) {
                UserManager::instance().onLoginSuccess(user, access, refresh);
            };

            QAbstractButton* clicked = msg.clickedButton();
            if (clicked == btnReg) {
                LoginDialog dlg(this);
                dlg.openOnRegisterPage();
                connect(&dlg, &LoginDialog::loginSuccess, this, openLogin);
                dlg.exec();
            } else if (clicked == btnLogin) {
                LoginDialog dlg(this);
                connect(&dlg, &LoginDialog::loginSuccess, this, openLogin);
                dlg.exec();
            }
        } else {
            // ── 已登录免费用户：提示升级会员 ──────────────────────────────
            QMessageBox::StandardButton btn = QMessageBox::warning(
                this, "转换次数不足",
                QString("今日剩余转换次数：%1 次\n"
                        "所选文件需要：%2 次\n\n"
                        "升级会员可享受无限次转换，是否立即升级？")
                        .arg(rem).arg(files.size()),
                QMessageBox::Yes | QMessageBox::No);
            if (btn == QMessageBox::Yes) onUpgradeRequested();
        }
        return;
    }

    // 确定输出路径（统一使用第一个文件的输出目录，除非 mode=0 由引擎自行决定）
    QString outDir = outputDirForFile(files.first());

    // 重置状态列
    for (const QString& fp : files) {
        int row = m_fileRowMap.value(fp, -1);
        if (row >= 0) {
            auto* st = m_fileTable->item(row, 4);
            if (st) { st->setText("等待中"); st->setForeground(QColor(CLR_TXT_SEC)); }
        }
    }

    m_isConverting = true;
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);
    m_progressBar->setValue(0);
    m_progressBar->show();
    m_statusLabel->setText(QString("正在转换 %1 个文件...").arg(files.size()));

    const bool useHighPrecision = isImageOcrType() && m_highPrecisionCb && m_highPrecisionCb->isChecked();
    if (useHighPrecision && !ConfigManager::instance().highPrecisionWarnDisabled()) {
        QMessageBox msg(this);
        msg.setIcon(QMessageBox::Information);
        msg.setWindowTitle("提示");
        msg.setText("高精度识别已开启，识别质量更高，但处理耗时会明显增加。");
        msg.setInformativeText("是否继续本次转换？");
        auto* btnContinue = msg.addButton("继续", QMessageBox::AcceptRole);
        auto* btnCancel = msg.addButton("取消", QMessageBox::RejectRole);
        auto* btnNoMore = msg.addButton("不再提示", QMessageBox::ActionRole);
        msg.setDefaultButton(btnContinue);
        msg.exec();
        QAbstractButton* clicked = msg.clickedButton();
        if (clicked == btnNoMore) {
            ConfigManager::instance().setHighPrecisionWarnDisabled(true);
        } else if (clicked != btnContinue) {
            m_isConverting = false;
            m_startBtn->setEnabled(true);
            m_cancelBtn->setEnabled(false);
            m_progressBar->hide();
            m_statusLabel->setText("已取消开始转换");
            return;
        }
    }

    // 记录本次是否需要合并（图片转Word 且开关开启）
    m_pendingMerge = (m_currentConvType == "图片转Word")
                  && m_mergeFileCb && m_mergeFileCb->isChecked();

    ConversionEngine::instance().convertAsync(m_currentConvType, files, outDir, useHighPrecision);
}

void MainWindow::onCancelConversion() {
    if (!m_isConverting) return;
    // 向后台线程发送取消信号（线程安全的原子标志）
    ConversionEngine::instance().requestCancel();
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setText("取消中...");
    m_statusLabel->setText("正在取消，等待当前文件处理完毕…");
}

void MainWindow::updateFileStatus(const QString& filePath,
                                   const QString& text,
                                   const QColor& color) {
    int row = m_fileRowMap.value(filePath, -1);
    if (row < 0) return;
    auto* st = m_fileTable->item(row, 4);
    if (st) { st->setText(text); st->setForeground(color); }
}

void MainWindow::onFileCompleted(const QString& filePath,
                                  const QString& outputPath,
                                  bool success,
                                  const QString& errMsg) {
    int row = m_fileRowMap.value(filePath, -1);
    if (row >= 0 && !outputPath.isEmpty()) m_rowOutputMap[row] = outputPath;

    if (success) {
        updateFileStatus(filePath, "✓ 完成", QColor("#34D399"));
        // 点亮“转换文件”和“目录”图标
        if (auto* i6 = m_fileTable->item(row, 6)) i6->setForeground(QColor(CLR_ACCENT));
        if (auto* i7 = m_fileTable->item(row, 7)) i7->setForeground(QColor(CLR_ACCENT));
    } else {
        if (errMsg.contains("用户已取消")) {
            updateFileStatus(filePath, "已取消", QColor("#F59E0B"));
        } else {
            updateFileStatus(filePath, "✗ 失败", QColor("#F87171"));
        }
        if (!errMsg.isEmpty()) {
            auto* st = m_fileTable->item(row, 4);
            if (st) st->setToolTip(errMsg);
        }
    }
}

void MainWindow::onAllFinished(int successCount, int failedCount) {
    m_isConverting = false;
    m_startBtn->setEnabled(true);
    m_cancelBtn->setEnabled(false);
    m_cancelBtn->setText("✕  取消");
    m_progressBar->setValue(100);
    m_progressBar->hide();

    // ── 合并模式：将所有成功的 .docx 合并为一个文件 ───────────────────
    const bool doMerge = m_pendingMerge && successCount > 0;
    m_pendingMerge = false;

    if (doMerge) {
        // 按行号顺序收集所有成功的输出路径
        QStringList outputPaths;
        for (int row = 0; row < m_fileTable->rowCount(); ++row) {
            if (m_rowOutputMap.contains(row))
                outputPaths << m_rowOutputMap[row];
        }

        if (outputPaths.size() >= 2) {
            // 合并文件名：基于第一个文件的基名 + _merged.docx
            QString firstBase = QFileInfo(outputPaths.first()).completeBaseName();
            QString outDir    = QFileInfo(outputPaths.first()).absolutePath();
            QString mergedPath = outDir + "/" + firstBase + QString("_等%1个_merged.docx")
                                 .arg(outputPaths.size());

            m_statusLabel->setText("正在合并文件，请稍候...");

            if (DocxMerger::merge(outputPaths, mergedPath)) {
                // 删除各个单独的 .docx，只保留合并后的文件
                for (const QString& p : outputPaths)
                    QFile::remove(p);

                // 更新 UI 状态列标记为"已合并"（直接按行号修改，绕过路径查找）
                for (int row = 0; row < m_fileTable->rowCount(); ++row) {
                    if (!m_rowOutputMap.contains(row)) continue;
                    auto* st = m_fileTable->item(row, 4);
                    if (st) { st->setText("✓ 已合并"); st->setForeground(QColor("#34D399")); }
                }

                QString statusText = QString("合并完成：%1 个文件 → %2")
                    .arg(outputPaths.size())
                    .arg(QFileInfo(mergedPath).fileName());
                m_statusLabel->setText(statusText);

                QMessageBox mb(this);
                mb.setWindowTitle("合并完成");
                mb.setIcon(QMessageBox::Information);
                mb.setText(QString("已将 %1 个文件成功合并为：\n%2")
                           .arg(outputPaths.size())
                           .arg(mergedPath));
                auto* btnOpen = mb.addButton("打开文件", QMessageBox::ActionRole);
                auto* btnDir  = mb.addButton("打开目录", QMessageBox::ActionRole);
                                mb.addButton("关闭",     QMessageBox::RejectRole);
                mb.setDefaultButton(btnOpen);
                mb.exec();

                QAbstractButton* clicked = mb.clickedButton();
                if (clicked == btnOpen)
                    QDesktopServices::openUrl(QUrl::fromLocalFile(mergedPath));
                else if (clicked == btnDir)
                    QDesktopServices::openUrl(QUrl::fromLocalFile(outDir));
                return;
            } else {
                // 合并失败，不删除原文件，给出提示
                QMessageBox::warning(this, "合并失败",
                    "文件合并过程中出现错误，各文件已单独保存在输出目录中。");
            }
        }
        // outputPaths.size() == 1 时无需合并，正常显示
    }

    // ── 普通完成提示 ──────────────────────────────────────────────────
    QString msg = QString("转换完成：成功 %1 个").arg(successCount);
    if (failedCount > 0) msg += QString("，失败 %1 个").arg(failedCount);
    m_statusLabel->setText(msg);

    if (failedCount == 0) {
        QMessageBox::information(this, "转换完成",
            QString("所有文件转换成功！共 %1 个文件").arg(successCount));
    } else {
        QMessageBox::warning(this, "转换完成",
            QString("转换完成：成功 %1 个，失败 %2 个\n"
                    "失败的文件请查看状态列的提示信息")
                    .arg(successCount).arg(failedCount));
    }
}

void MainWindow::onOcrModelsMissing() {
    OcrModelDownloadDialog dlg(this);
    dlg.exec();
}

// ─────────────────────────────────────────────────────────────────────
// 标题栏回调
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onLoginClicked() {
    LoginDialog dlg(this);
    connect(&dlg, &LoginDialog::loginSuccess,
            [this](const QJsonObject& user, const QString& access, const QString& refresh) {
                UserManager::instance().onLoginSuccess(user, access, refresh);
            });
    dlg.exec();
}

void MainWindow::onLogoutRequested() {
    auto btn = QMessageBox::question(this, "退出登录", "确定要退出登录吗？",
                                     QMessageBox::Yes | QMessageBox::No);
    if (btn == QMessageBox::Yes) UserManager::instance().logout();
}

void MainWindow::onUpgradeRequested() {
    auto& um = UserManager::instance();

    // 用 currentUser() 判断：即使 m_loggedIn 因网络抖动被误清，只要有用户数据仍可继续
    if (um.currentUser().isEmpty()) {
        QMessageBox::information(this, "请先登录", "请先登录后再购买会员");
        return;
    }

    MembershipDialog* dlg = new MembershipDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    connect(dlg, &MembershipDialog::paymentSuccess, this, [this]() {
        // 支付成功：立即刷新 + 延迟 2 秒再刷（给服务端事务提交留余量）
        UserManager::instance().fetchProfile();
        QTimer::singleShot(2000, this, [this]() {
            UserManager::instance().fetchProfile();
        });
    });

    dlg->exec();

    // 无论支付成功、取消还是直接关闭窗口，dialog 退出后都刷新一次，
    // 确保 TitleBar 显示的始终是服务端最新的会员状态
    UserManager::instance().fetchProfile();
}

void MainWindow::onMenuAction(const QString& action) {
    if (action == "customer_service") {
        onCustomerServiceClicked();
    } else if (action == "update") {
        UpdateManager::instance().checkForUpdate(false);
    } else if (action == "feedback") {
        QString apiUrl = ConfigManager::instance().apiBaseUrl();
        if (apiUrl.endsWith("/api/v1/"))
            apiUrl.chop(8);
        else if (apiUrl.endsWith("/api/v1"))
            apiUrl.chop(7);
        if (apiUrl.endsWith('/'))
            apiUrl.chop(1);
        QDesktopServices::openUrl(QUrl(apiUrl + "/support/"));
    } else if (action == "about") {
        showAboutDialog();
    } else if (action == "exit") {
        close();
    }
}

void MainWindow::onMinimize() { showMinimized(); }

void MainWindow::onMaximize() {
    if (isMaximized()) {
        showNormal();
        m_titleBar->setMaximized(false);
    } else {
        showMaximized();
        m_titleBar->setMaximized(true);
    }
}

// ─────────────────────────────────────────────────────────────────────
// UserManager 回调
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onAutoLoginSuccess(const QJsonObject& userInfo) {
    m_titleBar->setUserState(userInfo);
    m_statusLabel->setText(
        QString("欢迎回来，%1").arg(userInfo.value("username").toString()));
}

void MainWindow::onProfileUpdated(const QJsonObject& userInfo) {
    m_titleBar->setUserState(userInfo);
}

void MainWindow::onLoggedOut() {
    m_titleBar->setLoginState();
    m_statusLabel->setText("已退出登录");
}

// ─────────────────────────────────────────────────────────────────────
// UpdateManager 回调
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onUpdateAvailable(const QString& ver, const QString& notes,
                                    const QString& url, const QString& sha256,
                                    bool forceUpdate) {
    QString body = QString("新版本 %1 可用\n\n%2\n\n是否立即下载更新？").arg(ver, notes);
    if (forceUpdate) {
        // 强制更新：只提供"立即更新"，不允许跳过
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("强制更新");
        msgBox.setText(body);
        msgBox.setIcon(QMessageBox::Warning);
        QPushButton* btnNow = msgBox.addButton("立即更新", QMessageBox::AcceptRole);
        msgBox.setDefaultButton(btnNow);
        msgBox.exec();
        m_statusLabel->setText("正在下载更新...");
        UpdateManager::instance().downloadUpdate(url, sha256);
    } else {
        auto btn = QMessageBox::question(
            this, "发现新版本", body,
            QMessageBox::Yes | QMessageBox::No);
        if (btn == QMessageBox::Yes) {
            m_statusLabel->setText("正在下载更新...");
            UpdateManager::instance().downloadUpdate(url, sha256);
        }
    }
}

void MainWindow::onDownloadCompleted(const QString& zipPath) {
    m_statusLabel->setText("下载完成，正在安装...");
    // sha256 由 UpdateManager 内部 m_pendingSha256 保存，传空串让其自动使用内部值
    UpdateManager::instance().installUpdate(zipPath, "");
}

void MainWindow::onInstallCompleted() {
    QMessageBox::information(this, "更新成功", "更新安装完成，请重启应用以使用新版本。");
    m_statusLabel->setText("更新安装完成");
}

// ─────────────────────────────────────────────────────────────────────
// NoticeManager 回调
// ─────────────────────────────────────────────────────────────────────

void MainWindow::onNoticeAvailable(int noticeId, const QString& title, const QString& content) {
    auto* dlg = new NoticeDialog(noticeId, title, content, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}

// ─────────────────────────────────────────────────────────────────────
// 关于对话框
// ─────────────────────────────────────────────────────────────────────

void MainWindow::showAboutDialog() {
    QMessageBox about(this);
    about.setWindowTitle("关于 FileTran");
    about.setText(
        "<b>FileTran 文件格式转换器</b><br>"
        "版本 1.0.0<br><br>"
        "支持 PDF、Word、Excel、PPT、图片、TXT、HTML 等多格式互转<br><br>"
        "© 2024 安徽函韵科技有限公司");
    about.setIcon(QMessageBox::Information);
    about.exec();
}

// ─────────────────────────────────────────────────────────────────────
// 快捷键 / 关闭事件
// ─────────────────────────────────────────────────────────────────────

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && m_isConverting) {
        onCancelConversion();
    } else if (event->modifiers() == Qt::ControlModifier) {
        if (event->key() == Qt::Key_O) {
            onAddFiles();
        } else if (event->key() == Qt::Key_A) {
            for (int r = 0; r < m_fileTable->rowCount(); ++r) {
                auto* cb = m_fileTable->item(r, 1);
                if (cb) cb->setCheckState(Qt::Checked);
            }
        }
    } else if (event->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {
        if (event->key() == Qt::Key_O) onAddFolder();
    } else if (event->key() == Qt::Key_Delete) {
        // 删除选中行
        QList<int> rows;
        for (auto* sel : m_fileTable->selectedItems()) {
            if (!rows.contains(sel->row())) rows << sel->row();
        }
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows) onCellClicked(r, 7);
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        auto* msg = static_cast<MSG*>(message);
        if (msg->message == WM_NCHITTEST && !isMaximized()) {
            RECT wr{};
            HWND hwnd = reinterpret_cast<HWND>(winId());
            if (hwnd && GetWindowRect(hwnd, &wr)) {
                const LONG x = GET_X_LPARAM(msg->lParam);
                const LONG y = GET_Y_LPARAM(msg->lParam);
                const int  b = qMax(8, int(8 * devicePixelRatioF()));
                const bool left   = x < wr.left + b;
                const bool right  = x >= wr.right - b;
                const bool top    = y < wr.top + b;
                const bool bottom = y >= wr.bottom - b;
                if (top && left) {
                    *result = HTTOPLEFT;
                    return true;
                }
                if (top && right) {
                    *result = HTTOPRIGHT;
                    return true;
                }
                if (bottom && left) {
                    *result = HTBOTTOMLEFT;
                    return true;
                }
                if (bottom && right) {
                    *result = HTBOTTOMRIGHT;
                    return true;
                }
                if (left) {
                    *result = HTLEFT;
                    return true;
                }
                if (right) {
                    *result = HTRIGHT;
                    return true;
                }
                if (top) {
                    *result = HTTOP;
                    return true;
                }
                if (bottom) {
                    *result = HTBOTTOM;
                    return true;
                }
            }
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (m_isConverting) {
        auto btn = QMessageBox::question(
            this, "确认退出", "正在转换中，确定要退出吗？",
            QMessageBox::Yes | QMessageBox::No);
        if (btn == QMessageBox::No) { event->ignore(); return; }
    }
    event->accept();
}
