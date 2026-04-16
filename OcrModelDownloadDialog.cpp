#include "OcrModelDownloadDialog.h"
#include "../core/ConfigManager.h"
#include "../core/OcrModelHelper.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMouseEvent>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QStackedWidget>
#include <QTableWidget>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>

// ── palette ──────────────────────────────────────────────────────────────────
static const char* BG_WINDOW   = "#0d1117";
static const char* BG_HEADER   = "#0a0f17";
static const char* BG_TABLE    = "#111b2e";
static const char* BG_ROW_ALT  = "#0e1621";
static const char* CLR_BORDER  = "#21262d";
static const char* CLR_ACCENT  = "#00d4ff";
static const char* CLR_TXT     = "#c9d1d9";
static const char* CLR_TXT_SEC = "#8b949e";
static const char* CLR_OK      = "#3fb950";
static const char* CLR_WARN    = "#d29922";
static const char* CLR_ERR     = "#f85149";
static const char* CLR_BTN     = "#161b22";

// ── styles ───────────────────────────────────────────────────────────────────
static const QString BTN_DOWNLOAD_STYLE =
    "QPushButton#omdDownload{"
    "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0d6efd,stop:1 #00c6a7);"
    "  color:#fff; font-weight:700; font-size:13px;"
    "  border:none; border-radius:7px; padding:0 22px; height:36px;"
    "}"
    "QPushButton#omdDownload:hover{ background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #1a7fff,stop:1 #00d4b3); }"
    "QPushButton#omdDownload:disabled{ background:#2D3F55; color:#64748B; }";

static const QString BTN_CLOSE_STYLE =
    "QPushButton#omdClose{"
    "  background:#161b22; color:#8b949e;"
    "  border:1px solid #30363d; border-radius:7px; padding:0 22px; height:36px; font-size:13px;"
    "}"
    "QPushButton#omdClose:hover{ color:#c9d1d9; border-color:#58a6ff; }";

static const QString BTN_RETRY_STYLE =
    "QPushButton#omdRetry{"
    "  background:#1a2332; color:#00c6a7;"
    "  border:1px solid rgba(0,198,167,0.3); border-radius:7px; padding:0 22px; height:34px; font-size:12px;"
    "}"
    "QPushButton#omdRetry:hover{ background:#223040; }";

static const QString TABLE_STYLE =
    "QTableWidget{"
    "  background:#111b2e; alternate-background-color:#0e1621;"
    "  color:#c9d1d9; gridline-color:#21262d;"
    "  border:1px solid #21262d; border-radius:6px; font-size:12px;"
    "}"
    "QTableWidget::item{ padding:5px 8px; }"
    "QTableWidget::item:selected{ background:#1c3250; }"
    "QHeaderView::section{"
    "  background:#0a0f17; color:#8b949e; font-size:11px; font-weight:600;"
    "  border:none; border-bottom:1px solid #21262d; padding:6px 8px;"
    "}";

static const QString PROGRESS_STYLE =
    "QProgressBar{"
    "  background:#1a2332; border:1px solid #21262d; border-radius:5px;"
    "  color:#c9d1d9; font-size:11px; text-align:center; height:14px;"
    "}"
    "QProgressBar::chunk{ background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #0d6efd,stop:1 #00c6a7); border-radius:4px; }";

// ─────────────────────────────────────────────────────────────────────────────

OcrModelDownloadDialog::OcrModelDownloadDialog(QWidget* parent, bool serverOnly)
    : QDialog(parent), m_serverOnly(serverOnly)
{
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setModal(true);
    setFixedWidth(720);

    m_nam = new QNetworkAccessManager(this);
    m_nam->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);

    setupUi();
    fetchModelList();
}

// ── UI construction ───────────────────────────────────────────────────────────

