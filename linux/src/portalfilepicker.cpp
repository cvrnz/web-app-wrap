#include "portalfilepicker.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QRandomGenerator>
#include <QUrl>

// The portal's filter list: a(sa(us)), each filter a name and rules of (0 = glob, pattern).
struct PortalFilterRule {
    uint type = 0;
    QString pattern;
};
struct PortalFilter {
    QString name;
    QList<PortalFilterRule> rules;
};
Q_DECLARE_METATYPE(PortalFilterRule)
Q_DECLARE_METATYPE(PortalFilter)

QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilterRule &rule) {
    argument.beginStructure();
    argument << rule.type << rule.pattern;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilterRule &rule) {
    argument.beginStructure();
    argument >> rule.type >> rule.pattern;
    argument.endStructure();
    return argument;
}
QDBusArgument &operator<<(QDBusArgument &argument, const PortalFilter &filter) {
    argument.beginStructure();
    argument << filter.name << filter.rules;
    argument.endStructure();
    return argument;
}
const QDBusArgument &operator>>(const QDBusArgument &argument, PortalFilter &filter) {
    argument.beginStructure();
    argument >> filter.name >> filter.rules;
    argument.endStructure();
    return argument;
}

PortalFilePicker::PortalFilePicker(QObject *parent) : QObject(parent) {
    qDBusRegisterMetaType<PortalFilterRule>();
    qDBusRegisterMetaType<QList<PortalFilterRule>>();
    qDBusRegisterMetaType<PortalFilter>();
    qDBusRegisterMetaType<QList<PortalFilter>>();
}

void PortalFilePicker::openFile(const QString &title, const QString &filterName, const QStringList &patterns, Callback done) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()->isServiceRegistered("org.freedesktop.portal.Desktop")) {
        done({}, tr("There’s no file chooser: xdg-desktop-portal isn’t running."));
        return;
    }
    if (!m_requestPath.isEmpty())
        bus.disconnect({}, m_requestPath, "org.freedesktop.portal.Request", "Response", this, SLOT(onResponse(uint, QVariantMap)));
    m_done = std::move(done);

    // Listen for the answer before asking, at the path the portal will use, so a fast answer
    // isn't missed.
    const QString token = QString("waw%1").arg(QRandomGenerator::global()->generate());
    QString sender = bus.baseService().mid(1);
    sender.replace('.', '_');
    m_requestPath = QString("/org/freedesktop/portal/desktop/request/%1/%2").arg(sender, token);
    bus.connect({}, m_requestPath, "org.freedesktop.portal.Request", "Response", this, SLOT(onResponse(uint, QVariantMap)));

    PortalFilter filter{filterName, {}};
    for (const QString &pattern : patterns)
        filter.rules << PortalFilterRule{0, pattern};
    QVariantMap options{{"handle_token", token}, {"modal", true}};
    if (!patterns.isEmpty()) {
        options.insert("filters", QVariant::fromValue(QList<PortalFilter>{filter}));
        options.insert("current_filter", QVariant::fromValue(filter));
    }

    QDBusMessage message = QDBusMessage::createMethodCall("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop",
                                                          "org.freedesktop.portal.FileChooser", "OpenFile");
    message << QString() << title << options;
    auto *watcher = new QDBusPendingCallWatcher(bus.asyncCall(message), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
        watcher->deleteLater();
        QDBusPendingReply<QDBusObjectPath> reply = *watcher;
        if (reply.isError() && m_done) {
            Callback done = std::move(m_done);
            m_done = nullptr;
            done({}, tr("The file chooser didn’t open: %1").arg(reply.error().message()));
        }
    });
}

void PortalFilePicker::onResponse(uint response, const QVariantMap &results) {
    if (!m_done)
        return;
    Callback done = std::move(m_done);
    m_done = nullptr;
    // 0 is a choice, 1 a cancel and 2 anything else.
    const QStringList uris = results.value("uris").toStringList();
    if (response != 0 || uris.isEmpty()) {
        done({}, {});
        return;
    }
    done(QUrl(uris.first()).toLocalFile(), {});
}
