#pragma once

#include <QMainWindow>
#include <QStackedWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QProgressBar>
#include <QRadioButton>
#include <QButtonGroup>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QStyledItemDelegate>
#include <QCheckBox>
#include <QVector>
#include <QHash>
#include <QFileInfo>
#include <QPoint>
#include <QTimer>

class TitleBar;
class FileProcessor;
class CustomerServiceDialog;

// ── 左侧导航委托（一级 15px 主色 / 二级 13px 辅色 / 选中强调条可呼吸）──
class NavListDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit NavListDelegate(QListWidget* list);
    void  paint(QPainter* painter,
                const QStyleOptionViewItem& option,
                const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index)         const override;

private:
    QListWidget* m_list = nullptr;
};

// ── 主窗口 ────────────────────────────────────────────────────────────
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void dragEnterEvent(QDragEnterEvent* event)  override;
    void dropEvent(QDropEvent* event)            override;
    void keyPressEvent(QKeyEvent* event)         override;
    void closeEvent(QCloseEvent* event)          override;
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

private slots:
    // 标题栏
    void onLoginClicked();
    void onLogoutRequested();
    void onUpgradeRequested();
    void onMenuAction(const QString& action);
    void onMinimize();
    void onMaximize();

    // 导航
    void onNavItemClicked(QListWidgetItem* item);
    void onHighPrecisionToggled(bool checked);
    void onMergeFileToggled(bool checked);
    void onCustomerServiceClicked();

    // 文件操作
    void onAddFiles();
    void onAddFolder();
    void onClearFiles();
    void onCellClicked(int row, int col);

    // 转换
    void onStartConversion();
    void onCancelConversion();
    void onFileCompleted(const QString& filePath,
                         const QString& outputPath,
                         bool success,
                         const QString& errMsg);
    void onAllFinished(int successCount, int failedCount);
    void onOcrModelsMissing();

    // FileProcessor 回调
    void onFilesReady(const QVector<QFileInfo>& batch);

    // 输出路径
    void onOutputModeChanged(int id);
    void onBrowseOutputDir();

    // UserManager 回调
    void onAutoLoginSuccess(const QJsonObject& userInfo);
    void onProfileUpdated(const QJsonObject& userInfo);
    void onLoggedOut();

    // UpdateManager 回调
    void onUpdateAvailable(const QString& ver, const QString& notes,
                           const QString& url, const QString& sha256,
                           bool forceUpdate);
    void onDownloadCompleted(const QString& zipPath);
    void onInstallCompleted();

    // NoticeManager 回调
    void onNoticeAvailable(int noticeId, const QString& title, const QString& content);

private:
    void setupUi();
    void buildNavList();
    void buildFileTable();
    QString formatFileSize(qint64 bytes) const;
    void addFileRow(const QFileInfo& fi);
    bool fileAlreadyAdded(const QString& path) const;
    void updateFileStatus(const QString& filePath,
                          const QString& text,
                          const QColor& color);
    QStringList checkedFilePaths() const;
    QStringList acceptedExtForCurrentType() const;
    QString outputDirForFile(const QString& filePath) const;
    void selectFirstSubItem();
    bool isImageOcrType() const;
    void updateHighPrecisionVisibility();
    void updateMergeVisibility();
    void showAboutDialog();
    void onNavPulseTick();

    // ── UI 组件 ───────────────────────────────────────────────────
    TitleBar*       m_titleBar;
    QListWidget*    m_navList;
    QTableWidget*   m_fileTable;
    QProgressBar*   m_progressBar;
    QLabel*         m_fileCountLabel;
    QCheckBox*      m_highPrecisionCb = nullptr;
    QCheckBox*      m_mergeFileCb    = nullptr;  // 合并为一个文件 토글
    bool            m_pendingMerge   = false;    // 当前转换任务是否需要合并
    QLabel*         m_statusLabel;   // 状态栏左侧
    QPushButton*    m_startBtn;
    QPushButton*    m_cancelBtn;

    QButtonGroup*   m_outputGroup;
    QRadioButton*   m_sameDirRb;
    QRadioButton*   m_desktopRb;
    QRadioButton*   m_customRb;
    QPushButton*    m_browseBtn;       // 选择目录时的浏览按钮
    QLabel*         m_outputPathLabel; // 持续显示当前输出路径
    QLabel*         m_customDirLabel;  // 保留兼容（内部不再使用）

    // ── 状态 ─────────────────────────────────────────────────────
    QString         m_currentConvType;   // e.g. "PDF转Word"
    int             m_outputMode = 0;    // 0=同级 1=桌面 2=自定义
    QString         m_customOutputDir;
    bool            m_isConverting = false;

    // 文件路径 -> 行号 映射（转换完成后定位更新行状态）
    QHash<QString, int> m_fileRowMap;
    // 行号 -> 输出路径（转换后用于打开文件/目录按钮）
    QHash<int, QString> m_rowOutputMap;

    FileProcessor*           m_fileProcessor;
    CustomerServiceDialog*   m_customerServiceDialog = nullptr;

    QPoint          m_dragPos;
    bool            m_isDragging = false;

    QTimer*         m_navPulseTimer = nullptr;
};