void OcrModelDownloadDialog::setupUi()
{
    // outer frame
    auto* outer = new QWidget(this);
    outer->setObjectName("omdOuter");
    outer->setStyleSheet(
        QString("QWidget#omdOuter{"
                "  background:%1;"
                "  border:1px solid %2;"
                "  border-radius:12px;"
                "}").arg(BG_WINDOW, CLR_BORDER));

    auto* rootLay = new QVBoxLayout(this);
    rootLay->setContentsMargins(0,0,0,0);
    rootLay->addWidget(outer);

    auto* vl = new QVBoxLayout(outer);
    vl->setContentsMargins(24, 16, 24, 20);
    vl->setSpacing(14);

    // ── title bar ──────────────────────────────────────────────────────────
    auto* titleRow = new QHBoxLayout;
    titleRow->setContentsMargins(0,0,0,0);

    m_titleLabel = new QLabel(
        m_serverOnly ? QStringLiteral("📦  下载高精度 OCR 模型")
                     : QStringLiteral("📦  下载 OCR 模型"), outer);
    m_titleLabel->setStyleSheet(
        QString("color:%1; font-size:15px; font-weight:700;").arg(CLR_TXT));
    titleRow->addWidget(m_titleLabel);
    titleRow->addStretch();

    auto* xBtn = new QPushButton(QStringLiteral("✕"), outer);
    xBtn->setFixedSize(28, 28);
    xBtn->setStyleSheet(
        "QPushButton{ background:transparent; color:#8b949e; border:none; font-size:14px; border-radius:4px; }"
        "QPushButton:hover{ background:#2d333b; color:#f0f6fc; }");
    connect(xBtn, &QPushButton::clicked, this, &QDialog::reject);
    titleRow->addWidget(xBtn);

    vl->addLayout(titleRow);

    // divider
    auto* div = new QFrame(outer);
    div->setFrameShape(QFrame::HLine);
    div->setStyleSheet(QString("color:%1;").arg(CLR_BORDER));
    vl->addWidget(div);

    // ── description ────────────────────────────────────────────────────────
    auto* descLabel = new QLabel(
        m_serverOnly
            ? QStringLiteral("高精度识别需要 <b>PP-OCRv5 Server</b> 模型文件。\n"
                             "下载完成后即可使用高精度模式（约 165 MB）。")
            : QStringLiteral("图片转 Word / 图片转 Excel 需要 OCR 模型文件（.onnx）。\n"
                             "程序目录下的 <b>models/</b> 文件夹中未检测到任何模型，请下载后重试转换。"),
        outer);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QString("color:%1; font-size:12px; line-height:1.6;").arg(CLR_TXT_SEC));
    descLabel->setTextFormat(Qt::RichText);
    vl->addWidget(descLabel);

    // ── stacked pages ──────────────────────────────────────────────────────
    m_stack = new QStackedWidget(outer);
    m_stack->setStyleSheet("background:transparent;");
    vl->addWidget(m_stack, 1);

    // page 0 – fetching
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 20, 0, 20);
        m_fetchingLabel = new QLabel(QStringLiteral("⏳  正在从服务端获取模型文件列表…"), page);
        m_fetchingLabel->setAlignment(Qt::AlignCenter);
        m_fetchingLabel->setStyleSheet(
            QString("color:%1; font-size:13px;").arg(CLR_TXT_SEC));
        pl->addWidget(m_fetchingLabel);
        m_stack->addWidget(page);   // idx 0
    }

    // page 1 – error
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 12, 0, 12);
        pl->setSpacing(12);

        m_errLabel = new QLabel(page);
        m_errLabel->setWordWrap(true);
        m_errLabel->setTextFormat(Qt::PlainText);
        m_errLabel->setStyleSheet(
            QString("color:%1; font-size:12px;"
                    "background:#1a1015; border:1px solid %2;"
                    "border-radius:6px; padding:10px 12px;").arg(CLR_ERR, CLR_BORDER));
        pl->addWidget(m_errLabel);

        m_retryBtn = new QPushButton(QStringLiteral("重新获取"), page);
        m_retryBtn->setObjectName("omdRetry");
        m_retryBtn->setStyleSheet(BTN_RETRY_STYLE);
        m_retryBtn->setFixedWidth(120);
        connect(m_retryBtn, &QPushButton::clicked, this, &OcrModelDownloadDialog::onRetryFetch);

        auto* rl = new QHBoxLayout;
        rl->addWidget(m_retryBtn);
        rl->addStretch();
        pl->addLayout(rl);
        pl->addStretch();

        m_stack->addWidget(page);   // idx 1
    }

    // page 2 – file list
    {
        auto* page = new QWidget;
        auto* pl = new QVBoxLayout(page);
        pl->setContentsMargins(0, 0, 0, 0);
        pl->setSpacing(10);

        m_table = new QTableWidget(0, 4, page);
        m_table->setStyleSheet(TABLE_STYLE);
        m_table->setAlternatingRowColors(true);
        m_table->setSelectionMode(QAbstractItemView::NoSelection);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_table->verticalHeader()->hide();
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setShowGrid(true);
        m_table->setHorizontalHeaderLabels({
            QStringLiteral("文件名"),
            QStringLiteral("大小"),
            QStringLiteral("保存路径"),
            QStringLiteral("状态"),
        });
        m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
        m_table->setColumnWidth(1, 76);
        m_table->setColumnWidth(3, 110);
        pl->addWidget(m_table);

        // overall progress (hidden until download starts)
        m_overallBar = new QProgressBar(page);
        m_overallBar->setStyleSheet(PROGRESS_STYLE);
        m_overallBar->setRange(0, 100);
        m_overallBar->setValue(0);
        m_overallBar->setTextVisible(true);
        m_overallBar->setFixedHeight(18);
        m_overallBar->hide();
        pl->addWidget(m_overallBar);

        m_overallLabel = new QLabel(page);
        m_overallLabel->setStyleSheet(
            QString("color:%1; font-size:11px;").arg(CLR_TXT_SEC));
        m_overallLabel->hide();
        pl->addWidget(m_overallLabel);

        m_stack->addWidget(page);   // idx 2
    }

    // ── buttons ────────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout;
    btnRow->setSpacing(10);

    m_downloadBtn = new QPushButton(QStringLiteral("⬇  开始下载"), outer);
    m_downloadBtn->setObjectName("omdDownload");
    m_downloadBtn->setStyleSheet(BTN_DOWNLOAD_STYLE);
    m_downloadBtn->setEnabled(false);
    connect(m_downloadBtn, &QPushButton::clicked, this, &OcrModelDownloadDialog::onStartDownload);

    m_closeBtn = new QPushButton(QStringLiteral("关闭"), outer);
    m_closeBtn->setObjectName("omdClose");
    m_closeBtn->setStyleSheet(BTN_CLOSE_STYLE);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::reject);

    btnRow->addStretch();
    btnRow->addWidget(m_downloadBtn);
    btnRow->addWidget(m_closeBtn);
    vl->addLayout(btnRow);
}

