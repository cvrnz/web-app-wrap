#include "notifier.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QWebEngineNotification>

namespace {
const QString service = QStringLiteral("org.freedesktop.Notifications");
const QString path = QStringLiteral("/org/freedesktop/Notifications");
const QString interface = QStringLiteral("org.freedesktop.Notifications");

// Notification bodies may be read as markup, so page text is escaped.
QString escaped(const QString &text) { return text.toHtmlEscaped(); }
}

Notifier::Notifier(const QString &appName, const QString &desktopId, const QString &iconPath, QObject *parent)
    : QObject(parent), m_appName(appName), m_desktopId(desktopId), m_iconPath(iconPath) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.connect(service, path, interface, "ActionInvoked", this, SLOT(onActionInvoked(uint, QString)));
    bus.connect(service, path, interface, "NotificationClosed", this, SLOT(onClosed(uint, uint)));
    bus.connect(service, path, interface, "ActivationToken", this, SLOT(onActivationToken(uint, QString)));
}

Notifier::~Notifier() {
    for (const Shown &shown : std::as_const(m_shown))
        if (!shown.imagePath.isEmpty())
            QFile::remove(shown.imagePath);
}

void Notifier::showWebNotification(QWebEngineNotification *notification) {
    notification->setParent(this);
    const QString key = notification->origin().toString() + "\n" + notification->tag();
    // A notification without a tag never replaces another one.
    const uint replacesId = notification->tag().isEmpty() ? 0 : m_idByKey.value(key);

    // The page's own image, saved where the notification service can read it.
    QString imagePath;
    if (!notification->icon().isNull()) {
        const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
        imagePath = QString("%1/%2-notification-%3.png").arg(runtime, m_desktopId).arg(++m_imageCounter);
        if (!notification->icon().save(imagePath, "PNG"))
            imagePath.clear();
    }

    connect(notification, &QWebEngineNotification::closed, this, [this, notification] {
        // The page closed it, so take it off the screen too.
        for (auto it = m_shown.begin(); it != m_shown.end(); ++it)
            if (it->web == notification) {
                QDBusConnection::sessionBus().asyncCall(
                    QDBusMessage::createMethodCall(service, path, interface, "CloseNotification") << it.key());
                break;
            }
    });

    notify(replacesId, notification->title(), escaped(notification->message()), imagePath,
           Shown{notification, key, {}, imagePath});
    notification->show();
}

void Notifier::showMessage(const QString &title, const QString &body, const QString &openPath) {
    notify(0, title, escaped(body), {}, Shown{nullptr, {}, openPath, {}});
}

void Notifier::notify(uint replacesId, const QString &title, const QString &body, const QString &imagePath, Shown shown) {
    QVariantMap hints{{"desktop-entry", m_desktopId}};
    if (!imagePath.isEmpty())
        hints.insert("image-path", QUrl::fromLocalFile(imagePath).toString());
    QDBusMessage message = QDBusMessage::createMethodCall(service, path, interface, "Notify");
    message << m_appName << replacesId << m_iconPath << title << body
            << QStringList{"default", tr("Open")} << hints << -1;

    auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher, replacesId, shown] {
        watcher->deleteLater();
        QDBusPendingReply<uint> reply = *watcher;
        if (reply.isError()) {
            // No notification service is running; there's nowhere to show it.
            qWarning("Couldn't show a notification: %s", qPrintable(reply.error().message()));
            if (shown.web)
                shown.web->deleteLater();
            if (!shown.imagePath.isEmpty())
                QFile::remove(shown.imagePath);
            return;
        }
        const uint id = reply.value();
        if (replacesId && replacesId != id)
            forget(replacesId);
        else if (replacesId == id && m_shown.contains(id)) {
            // The same popup now shows the newer notification; let go of the older one.
            Shown old = m_shown.take(id);
            if (old.web && old.web != shown.web)
                old.web->deleteLater();
            if (!old.imagePath.isEmpty() && old.imagePath != shown.imagePath)
                QFile::remove(old.imagePath);
        }
        m_shown.insert(id, shown);
        if (!shown.key.isEmpty())
            m_idByKey.insert(shown.key, id);
    });
}

void Notifier::onActivationToken(uint id, const QString &token) {
    // Sent just before ActionInvoked. Qt's Wayland support uses this variable the next time a
    // window asks to be activated, which lets the compositor bring it forward.
    if (m_shown.contains(id))
        qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
}

void Notifier::onActionInvoked(uint id, const QString &actionKey) {
    if (!m_shown.contains(id) || actionKey != "default")
        return;
    const Shown shown = m_shown.value(id);
    if (shown.web) {
        // Lets the page run its click handler, which often opens what the notification was about.
        shown.web->click();
        emit webNotificationClicked(shown.web->origin());
    } else if (!shown.openPath.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(shown.openPath));
    }
}

void Notifier::onClosed(uint id, uint) {
    if (!m_shown.contains(id))
        return;
    const Shown shown = m_shown.value(id);
    if (shown.web)
        shown.web->close();
    forget(id);
}

void Notifier::forget(uint id) {
    const Shown shown = m_shown.take(id);
    if (!shown.key.isEmpty() && m_idByKey.value(shown.key) == id)
        m_idByKey.remove(shown.key);
    if (shown.web)
        shown.web->deleteLater();
    if (!shown.imagePath.isEmpty())
        QFile::remove(shown.imagePath);
}
