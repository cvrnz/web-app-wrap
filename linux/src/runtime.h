#pragma once

#include <QColor>
#include <QLocalServer>
#include <QObject>
#include <QStringList>
#include <QUrl>

#include "appbuilder.h"
#include "linkpolicy.h"
#include "paths.h"

class Notifier;
class QWindow;

// One running app, handed to its QML as `app`: its settings, where its links go, and the
// desktop services it needs (notifications, the browser, bringing a window forward).
class Runtime : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QUrl url READ url CONSTANT)
    Q_PROPERTY(QString slug READ slug CONSTANT)
    Q_PROPERTY(QColor titleBarColor READ titleBarColor CONSTANT)
    Q_PROPERTY(bool titleBarDark READ titleBarDark CONSTANT)
    Q_PROPERTY(QString storagePath READ storagePath CONSTANT)
    Q_PROPERTY(QString cachePath READ cachePath CONSTANT)
    Q_PROPERTY(QString downloadsPath READ downloadsPath CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY settingsChanged)
    Q_PROPERTY(QStringList allowedDomains READ allowedDomains NOTIFY settingsChanged)

public:
    Runtime(const Paths &paths, const WrapperConfig &config, QObject *parent = nullptr);

    // Makes this the app's only process. False if the app is already running, in which case
    // that process is asked to bring a window forward and this one should exit.
    bool claimSingleInstance();

    QString name() const { return m_config.name; }
    QUrl url() const { return m_config.url; }
    QString slug() const { return m_config.slug; }
    QColor titleBarColor() const { return m_config.titleBarColor.toQColor(); }
    bool titleBarDark() const { return m_config.titleBarColor.isDark(); }
    QString storagePath() const { return m_paths.profileFolder(m_config.slug); }
    QString cachePath() const { return m_paths.cacheFolder(m_config.slug); }
    QString downloadsPath() const;
    QString version() const;
    bool notificationsEnabled() const { return m_notificationsEnabled; }
    void setNotificationsEnabled(bool enabled);
    QStringList allowedDomains() const { return m_allowedDomains; }

    // "app", "browser" or "system" (see LinkPolicy).
    Q_INVOKABLE QString destination(const QUrl &url) const;
    Q_INVOKABLE bool isListed(const QUrl &url) const { return m_policy.isListed(url); }
    Q_INVOKABLE void openExternally(const QUrl &url) const;
    Q_INVOKABLE void copyText(const QString &text) const;

    // Adds a typed or pasted site. Returns an error to show, or an empty string.
    Q_INVOKABLE QString addAllowedDomain(const QString &input);
    Q_INVOKABLE void removeAllowedDomain(const QString &domain);

    // Copies Helium's cookies into `profile` (a WebEngineProfile), so the app is signed in
    // wherever Helium is. Done at every launch and from the ⋮ menu. A one-way copy: signing in
    // or out here doesn't reach Helium. Returns what happened, to show.
    Q_INVOKABLE QString importHeliumCookies(QObject *profile) const;

    // A web notification from the profile's presentNotification signal.
    Q_INVOKABLE void presentNotification(QObject *notification);
    Q_INVOKABLE void notifyDownloadFinished(const QString &fileName, const QString &path);
    // Brings a window forward, even when the compositor wouldn't on its own.
    Q_INVOKABLE void activate(QWindow *window) const;

signals:
    void settingsChanged();
    // The app was launched again while running.
    void openRequested();
    // A web notification from `origin` was clicked.
    void notificationClicked(const QUrl &origin);

private:
    void loadSettings();
    void saveSettings() const;

    Paths m_paths;
    WrapperConfig m_config;
    LinkPolicy m_policy;
    bool m_notificationsEnabled = true;
    QStringList m_allowedDomains;
    Notifier *m_notifier;
    QLocalServer m_server;
};