// ── networking ────────────────────────────────────────────────────────────────

QString OcrModelDownloadDialog::buildApiUrl() const
{
    QString base = ConfigManager::instance().apiBaseUrl();
    if (!base.endsWith(QLatin1Char('/')))
        base += QLatin1Char('/');
    QString url = base + QStringLiteral("update/ocr-models/");
    if (m_serverOnly)
        url += QStringLiteral("?tier=server");
    return url;
}

void OcrModelDownloadDialog::fetchModelList()
{
    setState(DlgState::Fetching);
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }

    const QString urlStr = buildApiUrl();
    QUrl fetchUrl(urlStr);
    QNetworkRequest req;
    req.setUrl(fetchUrl);
    req.setRawHeader("Accept", "application/json");
    req.setRawHeader("User-Agent",
        (ConfigManager::instance().appName() + QLatin1Char('/') +
         ConfigManager::instance().appVersion()).toUtf8());

    if (ConfigManager::instance().insecureTls() &&
        fetchUrl.scheme() == QLatin1String("https")) {
        QSslConfiguration ssl = req.sslConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        req.setSslConfiguration(ssl);
    }

    m_reply = m_nam->get(req);
    connect(m_reply, &QNetworkReply::finished,
            this, &OcrModelDownloadDialog::onFetchFinished);
}

void OcrModelDownloadDialog::onRetryFetch()
{
    fetchModelList();
}

