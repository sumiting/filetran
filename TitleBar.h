#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QPoint>
#include <QEvent>
#include <QJsonObject>
#include <QPixmap>

// 自定义深色标题栏
// 左: Logo + 应用名  中: 伸展  右: 圆形头像/登录 + 汉堡菜单 + 最小/最大/关闭
class TitleBar : public QWidget {
    Q_OBJECT
public:
    explicit TitleBar(QWidget* parent = nullptr);

    // 切换登录态（未登录 → 显示人形图标）
    void setLoginState();
    // 切换用户态（已登录 → 显示首字母头像 + 用户名）
    void setUserState(const QJsonObject& userInfo);
    // 通知当前窗口是否最大化，用于更新图标
    void setMaximized(bool maximized);

signals:
    void loginClicked();
    void logoutRequested();
    void upgradeRequested();
    void menuActionTriggered(const QString& action);
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();

protected:
    void mousePressEvent(QMouseEvent* event)       override;
    void mouseMoveEvent(QMouseEvent* event)        override;
    void mouseReleaseEvent(QMouseEvent* event)     override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event)            override;

private:
    void setupUi();
    void showHamburgerMenu();
    void showUserDropdown();
    void onAvatarClicked();

    // 绘制图标
    static QIcon makeAccountIcon(int sz, const QColor& color);
    static QIcon makeWinIcon(const QString& type, int sz, const QColor& color);

    QLabel*      m_logoLabel   = nullptr;
    QLabel*      m_titleLabel  = nullptr;

    // 统一的圆形头像/登录按钮
    QPushButton* m_avatarBtn   = nullptr;

    QPushButton* m_hamburgerBtn = nullptr;
    QPushButton* m_minBtn       = nullptr;
    QPushButton* m_maxBtn       = nullptr;
    QPushButton* m_closeBtn     = nullptr;

    bool    m_isLoggedIn = false;
    QString m_username;
    QString m_userType;
    QString m_userId;
    QString m_membershipExpireText;
    bool    m_isLifetimeMember = false;

    QPoint  m_dragPos;
    bool    m_isDragging = false;
    bool    m_maximized  = false;
};
