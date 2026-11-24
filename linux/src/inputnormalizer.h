#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

// Turns loosely typed input into the values an app needs.
namespace InputNormalizer {
// Accepts "example.com", "https://example.com/app", "localhost:3000" and so on. Returns an
// invalid QUrl unless the result is an http(s) address with a host. Addresses without a scheme
// get https://, except localhost, 127.0.0.1 and 0.0.0.0, which get http://.
QUrl normalizeUrl(const QString &raw);

// Lowercase ASCII slug: "My Cool App!" gives "my-cool-app". Never empty.
QString slug(const QString &name);

// A display name that's safe as a file name (no "/" or ":", no leading dot).
QString fileSafeName(const QString &name);
}

// The name field accepts folders, as on macOS and Windows: "WebAppWrap/Photos" names the app
// "Photos". Linux launchers have no folders, so on Linux the folders are kept in the list but
// don't change where the app goes.
struct AppLocation {
    QStringList folders;
    QString name;

    explicit AppLocation(const QString &input = {});

    // What the app's files, desktop entry, Wayland app ID and sign-ins are keyed by. Building
    // an app again under the same name keeps its sign-ins.
    QString slug() const { return InputNormalizer::slug(name); }
};
