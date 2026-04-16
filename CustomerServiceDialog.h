#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QPoint>

/**
 * FileTran 智能客服对话框
 *
 * 功能：
 *  - 接受用户输入，通过 POST /api/v1/support/chat/ 调用服务端 DeepSeek RAG 客服
 *  - 展示对话气泡（用户右对齐蓝色，客服左对齐灰色）
 *  - 维护最近 10 轮（20 条）对话历史随请求一并上传
 *  - 发送中禁用输入，并显示"正在思考…"占位气泡
 */
class CustomerServiceDialog : public QDialog {
    Q_OBJECT

public:
    explicit CustomerServiceDialog(QWidget* parent = nullptr);
    ~CustomerServiceDialog() override = default;

protected:
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event)       override;

private slots:
    void onSendClicked();
    void handleChatReply(bool ok, const QJsonObject& resp, const QString& err);

private:
    void setupUi();
    void appendBubble(const QString& text, bool isUser, bool isThinking = false);
    void scrollToBottom();
    void setInputEnabled(bool enabled);

    // ── 控件 ─────────────────────────────────────────────────────────────
    QScrollArea*  m_scrollArea     = nullptr;
    QWidget*      m_bubbleContainer = nullptr;
    QVBoxLayout*  m_bubbleLayout   = nullptr;
    QLineEdit*    m_input          = nullptr;
    QPushButton*  m_sendBtn        = nullptr;
    QLabel*       m_thinkingBubble = nullptr; // 指向当前"正在思考"气泡

    // ── 对话历史（最多 20 条 = 10 轮）────────────────────────────────────
    QJsonArray m_history;

    // ── 拖动 ─────────────────────────────────────────────────────────────
    QPoint m_dragPos;
    bool   m_isDragging = false;
};
