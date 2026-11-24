#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QWebEngineNotification;

// Shows a site's web notifications, and the app's own messages, through the desktop's
// notification service (org.freedesktop.Notifications, which mako provides on Omarchy).
// Clicking one tells the page, as a browser would, and asks for its tab to come forward.
class Notifier : public QObject {
    Q_OBJECT

public:
    Notifier(const QString &appName, const QString &desktopId, const QString &iconPath, QObject *parent = nullptr);
    ~Notifier() override;

    // Takes ownership of the notification.
    void showWebNotification(QWebEngineNotification *notification);
    // A message from the app itself, such as a finished download. Clicking it opens `openPath`
    // when one is given.
    void showMessage(const QString &title, const QString &body, const QString &openPath = {});

signals:
    // A web notification was clicked. `origin` is the site that sent it.
    void webNotificationClicked(const QUrl &origin);

private slots:
    void onActionInvoked(uint id, const QString &actionKey);
    void onClosed(uint id, uint reason);
    void onActivationToken(uint id, const QString &token);

private:
    struct Shown {
        QPointer<QWebEngineNotification> web;
        QString key;      // origin and tag, so a page's newer notification replaces the older one
        QString openPath; // for the app's own messages
        QString imagePath;
    };

    void notify(uint replacesId, const QString &title, const QString &body, const QString &imagePath, Shown shown);
    void forget(uint id);

    QString m_appName;
    QString m_desktopId;
    QString m_iconPath;
    QHash<uint, Shown> m_shown;
    QHash<QString, uint> m_idByKey;
    int m_imageCounter = 0;
};
