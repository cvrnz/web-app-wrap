#include "browsercookies.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>

#include <openssl/evp.h>

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("BrowserCookies", text);
}

std::optional<QByteArray> aesDecrypt(const QByteArray &cipher, const QByteArray &key) {
    if (cipher.isEmpty() || cipher.size() % 16 != 0)
        return std::nullopt;
    static const QByteArray iv(16, ' ');
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    QByteArray plain(cipher.size() + 16, Qt::Uninitialized);
    int length = 0, finalLength = 0;
    const bool ok = EVP_DecryptInit_ex(context, EVP_aes_128_cbc(), nullptr,
                                       reinterpret_cast<const unsigned char *>(key.constData()),
                                       reinterpret_cast<const unsigned char *>(iv.constData()))
        && EVP_DecryptUpdate(context, reinterpret_cast<unsigned char *>(plain.data()), &length,
                             reinterpret_cast<const unsigned char *>(cipher.constData()), int(cipher.size()))
        && EVP_DecryptFinal_ex(context, reinterpret_cast<unsigned char *>(plain.data()) + length, &finalLength);
    EVP_CIPHER_CTX_free(context);
    if (!ok)
        return std::nullopt;
    plain.truncate(length + finalLength);
    return plain;
}

QNetworkCookie::SameSite sameSite(int value) {
    switch (value) {
    case 0: return QNetworkCookie::SameSite::None;
    case 1: return QNetworkCookie::SameSite::Lax;
    case 2: return QNetworkCookie::SameSite::Strict;
    default: return QNetworkCookie::SameSite::Default;
    }
}

}

