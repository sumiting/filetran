#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QPoint>

// 忘记密码弹窗：填写用户名 -> 发验证码(60s倒计时) -> 填验证码+新密码 -> 重置
class ForgotPasswordDialog : public QDialog {
    Q_OBJECT
public:
    explicit ForgotPasswordDialog(QWidget* parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent* event)   override;
    void mouseMoveEvent(QMouseEvent* event)    override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private slots:
    void onSendCodeClicked();
    void onCodeTick();
    void onResetClicked();

    void handleSendCodeResponse(bool ok, const QJsonObject& resp, const QString& err);
    void handleResetResponse  (bool ok, const QJsonObject& resp, const QString& err);

private:
    void setupUi();

    QLineEdit*   m_usernameEdit;
    QLineEdit*   m_codeEdit;
    QLineEdit*   m_newPwdEdit;
    QLineEdit*   m_confirmPwdEdit;
    QPushButton* m_sendCodeBtn;
    QPushButton* m_resetBtn;
    QLabel*      m_statusLabel;

    QTimer*      m_codeTimer;
    int          m_countdown = 0;

    QPoint       m_dragPos;
    bool         m_isDragging = false;
};
