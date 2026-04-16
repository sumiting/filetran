#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QPoint>

/**
 * NoticeDialog — 系统公告弹框
 *
 * 启动时由 NoticeManager 触发显示。
 * - 关闭按钮 / 直接关闭窗口：不标记已读，下次启动仍会弹出。
 * - "不再展示"按钮：调用 NoticeManager::markRead() 后关闭，之后不再弹出。
 */
class NoticeDialog : public QDialog {
    Q_OBJECT
public:
    explicit NoticeDialog(int noticeId,
                          const QString& title,
                          const QString& content,
                          QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onDismissClicked();

private:
    void setupUi(const QString& title, const QString& content);

    int         m_noticeId;
    QPoint      m_dragPos;
    bool        m_isDragging = false;
};