void OcrModelDownloadDialog::onFetchFinished()
{
    if (!m_reply) return;

    const QNetworkReply::NetworkError err = m_reply->error();
    const QString errStr = m_reply->errorString();
    const QByteArray body = m_reply->readAll();
    const int httpStatus  = m_reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    m_reply->deleteLater();
    m_reply = nullptr;

    if (err != QNetworkReply::NoError) {
        m_fetchErrMsg = QStringLiteral(
            "无法连接到服务端接口：\n%1\n\n"
            "接口地址：%2\n\n"
            "请确认本机已启动 FileTran Django 服务（python manage.py runserver），\n"
            "或在 config.ini [Network] 中设置正确的 ApiBaseUrl。")
            .arg(errStr, buildApiUrl());
        setState(DlgState::FetchError);
        return;
    }

    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(body, &pe);
    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        m_fetchErrMsg = QStringLiteral(
            "服务端返回无效 JSON（HTTP %1）。\n\n接口地址：%2\n\n%3")
            .arg(httpStatus)
            .arg(buildApiUrl())
            .arg(QString::fromUtf8(body.left(300)));
        setState(DlgState::FetchError);
        return;
    }

    const QJsonArray files = doc.object().value(QStringLiteral("files")).toArray();
    if (files.isEmpty()) {
        m_fetchErrMsg = QStringLiteral(
            "服务端接口正常，但未返回任何模型文件记录。\n\n"
            "请在管理后台（/admin）同步 Gitee OCR 模型地址后重试。\n\n"
            "接口地址：%1").arg(buildApiUrl());
        setState(DlgState::FetchError);
        return;
    }

    m_files.clear();
    for (const QJsonValue& v : files) {
        const QJsonObject o = v.toObject();
        FileEntry e;
        e.name        = o.value(QStringLiteral("name")).toString();
        e.relativePath = o.value(QStringLiteral("path")).toString();
        e.downloadUrl = o.value(QStringLiteral("url")).toString();
        e.size        = o.value(QStringLiteral("size")).toVariant().toLongLong();
        if (!e.name.isEmpty() && !e.downloadUrl.isEmpty())
            m_files.append(e);
    }

    if (m_files.isEmpty()) {
        m_fetchErrMsg = QStringLiteral("服务端文件列表中无有效条目（name / url 缺失）。");
        setState(DlgState::FetchError);
        return;
    }

    setState(DlgState::Ready);
}

// ── download logic ────────────────────────────────────────────────────────────

void OcrModelDownloadDialog::onStartDownload()
{
    if (m_files.isEmpty()) return;

    // Build the pending list:
    //   - First run (Ready) → all files
    //   - Retry (Done with failures) → only failed rows
    m_pendingIndices.clear();
    if (m_state == DlgState::Done) {
        // Scan table for failed rows
        for (int i = 0; i < m_files.size(); ++i) {
            auto* it = m_table->item(i, 3);
            if (it && it->text().startsWith(QStringLiteral("失败")))
                m_pendingIndices.append(i);
        }
    } else {
        for (int i = 0; i < m_files.size(); ++i)
            m_pendingIndices.append(i);
    }

    if (m_pendingIndices.isEmpty()) return;

    m_batchSize = m_pendingIndices.size();
    m_batchDone = 0;

    m_overallBar->setRange(0, m_batchSize);
    m_overallBar->setValue(0);
    m_overallBar->setFormat(QStringLiteral("0 / %1 个文件").arg(m_batchSize));
    m_overallBar->show();
    m_overallLabel->show();
    m_downloadBtn->setEnabled(false);
    m_closeBtn->setEnabled(false);
    setState(DlgState::Downloading);
    downloadNext();
}

