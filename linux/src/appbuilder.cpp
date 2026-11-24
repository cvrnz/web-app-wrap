#include "appbuilder.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <algorithm>

#include "iconrenderer.h"

namespace {

QString tr(const char *text) { return QCoreApplication::translate("AppBuilder", text); }

bool writeAtomically(const QString &path, const QByteArray &data, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        if (error)
            *error = tr("Couldn’t write %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

// Desktop entry values can't hold line breaks, and backslashes are escapes.
QString desktopValue(QString value) {
    value.replace('\\', "\\\\").replace('\n', ' ').replace('\r', ' ');
    return value;
}

}

std::optional<WrapperConfig> WrapperConfig::load(const Paths &paths, const QString &slug, QString *error) {
    QFile file(paths.appConfig(slug));
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = tr("There’s no app called “%1”. Make it in WebAppWrap first.").arg(slug);
        return std::nullopt;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    WrapperConfig config;
    config.slug = slug;
    config.name = object.value("name").toString(slug);
    config.url = QUrl(object.value("url").toString());
    config.titleBarColor = HexColor::fromHex(object.value("titleBarColor").toString()).value_or(HexColor::defaultTitleBar());
    config.listId = object.value("id").toString();
    config.version = object.value("version").toString();
    if (!config.url.isValid()) {
        if (error)
            *error = tr("%1 has no web address.").arg(paths.appConfig(slug));
        return std::nullopt;
    }
    return config;
}

AppBuilder::AppBuilder(const Paths &paths, const QString &executable, const std::optional<QString> &version)
    : m_paths(paths), m_executable(executable), m_version(version) {}

QString AppBuilder::defaultExecutable() {
    const QString path = QCoreApplication::applicationFilePath();
    return path.startsWith("/usr/") ? QStringLiteral("webappwrap") : path;
}

QString AppBuilder::quoteExecArgument(const QString &argument) {
    static const QRegularExpression plain("^[A-Za-z0-9_./+-]+$");
    if (plain.match(argument).hasMatch())
        return argument;
    QString escaped = argument;
    escaped.replace('\\', "\\\\\\\\").replace('"', "\\\\\"").replace('`', "\\\\`").replace('$', "\\\\$");
    return "\"" + escaped + "\"";
}

std::optional<IconSource> AppBuilder::iconSource(const BatchEntry &entry, QString *error) {
    if (entry.image.isEmpty())
        return IconSource::generated(entry.location.name);
    return IconSource::load(entry.image, entry.zoom, entry.iconBackground, entry.iconColor, error);
}

BuildOutcome AppBuilder::build(const BatchEntry &entry, const HexColor &titleBarColor, const QString &listId) const {
    BuildOutcome outcome;
    outcome.slug = entry.location.slug();
    outcome.desktopFile = m_paths.desktopFile(outcome.slug);
    outcome.replaced = isInstalled(outcome.slug);

    const auto source = iconSource(entry, &outcome.error);
    if (!source)
        return outcome;

    const QString folder = m_paths.appFolder(outcome.slug);
    if (!QDir().mkpath(folder) || !QDir().mkpath(m_paths.applications)) {
        outcome.error = tr("Couldn’t create %1.").arg(folder);
        return outcome;
    }

    // 512 px is the largest size launchers and notifications ask for.
    const QImage icon = IconRenderer::render(*source, 512);
    QByteArray png;
    {
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        icon.save(&buffer, "PNG");
    }
    if (!writeAtomically(m_paths.appIcon(outcome.slug), png, &outcome.error))
        return outcome;

    QJsonObject config{{"name", entry.location.name},
                       {"url", entry.url.toString()},
                       {"titleBarColor", titleBarColor.hexString()}};
    if (!listId.isEmpty())
        config.insert("id", listId);
    if (m_version)
        config.insert("version", *m_version);
    if (!writeAtomically(m_paths.appConfig(outcome.slug), QJsonDocument(config).toJson(QJsonDocument::Indented), &outcome.error))
        return outcome;

    const QString id = Paths::desktopId(outcome.slug);
    const QString desktop = QStringList{
        "[Desktop Entry]",
        "Type=Application",
        "Version=1.0",
        "Name=" + desktopValue(entry.location.name),
        "Comment=" + desktopValue(entry.url.host()),
        "Exec=" + quoteExecArgument(m_executable) + " run " + outcome.slug,
        "Icon=" + desktopValue(m_paths.appIcon(outcome.slug)),
        "Terminal=false",
        "StartupNotify=true",
        "StartupWMClass=" + id,
        "Categories=Network;",
        "X-WebAppWrap-App=" + outcome.slug,
        "",
    }.join('\n');
    if (!writeAtomically(outcome.desktopFile, desktop.toUtf8(), &outcome.error))
        return outcome;

    outcome.ok = true;
    return outcome;
}

bool AppBuilder::isInstalled(const QString &slug) const {
    return QFile::exists(m_paths.desktopFile(slug)) && QFile::exists(m_paths.appConfig(slug));
}

QList<WrapperConfig> AppBuilder::installed() const {
    QList<WrapperConfig> apps;
    for (const QString &slug : QDir(m_paths.appsRoot()).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
        if (isInstalled(slug))
            if (const auto config = WrapperConfig::load(m_paths, slug))
                apps << *config;
    std::sort(apps.begin(), apps.end(), [](const WrapperConfig &a, const WrapperConfig &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return apps;
}

bool AppBuilder::remove(const QString &slug, bool deleteData, QString *error) const {
    if (!QFile::exists(m_paths.appFolder(slug)) && !QFile::exists(m_paths.desktopFile(slug))) {
        if (error)
            *error = tr("There’s no app called “%1”.").arg(slug);
        return false;
    }
    QFile::remove(m_paths.desktopFile(slug));
    QFile::remove(m_paths.appIcon(slug));
    QFile::remove(m_paths.appConfig(slug));
    if (deleteData) {
        QDir(m_paths.appFolder(slug)).removeRecursively();
        QDir(m_paths.cacheFolder(slug)).removeRecursively();
    }
    return true;
}
