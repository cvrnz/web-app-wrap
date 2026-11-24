#pragma once

#include <QString>

// Where WebAppWrap and the apps it makes keep their files. Tests pass temporary folders.
//
//     ~/.local/share/webappwrap/apps.json              the list of apps made in the window
//     ~/.local/share/webappwrap/icons/                 the image each listed app's icon is drawn from
//     ~/.local/share/webappwrap/previous-icons/        replaced and removed icons
//     ~/.local/share/webappwrap/apps/<slug>/           one built app: app.json, icon.png,
//                                                      settings.json and its sign-ins (profile/)
//     ~/.local/share/applications/webappwrap-<slug>.desktop   its launcher entry
//     ~/.cache/webappwrap/<slug>/                      its web cache
struct Paths {
    QString dataRoot;
    QString applications;
    QString cacheRoot;

    // The XDG locations ($XDG_DATA_HOME and $XDG_CACHE_HOME, or their defaults).
    static Paths standard();
    // Everything under one folder, for tests.
    static Paths under(const QString &root);

    QString appsRoot() const { return dataRoot + "/apps"; }
    QString appFolder(const QString &slug) const { return appsRoot() + "/" + slug; }
    QString appConfig(const QString &slug) const { return appFolder(slug) + "/app.json"; }
    QString appIcon(const QString &slug) const { return appFolder(slug) + "/icon.png"; }
    QString appSettings(const QString &slug) const { return appFolder(slug) + "/settings.json"; }
    QString profileFolder(const QString &slug) const { return appFolder(slug) + "/profile"; }
    QString cacheFolder(const QString &slug) const { return cacheRoot + "/" + slug; }
    QString desktopFile(const QString &slug) const { return applications + "/" + desktopId(slug) + ".desktop"; }

    // The desktop entry's name without ".desktop", which is also the app's Wayland app ID, so
    // Hyprland and the launcher match its windows to its entry and icon.
    static QString desktopId(const QString &slug) { return "webappwrap-" + slug; }
};
