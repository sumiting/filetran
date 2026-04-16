#include "MembershipDialog.h"
#include "../core/ConfigManager.h"
#include "../user/UserManager.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QUrl>
#include <QTimer>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

// ── 构造/析构 ─────────────────────────────────────────────────────────

MembershipDialog::MembershipDialog(QWidget* parent)
    : QDialog(parent)
    , m_alive(std::make_shared<bool>(true))
{
    setWindowTitle("FileTran - 开通会员");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(820, 560);

    // WebView2 需要原生 HWND
    setAttribute(Qt::WA_NativeWindow);

    // 加载提示占位（WebView2 初始化完成后被覆盖）
    auto* lay  = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* hint = new QLabel("正在加载支付页面...", this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("background:#0d1117; color:#8b949e; font-size:13px;"
                        "font-family:'Microsoft YaHei','Segoe UI',sans-serif;");
    lay->addWidget(hint);

    // ── 构造支付页 URL ────────────────────────────────────────────────
    // apiBaseUrl 形如 "https://www.filetran.cn/api/v1/"
    // 去掉 "/api/v1/" 后缀得到网站根 URL
    QString apiUrl = ConfigManager::instance().apiBaseUrl();
    if (apiUrl.endsWith("/api/v1/"))  apiUrl.chop(8);
    else if (apiUrl.endsWith("/api/v1")) apiUrl.chop(7);
    if (apiUrl.endsWith('/'))         apiUrl.chop(1);

    // 将 JWT Access Token 以 URL 参数传给页面（页面通过 JS 读取并附到 API 请求头）
    const QString token = QString::fromUtf8(
        QUrl::toPercentEncoding(UserManager::instance().accessToken()));

    m_payUrl = apiUrl + "/pay/desktop/?token=" + token;
}

MembershipDialog::~MembershipDialog()
{
    *m_alive = false;

    if (m_webView && m_msgToken.value)
        m_webView->remove_WebMessageReceived(m_msgToken);
    if (m_controller)
        m_controller->Close();
}

// ── 事件 ─────────────────────────────────────────────────────────────

void MembershipDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (!m_initDone) {
        m_initDone = true;
        QTimer::singleShot(0, this, &MembershipDialog::initWebView);
    }
}

void MembershipDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateBounds();
}

void MembershipDialog::updateBounds()
{
    if (!m_controller) return;
    RECT rc{ 0, 0, width(), height() };
    m_controller->put_Bounds(rc);
}

// ── WebView2 初始化 ───────────────────────────────────────────────────

void MembershipDialog::initWebView()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());

    auto    alive   = m_alive;
    QString initUrl = m_payUrl;

    QString udf = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                  + QStringLiteral("/WebView2");
    QDir().mkpath(udf);
    std::wstring wUdf = udf.toStdWString();

    HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
        nullptr, wUdf.c_str(), nullptr,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this, alive, hwnd, initUrl]
            (HRESULT result, ICoreWebView2Environment* env) -> HRESULT
            {
                if (!*alive) return S_OK;

                if (FAILED(result) || !env) {
                    QMetaObject::invokeMethod(this, [this, alive, result]() {
                        if (!*alive) return;
                        if (m_retryCount < kMaxWv2Retries) {
                            ++m_retryCount;
                            QTimer::singleShot(1500, this, &MembershipDialog::initWebView);
                            return;
                        }
                        QMessageBox::critical(
                            this, "WebView2 错误",
                            QString("WebView2 运行环境创建失败\n"
                                    "请确保已安装 Edge WebView2 Runtime\n"
                                    "HRESULT = 0x%1")
                            .arg(static_cast<unsigned>(result), 8, 16, QLatin1Char('0')));
                        reject();
                    }, Qt::QueuedConnection);
                    return S_OK;
                }

                env->CreateCoreWebView2Controller(
                    hwnd,
                    Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                        [this, alive, initUrl]
                        (HRESULT result2, ICoreWebView2Controller* ctrl) -> HRESULT
                        {
                            if (!*alive) return S_OK;

                            if (FAILED(result2) || !ctrl) {
                                QMetaObject::invokeMethod(this, [this, alive]() {
                                    if (!*alive) return;
                                    QMessageBox::critical(this, "WebView2 错误",
                                                          "WebView2 控制器创建失败");
                                    reject();
                                }, Qt::QueuedConnection);
                                return S_OK;
                            }

                            m_controller = ctrl;
                            ctrl->get_CoreWebView2(&m_webView);

                            // 填满对话框
                            RECT rc{ 0, 0, width(), height() };
                            ctrl->put_Bounds(rc);
                            ctrl->put_IsVisible(TRUE);

                            // 隐藏右键菜单和状态栏
                            Microsoft::WRL::ComPtr<ICoreWebView2Settings> wv2Settings;
                            if (SUCCEEDED(m_webView->get_Settings(&wv2Settings)) && wv2Settings) {
                                wv2Settings->put_AreDefaultContextMenusEnabled(FALSE);
                                wv2Settings->put_IsStatusBarEnabled(FALSE);
                            }

                            // ── WebMessage 接收器 ──────────────────────
                            m_webView->add_WebMessageReceived(
                                Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                    [this, alive]
                                    (ICoreWebView2* /*sender*/,
                                     ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT
                                    {
                                        if (!*alive) return S_OK;

                                        LPWSTR msgW = nullptr;
                                        args->TryGetWebMessageAsString(&msgW);
                                        if (!msgW) return S_OK;

                                        QString msg = QString::fromWCharArray(msgW);
                                        CoTaskMemFree(msgW);

                                        QMetaObject::invokeMethod(
                                            this,
                                            [this, alive, msg]() {
                                                if (!*alive) return;

                                                QJsonDocument doc =
                                                    QJsonDocument::fromJson(msg.toUtf8());
                                                if (!doc.isObject()) return;

                                                const QString type =
                                                    doc.object().value("type").toString();

                                                if (type == "payment_success") {
                                                    QMessageBox::information(
                                                        this, "支付成功",
                                                        "会员已开通，感谢您的支持！");
                                                    emit paymentSuccess();
                                                    accept();

                                                } else if (type == "payment_close") {
                                                    reject();

                                                } else if (type == "payment_minimize") {
                                                    showMinimized();
                                                }
                                            },
                                            Qt::QueuedConnection);

                                        return S_OK;
                                    }
                                ).Get(),
                                &m_msgToken
                            );

                            // 导航到支付页
                            m_webView->Navigate(initUrl.toStdWString().c_str());

                            return S_OK;
                        }
                    ).Get()
                );
                return S_OK;
            }
        ).Get()
    );

    if (FAILED(hr)) {
        if (m_retryCount < kMaxWv2Retries) {
            ++m_retryCount;
            QTimer::singleShot(1500, this, &MembershipDialog::initWebView);
            return;
        }
        QMessageBox::critical(
            this, "WebView2 错误",
            QString("WebView2 初始化失败，HRESULT = 0x%1\n\n"
                    "请安装 Edge WebView2 Runtime：\n"
                    "https://developer.microsoft.com/en-us/microsoft-edge/webview2/")
            .arg(static_cast<unsigned>(hr), 8, 16, QLatin1Char('0')));
        reject();
    }
}