void OcrModelDownloadDialog::downloadNext()
{
    if (m_pendingIndices.isEmpty()) {
        setState(DlgState::Done);
        return;
    }

    m_currentDownloadIdx = m_pendingIndices.takeFirst();
    const FileEntry& e   = m_files[m_currentDownloadIdx];

    setRowStatus(m_currentDownloadIdx, QStringLiteral("下载中…"), QColor(CLR_WARN));

    // ensure target directory exists
    const QString filePath = targetPath(e);
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    const QString tmpPath = filePath + QStringLiteral(".downloading");
    QFile::remove(tmpPath);

    // open the tmp file explicitly (NOT parented to the reply)
    closeCurrentFile();
    m_currentFile = new QFile(tmpPath);
    if (!m_currentFile->open(QIODevice::WriteOnly)) {
        delete m_currentFile;
        m_currentFile = nullptr;
        setRowStatus(m_currentDownloadIdx,
            QStringLiteral("失败：无法创建本地文件 %1").arg(tmpPath),
            QColor(CLR_ERR),
            QStringLiteral("目标路径：%1\n请检查磁盘空间和写入权限").arg(filePath));
        downloadNext();
        return;
    }

    QUrl dlUrl(e.downloadUrl);
    QNetworkRequest req;
    req.setUrl(dlUrl);
    req.setTransferTimeout(0); // disable timeout for large file downloads

    // Simulate a real browser request to avoid Gitee 403 / hot-link protection
    req.setRawHeader("User-Agent",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
        "AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/124.0.0.0 Safari/537.36");
    req.setRawHeader("Accept",
        "text/html,application/xhtml+xml,application/xml;"
        "q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8");
    req.setRawHeader("Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8");
    req.setRawHeader("Referer",         "https://gitee.com/");
    req.setRawHeader("Connection",      "keep-alive");

    if (ConfigManager::instance().insecureTls() &&
        dlUrl.scheme() == QLatin1String("https")) {
        QSslConfiguration ssl = req.sslConfiguration();
        ssl.setPeerVerifyMode(QSslSocket::VerifyNone);
        req.setSslConfiguration(ssl);
    }

    m_reply = m_nam->get(req);

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_reply && m_currentFile)
            m_currentFile->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress,
            this, &OcrModelDownloadDialog::onFileProgress);
    connect(m_reply, &QNetworkReply::finished,
            this, &OcrModelDownloadDialog::onFileDownloadFinished);
}

void OcrModelDownloadDialog::onFileProgress(qint64 recv, qint64 total)
{
    if (m_currentDownloadIdx < 0) return;
    QString text;
    if (total > 0) {
        const int pct = int(recv * 100 / total);
        text = QStringLiteral("%1%  (%2/%3 MB)")
            .arg(pct)
            .arg(recv  / 1024.0 / 1024.0, 0, 'f', 1)
            .arg(total / 1024.0 / 1024.0, 0, 'f', 1);
    } else {
        text = QStringLiteral("%1 MB").arg(recv / 1024.0 / 1024.0, 0, 'f', 1);
    }
    setRowStatus(m_currentDownloadIdx, text, QColor(CLR_WARN));
}

