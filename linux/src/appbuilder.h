#pragma once

#include <QList>
#include <QString>
#include <optional>

#include "batchfile.h"
#include "hexcolor.h"
#include "iconrenderer.h"
#include "paths.h"

// What a built app reads at launch, from <data>/apps/<slug>/app.json.
struct WrapperConfig {
    QString slug;
    QString name;
    QUrl url;
    HexColor titleBarColor = HexColor::defaultTitleBar();
    QString listId;  // the app's ID in WebAppWrap's list; empty for apps made from the command line
    QString version; // the WebAppWrap that built it

    static std::optional<WrapperConfig> load(const Paths &paths, const QString &slug, QString *error = nullptr);
};

struct BuildOutcome {
    bool ok = false;
    QString error;
    QString slug;
    QString desktopFile;
    bool replaced = false; // an app with this name was already installed
};

// Makes an app: an icon, the app.json the app reads at launch, and a desktop entry that runs
// `webappwrap run <slug>` under its own Wayland app ID. Nothing is copied per app, so updating
// WebAppWrap updates every app.
class AppBuilder {
public:
    // `executable` is what the desktop entry runs.
    AppBuilder(const Paths &paths, const QString &executable, const std::optional<QString> &version);

    // `titleBarColor` is the entry's color, or the site's theme color the caller looked up.
    BuildOutcome build(const BatchEntry &entry, const HexColor &titleBarColor, const QString &listId = {}) const;

    bool isInstalled(const QString &slug) const;
    // Every built app, by name.
    QList<WrapperConfig> installed() const;
    // Removes the desktop entry and icon. With `deleteData`, also its sign-ins, settings and cache.
    bool remove(const QString &slug, bool deleteData, QString *error = nullptr) const;

    // The icon an entry gets, as the builder draws it.
    static std::optional<IconSource> iconSource(const BatchEntry &entry, QString *error = nullptr);

    // What desktop entries run: plain "webappwrap" for the installed package, which keeps
    // entries working when it moves, and the full path for a build run from a checkout.
    static QString defaultExecutable();
    // Quotes an argument for a desktop entry's Exec key.
    static QString quoteExecArgument(const QString &argument);

private:
    Paths m_paths;
    QString m_executable;
    std::optional<QString> m_version;
};