namespace BrowserCookies {

Source helium() {
    const QString config = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    // Helium keeps its key under Chromium's name.
    return {QStringLiteral("Helium"), config + "/net.imput.helium/Default", QStringLiteral("chromium")};
}

QByteArray deriveKey(const QByteArray &password) {
    QByteArray key(16, Qt::Uninitialized);
    static const QByteArray salt("saltysalt");
    PKCS5_PBKDF2_HMAC_SHA1(password.constData(), int(password.size()),
                           reinterpret_cast<const unsigned char *>(salt.constData()), int(salt.size()), 1,
                           int(key.size()), reinterpret_cast<unsigned char *>(key.data()));
    return key;
}

std::optional<QByteArray> decrypt(const QByteArray &encrypted, const QByteArray &keyV11,
                                  const QString &hostKey, int databaseVersion) {
    static const QByteArray keyV10 = deriveKey("peanuts");
    std::optional<QByteArray> plain;
    if (encrypted.startsWith("v10"))
        plain = aesDecrypt(encrypted.mid(3), keyV10);
    else if (encrypted.startsWith("v11"))
        plain = aesDecrypt(encrypted.mid(3), keyV11);
    if (!plain)
        return std::nullopt;
    if (databaseVersion >= 24) {
        // A wrong key can still unpad cleanly by chance; the host's hash can't match by chance.
        if (plain->left(32) != QCryptographicHash::hash(hostKey.toUtf8(), QCryptographicHash::Sha256))
            return std::nullopt;
        plain->remove(0, 32);
    }
    return plain;
}

QDateTime fromChromeTime(qint64 microseconds) {
    // 11644473600 seconds separate 1601-01-01 from 1970-01-01.
    return QDateTime::fromMSecsSinceEpoch(microseconds / 1000 - 11644473600000LL, QTimeZone::UTC);
}

std::optional<QList<Cookie>> readCookiesFile(const QString &cookiesFile, const QByteArray &password, QString *error) {
    auto fail = [error](const QString &message) -> std::optional<QList<Cookie>> {
        if (error)
            *error = message;
        return std::nullopt;
    };
    if (!QFile::exists(cookiesFile))
        return fail(tr("There’s no cookies file at %1.").arg(cookiesFile));

    QTemporaryDir temporary;
    const QString copy = temporary.filePath("Cookies");
    if (!temporary.isValid() || !QFile::copy(cookiesFile, copy))
        return fail(tr("Couldn’t copy %1.").arg(cookiesFile));
    QFile::copy(cookiesFile + "-journal", copy + "-journal");

    const QString connection = QUuid::createUuid().toString();
    std::optional<QList<Cookie>> result;
    {
        QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", connection);
        database.setDatabaseName(copy);
        if (!database.open()) {
            fail(tr("Couldn’t open the cookies file: %1").arg(database.lastError().text()));
        } else {
            QSqlQuery version(database);
            const int databaseVersion = version.exec("SELECT value FROM meta WHERE key = 'version'") && version.next()
                ? version.value(0).toInt() : 0;

            QSqlQuery query(database);
            if (!query.exec("SELECT host_key, name, value, encrypted_value, path, expires_utc, is_secure, "
                            "is_httponly, has_expires, samesite, top_frame_site_key FROM cookies")) {
                fail(tr("Couldn’t read the cookies file: %1").arg(query.lastError().text()));
            } else {
                const QByteArray key = deriveKey(password);
                const QDateTime now = QDateTime::currentDateTimeUtc();
                QList<Cookie> cookies;
                int undecryptable = 0, total = 0;
                while (query.next()) {
                    // Partitioned cookies belong to one embedding site, which a QNetworkCookie
                    // can't express.
                    if (!query.value(10).toString().isEmpty())
                        continue;
                    ++total;
                    const QString host = query.value(0).toString();
                    QByteArray value = query.value(2).toByteArray();
                    const QByteArray encrypted = query.value(3).toByteArray();
                    if (!encrypted.isEmpty()) {
                        const auto plain = decrypt(encrypted, key, host, databaseVersion);
                        if (!plain) {
                            ++undecryptable;
                            continue;
                        }
                        value = *plain;
                    }
                    QNetworkCookie cookie(query.value(1).toByteArray(), value);
                    cookie.setPath(query.value(4).toString());
                    cookie.setSecure(query.value(6).toBool());
                    cookie.setHttpOnly(query.value(7).toBool());
                    cookie.setSameSitePolicy(sameSite(query.value(9).toInt()));
                    if (query.value(8).toBool()) {
                        const QDateTime expires = fromChromeTime(query.value(5).toLongLong());
                        if (expires <= now)
                            continue;
                        cookie.setExpirationDate(expires);
                    }
                    // A host_key with a leading dot is a domain cookie; without one, the cookie
                    // belongs to that host alone, which Qt expresses as no domain at all.
                    const bool domainCookie = host.startsWith('.');
                    if (domainCookie)
                        cookie.setDomain(host);
                    const QString originHost = domainCookie ? host.mid(1) : host;
                    cookies << Cookie{cookie, QUrl(QStringLiteral("https://") + originHost + "/")};
                }
                if (total > 0 && undecryptable == total)
                    fail(tr("Couldn’t decrypt any cookies; the keyring password doesn’t match."));
                else
                    result = cookies;
            }
            database.close();
        }
    }
    QSqlDatabase::removeDatabase(connection);
    return result;
}

std::optional<QList<Cookie>> read(const Source &source, QString *error) {
    const QString cookiesFile = source.profileDir + "/Cookies";
    if (!QFile::exists(cookiesFile)) {
        if (error)
            *error = tr("%1 isn’t set up on this computer (no %2).").arg(source.name, cookiesFile);
        return std::nullopt;
    }
    QProcess lookup;
    lookup.start("secret-tool", {"lookup", "application", source.keyringApplication});
    if (!lookup.waitForFinished(10000) || lookup.exitCode() != 0) {
        if (error)
            *error = lookup.error() == QProcess::FailedToStart
                ? tr("secret-tool isn’t installed (it’s in the libsecret package).")
                : tr("Couldn’t get %1’s password from the keyring. Is the keyring unlocked?").arg(source.name);
        return std::nullopt;
    }
    QByteArray password = lookup.readAllStandardOutput();
    if (password.endsWith('\n'))
        password.chop(1);
    return readCookiesFile(cookiesFile, password, error);
}

}