void OcrModelDownloadDialog::onFileDownloadFinished()
{
    if (!m_reply) return;

    // Read reply info before deleteLater
    const QNetworkReply::NetworkError err = m_reply->error();
    const QString errStr  = m_reply->errorString();
    const int httpStatus  = m_reply->attribute(
        QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QUrl finalUrl   = m_reply->url();
    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_currentDownloadIdx < 0) return;

    const FileEntry& e    = m_files[m_currentDownloadIdx];
    const QString filePath = targetPath(e);
    const QString tmpPath  = filePath + QStringLiteral(".downloading");

    // MUST close the file explicitly BEFORE rename (not relying on deleteLater)
    closeCurrentFile();

    if (err == QNetworkReply::NoError) {
        // Verify the downloaded file has a reasonable size
        const qint64 tmpSize = QFileInfo(tmpPath).size();
        if (tmpSize < 1024) {
            // Suspiciously small - probably an error page
            QFile tmpFile(tmpPath);
            QByteArray content;
            if (tmpFile.open(QIODevice::ReadOnly))
                content = tmpFile.read(512);
            tmpFile.close();
            QFile::remove(tmpPath);

            const QString hint = content.isEmpty()
                ? QStringLiteral("服务端返回空内容")
                : QStringLiteral("服务端返回非模型内容：%1").arg(
                      QString::fromUtf8(content).trimmed().left(200));
            const QString tooltip = QStringLiteral(
                "HTTP 状态: %1\nURL: %2\n\n%3\n\n"
                "可能原因：Gitee 下载链接失效或需要登录访问。\n"
                "请在管理后台重新同步 OCR 模型地址。")
                .arg(httpStatus).arg(finalUrl.toString()).arg(hint);
            setRowStatus(m_currentDownloadIdx,
                QStringLiteral("失败：响应内容无效"), QColor(CLR_ERR), tooltip);
        } else {
            QFile::remove(filePath);
            if (QFile::rename(tmpPath, filePath)) {
                const qint64 actual = QFileInfo(filePath).size();
                setRowStatus(m_currentDownloadIdx,
                    QStringLiteral("✓  已完成 (%1 MB)").arg(
                        actual / 1024.0 / 1024.0, 0, 'f', 2),
                    QColor(CLR_OK));
                ++m_batchDone;
            } else {
                setRowStatus(m_currentDownloadIdx,
                    QStringLiteral("失败：无法重命名临时文件"), QColor(CLR_ERR),
                    QStringLiteral("临时文件：%1\n目标文件：%2").arg(tmpPath, filePath));
            }
        }
    } else {
        QFile::remove(tmpPath);
        const QString tooltip = QStringLiteral(
            "错误代码: %1\n详情: %2\nHTTP 状态: %3\nURL: %4\n\n"
            "常见原因：\n"
            "• Gitee 大文件限速或需要登录\n"
            "• 网络连接中断\n"
            "• 服务端 URL 记录已失效，请重新同步")
            .arg(int(err)).arg(errStr).arg(httpStatus).arg(finalUrl.toString());
        setRowStatus(m_currentDownloadIdx,
            QStringLiteral("失败：%1").arg(errStr),
            QColor(CLR_ERR), tooltip);
    }

    // Update batch progress
    const int batchCompleted = m_batchSize - m_pendingIndices.size();
    m_overallBar->setValue(batchCompleted);
    m_overallBar->setFormat(
        QStringLiteral("%1 / %2 个文件").arg(batchCompleted).arg(m_batchSize));
    if (!m_pendingIndices.isEmpty()) {
        m_overallLabel->setText(
            QStringLiteral("正在下载第 %1 个，共 %2 个文件…")
            .arg(batchCompleted + 1).arg(m_batchSize));
    }

    m_currentDownloadIdx = -1;
    downloadNext();
}

// ── state machine ──────────────────────────────────────────────────────────

void OcrModelDownloadDialog::setState(DlgState s)
{
    m_state = s;
    switch (s) {
    case DlgState::Fetching:
        m_stack->setCurrentIndex(0);
        m_downloadBtn->setEnabled(false);
        m_closeBtn->setEnabled(true);
        adjustSize();
        break;

    case DlgState::FetchError:
        m_errLabel->setText(m_fetchErrMsg);
        m_stack->setCurrentIndex(1);
        m_downloadBtn->setEnabled(false);
        m_closeBtn->setEnabled(true);
        adjustSize();
        break;

    case DlgState::Ready:
        populateTable();
        m_stack->setCurrentIndex(2);
        m_downloadBtn->setEnabled(true);
        m_downloadBtn->setText(QStringLiteral("⬇  开始下载  (%1 个文件)").arg(m_files.size()));
        m_closeBtn->setEnabled(true);
        adjustSize();
        break;

    case DlgState::Downloading:
        m_stack->setCurrentIndex(2);
        m_downloadBtn->setEnabled(false);
        m_closeBtn->setEnabled(false);
        break;

    case DlgState::Done: {
        // Count by scanning table (reliable regardless of how we got here)
        const int totalFailed = countFailedRows();
        const int totalOk     = m_files.size() - totalFailed;
        m_overallBar->setValue(m_batchSize);
        m_overallBar->setFormat(QStringLiteral("完成"));
        if (totalFailed == 0) {
            m_overallLabel->setText(
                QStringLiteral("✓  全部 %1 个文件下载成功，请关闭窗口后重新点击「开始转换」")
                .arg(m_files.size()));
            m_overallLabel->setStyleSheet(
                QString("color:%1; font-size:12px; font-weight:600;").arg(CLR_OK));
            m_downloadBtn->setEnabled(false);
        } else {
            m_overallLabel->setText(
                QStringLiteral("⚠  已完成 %1 个，失败 %2 个  （悬停失败行查看原因，可重试）")
                .arg(totalOk).arg(totalFailed));
            m_overallLabel->setStyleSheet(
                QString("color:%1; font-size:12px;").arg(CLR_WARN));
            m_downloadBtn->setText(
                QStringLiteral("⬇  重试失败项 (%1 个)").arg(totalFailed));
            m_downloadBtn->setEnabled(true);
        }
        m_closeBtn->setEnabled(true);
        m_closeBtn->setText(QStringLiteral("关闭"));
        break;
    }
    }
}

