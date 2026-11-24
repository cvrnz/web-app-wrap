#include "appregistry.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

void putOptional(QJsonObject &object, const char *key, const std::optional<QString> &value) {
    if (value)
        object.insert(key, *value);
}

std::optional<QString> optionalString(const QJsonObject &object, const char *key) {
    const QJsonValue value = object.value(key);
    return value.isString() ? std::optional<QString>(value.toString()) : std::nullopt;
}

QJsonObject recordToJson(const AppRecord &record) {
    QJsonObject object{{"id", record.idString()}, {"settings", record.settings.toJson()}};
    if (record.lastBuild) {
        QJsonObject build{{"date", record.lastBuild->date.toUTC().toString(Qt::ISODate)},
                          {"settings", record.lastBuild->settings.toJson()}};
        putOptional(build, "version", record.lastBuild->version);
        object.insert("lastBuild", build);
    }
    return object;
}

AppRecord recordFromJson(const QJsonObject &object) {
    AppRecord record;
    record.id = QUuid::fromString(object.value("id").toString());
    record.settings = AppSettings::fromJson(object.value("settings").toObject());
    if (object.value("lastBuild").isObject()) {
        const QJsonObject build = object.value("lastBuild").toObject();
        record.lastBuild = BuildStamp{optionalString(build, "version"),
                                      QDateTime::fromString(build.value("date").toString(), Qt::ISODate),
                                      AppSettings::fromJson(build.value("settings").toObject())};
    }
    return record;
}

}

QJsonObject AppSettings::toJson() const {
    QJsonObject object{{"name", name}, {"url", url}, {"zoom", zoom}};
    putOptional(object, "icon", icon);
    putOptional(object, "iconBackground", iconBackground);
    putOptional(object, "iconColor", iconColor);
    putOptional(object, "titleBarColor", titleBarColor);
    return object;
}

AppSettings AppSettings::fromJson(const QJsonObject &object) {
    AppSettings settings;
    settings.name = object.value("name").toString();
    settings.url = object.value("url").toString();
    settings.icon = optionalString(object, "icon");
    settings.zoom = object.value("zoom").toDouble(1);
    settings.iconBackground = optionalString(object, "iconBackground");
    settings.iconColor = optionalString(object, "iconColor");
    settings.titleBarColor = optionalString(object, "titleBarColor");
    return settings;
}

QString AppRecord::describe(BuildReason reason) {
    switch (reason) {
    case BuildReason::NeverBuilt: return QCoreApplication::translate("AppRecord", "Not built yet");
    case BuildReason::Changed: return QCoreApplication::translate("AppRecord", "Changed since the last build");
    case BuildReason::NewVersion: return QCoreApplication::translate("AppRecord", "Built with another version of WebAppWrap");
    case BuildReason::UnknownVersion: return QCoreApplication::translate("AppRecord", "This WebAppWrap has no version, so it always rebuilds");
    case BuildReason::Missing: return QCoreApplication::translate("AppRecord", "The app isn’t installed");
    }
    return {};
}

std::optional<AppRecord::BuildReason> AppRecord::buildReason(const std::optional<QString> &currentVersion, bool appExists) const {
    if (!lastBuild)
        return BuildReason::NeverBuilt;
    if (lastBuild->settings != settings)
        return BuildReason::Changed;
    if (!currentVersion)
        return BuildReason::UnknownVersion;
    if (lastBuild->version != currentVersion)
        return BuildReason::NewVersion;
    if (!appExists)
        return BuildReason::Missing;
    return std::nullopt;
}

std::optional<BatchEntry> AppRecord::batchEntry(const QString &iconsFolder) const {
    BatchEntry entry;
    entry.location = settings.location();
    entry.url = InputNormalizer::normalizeUrl(settings.url);
    if (!entry.url.isValid() || entry.location.name.isEmpty())
        return std::nullopt;
    if (settings.icon)
        entry.image = iconsFolder + "/" + *settings.icon;
    entry.zoom = settings.zoom;
    auto color = [](const std::optional<QString> &hex) {
        return hex ? HexColor::fromHex(*hex) : std::nullopt;
    };
    entry.iconBackground = color(settings.iconBackground);
    entry.iconColor = color(settings.iconColor);
    entry.titleBarColor = color(settings.titleBarColor);
    return entry;
}

std::optional<QList<AppRecord>> AppRegistry::load(QString *error) const {
    QFile file(listFile());
    if (!file.exists())
        return QList<AppRecord>();
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (!document.isObject()) {
        if (error)
            *error = QCoreApplication::translate("AppRegistry", "%1 isn’t valid: %2").arg(listFile(), parseError.errorString());
        return std::nullopt;
    }
    QList<AppRecord> records;
    for (const QJsonValue &value : document.object().value("apps").toArray())
        records << recordFromJson(value.toObject());
    return records;
}

bool AppRegistry::save(const QList<AppRecord> &records, QString *error) const {
    QDir().mkpath(m_folder);
    QJsonArray apps;
    for (const AppRecord &record : records)
        apps.append(recordToJson(record));
    QSaveFile file(listFile());
    if (!file.open(QIODevice::WriteOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.write(QJsonDocument(QJsonObject{{"apps", apps}}).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    return true;
}

QString AppRegistry::importIcon(const QString &source, const QUuid &id, const std::optional<QString> &current,
                                QString *error) const {
    if (current) {
        const QString currentPath = iconPath(*current);
        if (QFileInfo(source).canonicalFilePath() == QFileInfo(currentPath).canonicalFilePath())
            return *current;
        QFile a(source), b(currentPath);
        if (a.open(QIODevice::ReadOnly) && b.open(QIODevice::ReadOnly) && a.readAll() == b.readAll())
            return *current;
    }
    QDir().mkpath(iconsFolder());
    // A new name for every new image, so replacing an icon always changes the app's settings.
    const QString token = QUuid::createUuid().toString(QUuid::Id128).left(8).toLower();
    const QString suffix = QFileInfo(source).suffix().toLower();
    const QString fileName = id.toString(QUuid::WithoutBraces).toLower() + "-" + token + (suffix.isEmpty() ? "" : "." + suffix);
    if (!QFile::copy(source, iconPath(fileName))) {
        if (error)
            *error = QCoreApplication::translate("AppRegistry", "Couldn’t copy %1.").arg(source);
        return {};
    }
    return fileName;
}

QString AppRegistry::retireIcon(const QString &fileName, const QString &appName, const QDateTime &date) const {
    const QString source = iconPath(fileName);
    if (!QFile::exists(source))
        return {};
    QDir().mkpath(previousIconsFolder());
    const QString base = InputNormalizer::fileSafeName(AppLocation(appName).name) + " " + date.toString("yyyy-MM-dd HH.mm.ss");
    const QString suffix = QFileInfo(source).suffix();
    const QString ext = suffix.isEmpty() ? "" : "." + suffix;
    QString destination = previousIconsFolder() + "/" + base + ext;
    for (int counter = 2; QFile::exists(destination); ++counter)
        destination = previousIconsFolder() + "/" + base + " " + QString::number(counter) + ext;
    return QFile::rename(source, destination) ? destination : QString();
}
