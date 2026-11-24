#pragma once

#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUuid>
#include <optional>

#include "batchfile.h"

// What an app in the list is made from: the same inputs as the window and a batch entry,
// except that the icon is a file WebAppWrap keeps in its own icons folder. Stored in the same
// apps.json format as on macOS.
struct AppSettings {
    QString name; // may include folders, as in the window
    QString url;
    std::optional<QString> icon; // file name in the icons folder; none draws the default icon
    double zoom = 1;
    std::optional<QString> iconBackground; // "#RRGGBB"
    std::optional<QString> iconColor;      // "#RRGGBB"; none keeps the SVG's own colors
    std::optional<QString> titleBarColor;  // "#RRGGBB"; none uses the site's theme color

    AppLocation location() const { return AppLocation(name); }

    QJsonObject toJson() const;
    static AppSettings fromJson(const QJsonObject &object);
    bool operator==(const AppSettings &other) const = default;
};

// When an app was last built, by which WebAppWrap, and from which settings.
struct BuildStamp {
    std::optional<QString> version; // the commit WebAppWrap was built from; none for development builds
    QDateTime date;
    AppSettings settings;

    bool operator==(const BuildStamp &other) const = default;
};

// One app in the list WebAppWrap keeps of the apps made in its window.
struct AppRecord {
    QUuid id;
    AppSettings settings;
    std::optional<BuildStamp> lastBuild;

    // Why the app needs building, or none if it's up to date and can be skipped.
    enum class BuildReason { NeverBuilt, Changed, NewVersion, UnknownVersion, Missing };
    static QString describe(BuildReason reason);

    // An app is skipped only when its settings and the WebAppWrap version match its last
    // build, and the app is still installed.
    std::optional<BuildReason> buildReason(const std::optional<QString> &currentVersion, bool appExists) const;

    // The same inputs as a batch-file entry, so the list builds exactly like a batch.
    std::optional<BatchEntry> batchEntry(const QString &iconsFolder) const;

    // Swift writes UUIDs in uppercase; so does this, so the file reads the same everywhere.
    QString idString() const { return id.toString(QUuid::WithoutBraces).toUpper(); }

    bool operator==(const AppRecord &other) const = default;
};

// The list and its icons, kept in one folder (normally ~/.local/share/webappwrap):
//
//     apps.json         every app's settings and when it was last built
//     icons/            the image each app's icon is drawn from
//     previous-icons/   icons that were replaced or removed, kept in case they're wanted again
class AppRegistry {
public:
    explicit AppRegistry(const QString &folder) : m_folder(folder) {}

    QString folder() const { return m_folder; }
    QString listFile() const { return m_folder + "/apps.json"; }
    QString iconsFolder() const { return m_folder + "/icons"; }
    QString previousIconsFolder() const { return m_folder + "/previous-icons"; }
    QString iconPath(const QString &fileName) const { return iconsFolder() + "/" + fileName; }

    // An empty list if nothing has been saved yet; none with `error` set if the file can't be read.
    std::optional<QList<AppRecord>> load(QString *error = nullptr) const;
    bool save(const QList<AppRecord> &records, QString *error = nullptr) const;

    // Copies `source` into the icons folder and returns its new file name. If it's the same
    // image as `current`, returns `current` instead of making a copy. Empty on failure.
    QString importIcon(const QString &source, const QUuid &id, const std::optional<QString> &current,
                       QString *error = nullptr) const;
    // Moves an icon that's no longer used into previous-icons, named after the app and the time.
    QString retireIcon(const QString &fileName, const QString &appName,
                       const QDateTime &date = QDateTime::currentDateTime()) const;

private:
    QString m_folder;
};