void OcrModelDownloadDialog::populateTable()
{
    m_table->setRowCount(m_files.size());
    for (int i = 0; i < m_files.size(); ++i) {
        const FileEntry& e = m_files[i];

        auto* nameItem = new QTableWidgetItem(e.name);
        nameItem->setForeground(QColor(CLR_TXT));
        m_table->setItem(i, 0, nameItem);

        QString sizeStr = e.size > 0
            ? QStringLiteral("%1 MB").arg(e.size / 1024.0 / 1024.0, 0, 'f', 1)
            : QStringLiteral("—");
        auto* sizeItem = new QTableWidgetItem(sizeStr);
        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        sizeItem->setForeground(QColor(CLR_TXT_SEC));
        m_table->setItem(i, 1, sizeItem);

        // show relative path (stripped leading "PP-OCRv5/" etc.)
        const QString rel = e.relativePath.isEmpty() ? e.name : e.relativePath;
        auto* pathItem = new QTableWidgetItem(rel);
        pathItem->setForeground(QColor(CLR_TXT_SEC));
        pathItem->setToolTip(targetPath(e));
        m_table->setItem(i, 2, pathItem);

        auto* statusItem = new QTableWidgetItem(QStringLiteral("待下载"));
        statusItem->setForeground(QColor(CLR_TXT_SEC));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 3, statusItem);

        m_table->setRowHeight(i, 30);
    }

    // dynamic height: 8 rows max visible, then scroll
    const int rowH    = 30;
    const int headH   = m_table->horizontalHeader()->height();
    const int visible = qMin(m_files.size(), 8);
    m_table->setFixedHeight(headH + visible * rowH + 2);
}

// ── helpers ────────────────────────────────────────────────────────────────

void OcrModelDownloadDialog::closeCurrentFile()
{
    if (m_currentFile) {
        m_currentFile->flush();
        m_currentFile->close();
        delete m_currentFile;
        m_currentFile = nullptr;
    }
}

int OcrModelDownloadDialog::countFailedRows() const
{
    int cnt = 0;
    for (int i = 0; i < m_table->rowCount(); ++i) {
        const auto* it = m_table->item(i, 3);
        if (it && it->text().startsWith(QStringLiteral("失败")))
            ++cnt;
    }
    return cnt;
}

void OcrModelDownloadDialog::setRowStatus(int row, const QString& text,
                                          const QColor& color, const QString& tooltip)
{
    if (row < 0 || row >= m_table->rowCount()) return;
    auto* it = m_table->item(row, 3);
    if (!it) {
        it = new QTableWidgetItem;
        it->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(row, 3, it);
    }
    it->setText(text);
    it->setForeground(color.isValid() ? color : QColor(CLR_TXT_SEC));
    it->setToolTip(tooltip.isEmpty() ? text : tooltip);
}

QString OcrModelDownloadDialog::targetPath(const FileEntry& e) const
{
    const QString base = OcrModelHelper::modelsDir();
    // Use relativePath as sub-path if available, else just filename
    const QString rel = e.relativePath.isEmpty() ? e.name : e.relativePath;
    return base + QLatin1Char('/') + rel;
}

// ── drag support ─────────────────────────────────────────────────────────────

void OcrModelDownloadDialog::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragPos  = e->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QDialog::mousePressEvent(e);
}

void OcrModelDownloadDialog::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging && (e->buttons() & Qt::LeftButton))
        move(e->globalPosition().toPoint() - m_dragPos);
    QDialog::mouseMoveEvent(e);
}

void OcrModelDownloadDialog::mouseReleaseEvent(QMouseEvent* e)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(e);
}
