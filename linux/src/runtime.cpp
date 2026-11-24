#include "runtime.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QWebEngineNotification>
#include <QWindow>

#include "notifier.h"
#include "version.h"

Runtime::Runtime(const Paths &paths, const WrapperConfig &config, QObject *parent)
    : QObject(parent), m_paths(paths), m_config(config),
      m_notifier(new Notifier(config.name, Paths::desktopId(config.slug), paths.appIcon(config.slug), this)) {
    loadSettings();
    connect(m_notifier, &Notifier::webNotificationClicked, this, &Runtime::notificationClicked);
}

bool Runtime::claimSingleInstance() {
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString socketPath = runtimeDir + "/" + Paths::desktopId(m_config.slug) + ".socket";

    QLocalSocket socket;
    socket.connectToServer(socketPath);
    if (socket.waitForConnected(500)) {
        // Hand over the launcher's activation token, so the running app may come forward.
        socket.write("open " + qgetenv("XDG_ACTIVATION_TOKEN") + "\n");
        socket.waitForBytesWritten(500);
        return false;
    }

    // Nothing answered, so any socket file left behind is from an app that crashed.
    QLocalServer::removeServer(socketPath);
    m_server.setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server.listen(socketPath))
        qWarning("Couldn't listen on %s: %s", qPrintable(socketPath), qPrintable(m_server.errorString()));
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *client = m_server.nextPendingConnection()) {
            connect(client, &QLocalSocket::readyRead, this, [this, client] {
                const QByteArray line = client->readAll().trimmed();
                if (!line.startsWith("open"))
                    return;
                const QByteArray token = line.mid(4).trimmed();
                if (!token.isEmpty())
                    qputenv("XDG_ACTIVATION_TOKEN", token);
                emit openRequested();
                client->disconnectFromServer();
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        }
    });
    return true;
}

QString Runtime::downloadsPath() const {
    const QString downloads = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    return downloads.isEmpty() ? QDir::homePath() + "/Downloads" : downloads;
}

QString Runtime::version() const {
    return Version::describe(m_config.version.isEmpty() ? std::nullopt : std::optional<QString>(m_config.version));
}

QString Runtime::destination(const QUrl &url) const {
    switch (m_policy.destination(url)) {
    case LinkPolicy::Destination::App: return QStringLiteral("app");
    case LinkPolicy::Destination::Browser: return QStringLiteral("browser");
    case LinkPolicy::Destination::System: return QStringLiteral("system");
    }
    return QStringLiteral("app");
}

void Runtime::openExternally(const QUrl &url) const {
    QDesktopServices::openUrl(url);
}

void Runtime::copyText(const QString &text) const {
    QGuiApplication::clipboard()->setText(text);
}

QString Runtime::addAllowedDomain(const QString &input) {
    const QString domain = LinkPolicy::normalizeDomain(input);
    if (domain.isEmpty())
        return tr("“%1” isn’t a web address.").arg(input.trimmed());
    if (m_policy.isListed(QUrl("https://" + domain)) && !m_allowedDomains.contains(domain))
        return tr("%1 is already part of the app.").arg(domain);
    if (!m_allowedDomains.contains(domain)) {
        m_allowedDomains << domain;
        m_allowedDomains.sort();
        saveSettings();
        loadSettings();
    }
    return {};
}

void Runtime::removeAllowedDomain(const QString &domain) {
    if (m_allowedDomains.removeAll(domain)) {
        saveSettings();
        loadSettings();
    }
}

void Runtime::setNotificationsEnabled(bool enabled) {
    if (enabled == m_notificationsEnabled)
        return;
    m_notificationsEnabled = enabled;
    saveSettings();
    emit settingsChanged();
}

void Runtime::presentNotification(QObject *object) {
    auto *notification = qobject_cast<QWebEngineNotification *>(object);
    if (!notification)
        return;
    if (!m_notificationsEnabled) {
        notification->close();
        return;
    }
    m_notifier->showWebNotification(notification);
}

void Runtime::notifyDownloadFinished(const QString &fileName, const QString &path) {
    m_notifier->showMessage(tr("Downloaded %1").arg(fileName), tr("Saved in %1").arg(QFileInfo(path).absolutePath()), path);
}

void Runtime::activate(QWindow *window) const {
    if (!window)
        return;
    if (window->visibility() == QWindow::Minimized)
        window->showNormal();
    window->show();
    window->raise();
    window->requestActivate();

    // Hyprland ignores activation requests unless misc:focus_on_activate is on, so ask it
    // directly. Its window list gives each window's process and title, which find this one.
    if (qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"))
        return;
    const QString hyprctl = QStandardPaths::findExecutable("hyprctl");
    if (hyprctl.isEmpty())
        return;
    auto *clients = new QProcess(const_cast<Runtime *>(this));
    const QString title = window->title();
    connect(clients, &QProcess::finished, clients, [clients, hyprctl, title] {
        clients->deleteLater();
        const qint64 pid = QCoreApplication::applicationPid();
        QString address, fallback;
        for (const QJsonValue &value : QJsonDocument::fromJson(clients->readAllStandardOutput()).array()) {
            const QJsonObject client = value.toObject();
            if (client.value("pid").toInteger() != pid)
                continue;
            if (fallback.isEmpty())
                fallback = client.value("address").toString();
            if (client.value("title").toString() == title)
                address = client.value("address").toString();
        }
        if (address.isEmpty())
            address = fallback;
        if (!address.isEmpty())
            QProcess::startDetached(hyprctl, {"dispatch", "focuswindow", "address:" + address});
    });
    clients->start(hyprctl, {"-j", "clients"});
}

void Runtime::loadSettings() {
    QFile file(m_paths.appSettings(m_config.slug));
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
        m_notificationsEnabled = object.value("notifications").toBool(true);
        m_allowedDomains.clear();
        for (const QJsonValue &value : object.value("allowedDomains").toArray())
            if (!value.toString().isEmpty())
                m_allowedDomains << value.toString();
    }
    m_policy = LinkPolicy(m_config.url, m_allowedDomains);
    emit settingsChanged();
}

void Runtime::saveSettings() const {
    QDir().mkpath(m_paths.appFolder(m_config.slug));
    QSaveFile file(m_paths.appSettings(m_config.slug));
    if (!file.open(QIODevice::WriteOnly))
        return;
    const QJsonObject object{{"notifications", m_notificationsEnabled},
                             {"allowedDomains", QJsonArray::fromStringList(m_allowedDomains)}};
    file.write(QJsonDocument(object).toJson(QJsonDocument::Indented));
    file.commit();
}
