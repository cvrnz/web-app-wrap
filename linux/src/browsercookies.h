#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QNetworkCookie>
#include <QString>
#include <QUrl>
#include <optional>

// Reads a Chromium-based browser's cookies on Linux, so an app can start out signed in where
// the browser is. Some sites, Google's sign-in among them, turn away embedded browsers like
// Qt WebEngine, but accept a session made in a real browser.
//
// Chromium keeps cookies in an SQLite file, `<profile>/Cookies`, with each value encrypted
// with AES-128-CBC. "v10" values use a fixed key; "v11" values use a key derived from the
// browser's "Safe Storage" password in the desktop keyring. From database version 24 on, the
// decrypted value starts with the SHA-256 of the cookie's host, which also shows whether the
// key was right.
namespace BrowserCookies {

struct Cookie {
    QNetworkCookie cookie;
    QUrl origin; // where the cookie is set from, which Qt WebEngine's cookie store needs
};

struct Source {
    QString name;         // shown to the user, like "Helium"
    QString profileDir;   // the folder holding the Cookies file
    QString keyringApplication; // the "application" attribute of its Safe Storage password
};

// Helium's default profile.
Source helium();

// The 16-byte AES key Chromium derives from a Safe Storage password.
QByteArray deriveKey(const QByteArray &password);

// One stored value, or nothing when it can't be decrypted with `keyV11` (or the fixed v10 key).
// `hostKey` is checked against the value's hash prefix when `databaseVersion` is 24 or newer.
std::optional<QByteArray> decrypt(const QByteArray &encrypted, const QByteArray &keyV11,
                                  const QString &hostKey, int databaseVersion);

// Chromium's timestamps count microseconds since 1601-01-01 UTC.
QDateTime fromChromeTime(qint64 microseconds);

// Every unexpired, unpartitioned cookie in a Cookies file. The file is copied first, since
// the browser may have it open.
std::optional<QList<Cookie>> readCookiesFile(const QString &cookiesFile, const QByteArray &password, QString *error);

// The browser's cookies, with its password looked up in the keyring.
std::optional<QList<Cookie>> read(const Source &source, QString *error);

}
