#include "LoginDialog.h"
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
#include <QDesktopServices>

// ── 构造/析构 ─────────────────────────────────────────────────────────

LoginDialog::LoginDialog(QWidget* parent)
    : QDialog(parent)
    , m_alive(std::make_shared<bool>(true))
{
    setWindowTitle("FileTran - 登录");
    setWindowFlags(Qt::Dialog | Qt::WindowCloseButtonHint);
    setFixedSize(480, 620);

    // WebView2 需要原生 HWND
    setAttribute(Qt::WA_NativeWindow);

    // 加载提示（WebView2 初始化完成后被覆盖）
    auto* lay  = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto* hint = new QLabel("正在加载登录页面...", this);
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet(
        "color:#888; font-size:13px;"
        "background:#0d1117; color:#8b949e;");
    lay->addWidget(hint);

    // ── 构造 URL ──────────────────────────────────────────────────────
    // apiBaseUrl 形如 "http://127.0.0.1:8000/api/v1/" 或
    //                  "https://www.filetran.cn/api/v1/"
    // webBase 去掉 "/api/v1/" 后缀得到网站根 URL。
    QString apiUrl = ConfigManager::instance().apiBaseUrl();
    if (apiUrl.endsWith("/api/v1/"))
        apiUrl.chop(8);
    else if (apiUrl.endsWith("/api/v1"))
        apiUrl.chop(7);
    if (apiUrl.endsWith('/'))
        apiUrl.chop(1);

    const QString mid  = QString::fromUtf8(
        QUrl::toPercentEncoding(UserManager::instance().machineId()));
    const QString mnam = QString::fromUtf8(
        QUrl::toPercentEncoding(UserManager::instance().machineName()));
    const QString params = QString("?machine_id=%1&machine_name=%2")
                           .arg(mid, mnam);

    // 专用桌面登录页（深色主题，不含网站导航）
    m_loginUrl    = apiUrl + "/login/desktop/" + params;
    m_registerUrl = apiUrl + "/login/desktop/?tab=register&machine_id=" + mid + "&machine_name=" + mnam;
    m_initialUrl  = m_loginUrl;
}

LoginDialog::~LoginDialog()
{
    *m_alive = false;

    if (m_webView && m_msgToken.value)
        m_webView->remove_WebMessageReceived(m_msgToken);
    if (m_controller)
        m_controller->Close();
}

// ── 公共方法 ──────────────────────────────────────────────────────────

void LoginDialog::openOnRegisterPage()
{
    if (m_webView) {
        // WebView2 已就绪：通过 JS 切换到注册 tab，无需整页重载
        m_webView->ExecuteScript(L"showPage('register');", nullptr);
    } else {
        // 尚未初始化：设置首次加载 URL 带 ?tab=register
        m_initialUrl = m_registerUrl;
    }
}

// ── 事件 ─────────────────────────────────────────────────────────────

void LoginDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    if (!m_initDone) {
        m_initDone = true;
        // 推迟到事件循环，确保 HWND 已创建
        QTimer::singleShot(0, this, &LoginDialog::initWebView);
    }
}

void LoginDialog::resizeEvent(QResizeEvent* event)
{
    QDialog::resizeEvent(event);
    updateBounds();
}

void LoginDialog::updateBounds()
{
    if (!m_controller) return;
    RECT rc{ 0, 0, width(), height() };
    m_controller->put_Bounds(rc);
}

// ── WebView2 初始化 ───────────────────────────────────────────────────

void LoginDialog::initWebView()
{
    HWND hwnd = reinterpret_cast<HWND>(winId());

    auto alive      = m_alive;
    QString initUrl = m_initialUrl;

    // WebView2 的 userDataFolder 必须指向可写目录，否则安装到 Program Files
    // 等受保护路径时会因权限不足返回 E_ACCESSDENIED (0x80070005)。
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
                            QTimer::singleShot(1500, this, &LoginDialog::initWebView);
                            return;
                        }
                        QMessageBox::critical(
                            this, "WebView2 错误",
                            QString("WebView2 运行环境创建失败（请确保已安装 Edge WebView2 Runtime）\n"
                                    "HRESULT = 0x%1")
                            .arg(static_cast<unsigned>(result), 8, 16,
                                 QLatin1Char('0')));
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

                            // ── WebMessage 接收器（登录成功回调）────────
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

                                                QJsonObject obj = doc.object();
                                                QString type = obj.value("type").toString();

                                                if (type == "login_success") {
                                                    QString access  =
                                                        obj.value("access").toString();
                                                    QString refresh =
                                                        obj.value("refresh").toString();

                                                    // 将 membership / device_limit
                                                    // 合并进 user 对象，与旧接口保持一致
                                                    QJsonObject user =
                                                        obj.value("user").toObject();
                                                    if (obj.contains("membership"))
                                                        user["membership"] =
                                                            obj.value("membership");
                                                    if (obj.contains("device_limit"))
                                                        user["device_limit"] =
                                                            obj.value("device_limit");

                                                    if (!access.isEmpty()) {
                                                        emit loginSuccess(user, access, refresh);
                                                        accept();
                                                    }

                                                } else if (type == "error") {
                                                    QString errMsg =
                                                        obj.value("message").toString();
                                                    QString errCode =
                                                        obj.value("code").toString();
                                                    qDebug() << "[LoginDialog] error from web:"
                                                             << errMsg;

                                                    if (errCode == "device_limit_exceeded") {
                                                        const QString websiteUrl =
                                                            "https://www.filetran.cn";
                                                        const QString loginUrl =
                                                            "https://www.filetran.cn/login/"
                                                            "?next=/profile/%23devices";
                                                        QDesktopServices::openUrl(
                                                            QUrl(loginUrl));
                                                        QMessageBox::warning(
                                                            this,
                                                            "登录失败",
                                                            errMsg + "\n\n"
                                                            "官网地址：" + websiteUrl + "\n"
                                                            "（已自动在浏览器中打开官网登录页，"
                                                            "请解绑旧设备后重试）");
                                                    } else {
                                                        QMessageBox::warning(
                                                            this, "登录失败", errMsg);
                                                    }
                                                }
                                            },
                                            Qt::QueuedConnection);

                                        return S_OK;
                                    }
                                ).Get(),
                                &m_msgToken
                            );

                            // 导航到登录页
                            // 注：服务端 desktop_login_page 在每次加载时会主动
                            //     调用 logout() 清理旧 session，无需在此清 Cookie
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
            QTimer::singleShot(1500, this, &LoginDialog::initWebView);
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
