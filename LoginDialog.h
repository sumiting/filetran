#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QShowEvent>
#include <QResizeEvent>
#include <memory>

// Windows / WebView2 headers
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

/**
 * LoginDialog
 *
 * 使用 Microsoft Edge WebView2 内嵌服务端网页登录/注册界面。
 * 用户在网页完成操作后，服务端渲染 desktop_callback.html，
 * 通过 window.chrome.webview.postMessage 将 JWT 令牌和用户信息
 * 传回客户端，触发 loginSuccess 信号。
 */
class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget* parent = nullptr);
    ~LoginDialog() override;

    /** 打开时默认导航到注册页（用于游客额度耗尽后引导注册）*/
    void openOnRegisterPage();

signals:
    void loginSuccess(const QJsonObject& userData,
                      const QString& accessToken,
                      const QString& refreshToken);

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initWebView();
    void updateBounds();

    QString m_loginUrl;     // /login/?desktop=1&machine_id=...
    QString m_registerUrl;  // /register/?desktop=1&machine_id=...
    QString m_initialUrl;   // 首次加载的 URL（可被 openOnRegisterPage 切换）
    bool    m_initDone   = false;
    int     m_retryCount = 0;               // WebView2 初始化失败自动重试次数
    static constexpr int kMaxWv2Retries = 3; // 最多重试 3 次，间隔 1.5 s

    // 用 shared_ptr<bool> 标记对话框存活状态，避免异步回调访问已销毁的 this
    std::shared_ptr<bool> m_alive;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2>           m_webView;
    EventRegistrationToken                           m_msgToken{};
};
