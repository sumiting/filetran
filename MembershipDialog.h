#pragma once

#include <QDialog>
#include <QShowEvent>
#include <QResizeEvent>
#include <memory>

// Windows / WebView2 headers
#include <windows.h>
#include <wrl.h>
#include <WebView2.h>

/**
 * MembershipDialog
 *
 * 使用 Microsoft Edge WebView2 内嵌服务端支付页面（/pay/desktop/）。
 * C++ 将 JWT Access Token 作为 URL 参数传给网页，网页自行调用 REST API
 * 完成套餐展示、Native 二维码生成、订单轮询等逻辑。
 * 支付成功后网页通过 window.chrome.webview.postMessage 发送
 *   {"type":"payment_success"}
 * 客户端收到后 emit paymentSuccess() 信号并关闭对话框。
 */
class MembershipDialog : public QDialog {
    Q_OBJECT
public:
    explicit MembershipDialog(QWidget* parent = nullptr);
    ~MembershipDialog() override;

signals:
    void paymentSuccess();

protected:
    void showEvent(QShowEvent* event)   override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void initWebView();
    void updateBounds();

    QString m_payUrl;    // /pay/desktop/?token=<access_token>
    bool    m_initDone   = false;
    int     m_retryCount = 0;               // WebView2 初始化失败自动重试次数
    static constexpr int kMaxWv2Retries = 3; // 最多重试 3 次，间隔 1.5 s

    // 用 shared_ptr<bool> 标记对话框存活状态，避免异步回调访问已销毁的 this
    std::shared_ptr<bool> m_alive;

    Microsoft::WRL::ComPtr<ICoreWebView2Controller> m_controller;
    Microsoft::WRL::ComPtr<ICoreWebView2>           m_webView;
    EventRegistrationToken                           m_msgToken{};
};
