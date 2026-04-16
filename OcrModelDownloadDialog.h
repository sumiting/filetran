#pragma once

#include <QDialog>
#include <QFile>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPoint>
#include <QString>

class QLabel;
class QTableWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;

/// 对话框状态机
enum class DlgState {
    Fetching,       // 正在拉取模型列表
    FetchError,     // 拉取失败
    Ready,          // 已显示文件列表，等待用户点击下载
    Downloading,    // 下载中
    Done,           // 全部完成
};

class OcrModelDownloadDialog : public QDialog {
    Q_OBJECT
public:
    /// serverOnly=true：仅拉取 server 高精度模型（?tier=server），供会员按需下载
    explicit OcrModelDownloadDialog(QWidget* parent = nullptr, bool serverOnly = false);

protected:
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onFetchFinished();
    void onFileProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFileDownloadFinished();
    void onStartDownload();
    void onRetryFetch();

private:
    struct FileEntry {
        QString name;           // e.g. "ch_PP-OCRv4_det_infer.onnx"
        QString relativePath;   // e.g. "PP-OCRv4/det/ch_PP-OCRv4_det_infer.onnx"
        QString downloadUrl;    // direct download link
        qint64  size = 0;       // bytes, 0 = unknown
    };

    void setupUi();
    QString buildApiUrl() const;
    void fetchModelList();
    void setState(DlgState s);
    void populateTable();
    void downloadNext();
    void closeCurrentFile();                // flush + close + delete m_currentFile
    void setRowStatus(int row, const QString& text, const QColor& color,
                      const QString& tooltip = QString());
    QString targetPath(const FileEntry& e) const;
    int countFailedRows() const;           // scan table for rows starting with "失败"

    // ── data ──────────────────────────────────────────────────────
    bool               m_serverOnly = false;
    QList<FileEntry>   m_files;
    QList<int>         m_pendingIndices;    // file indices queued for this batch
    int                m_currentDownloadIdx = -1; // index in m_files being downloaded
    int                m_batchSize   = 0;   // total files in current batch
    int                m_batchDone   = 0;   // completed (success) in current batch
    DlgState           m_state       = DlgState::Fetching;
    QNetworkAccessManager* m_nam     = nullptr;
    QNetworkReply*     m_reply       = nullptr;
    QFile*             m_currentFile = nullptr; // explicitly managed, NOT child of reply
    QString            m_fetchErrMsg;

    // ── UI ────────────────────────────────────────────────────────
    QLabel*        m_titleLabel   = nullptr;
    QStackedWidget* m_stack       = nullptr;
    // page 0: fetching
    QLabel*        m_fetchingLabel = nullptr;
    // page 1: error
    QLabel*        m_errLabel      = nullptr;
    QPushButton*   m_retryBtn      = nullptr;
    // page 2: file list + download
    QTableWidget*  m_table         = nullptr;
    QProgressBar*  m_overallBar    = nullptr;
    QLabel*        m_overallLabel  = nullptr;

    QPushButton*   m_downloadBtn  = nullptr;
    QPushButton*   m_closeBtn     = nullptr;

    // drag support (frameless)
    QPoint  m_dragPos;
    bool    m_dragging = false;
};
