#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

#include "appbuilder.h"
#include "appregistry.h"
#include "batchfile.h"
#include "browsercookies.h"
#include "hexcolor.h"
#include "iconrenderer.h"
#include "inputnormalizer.h"
#include "json5.h"
#include "linkpolicy.h"
#include "theme.h"
#include "themecolor.h"
#include "translator.h"

// JSON, HTML and SVG fixtures live in their own header, because moc misreads raw strings.
#include "fixtures.h"

#include <openssl/evp.h>

namespace {

// Encrypts a cookie value the way Chromium stores it: "v11", then AES-128-CBC with 16 spaces
// as the IV, of the host's SHA-256 followed by the value.
QByteArray chromeEncrypt(const QByteArray &value, const QString &host, const QByteArray &password,
                         const char *prefix = "v11") {
    const QByteArray key = BrowserCookies::deriveKey(password);
    const QByteArray plain = QCryptographicHash::hash(host.toUtf8(), QCryptographicHash::Sha256) + value;
    const QByteArray iv(16, ' ');
    QByteArray cipher(plain.size() + 16, Qt::Uninitialized);
    int length = 0, finalLength = 0;
    EVP_CIPHER_CTX *context = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(context, EVP_aes_128_cbc(), nullptr, reinterpret_cast<const unsigned char *>(key.constData()),
                       reinterpret_cast<const unsigned char *>(iv.constData()));
    EVP_EncryptUpdate(context, reinterpret_cast<unsigned char *>(cipher.data()), &length,
                      reinterpret_cast<const unsigned char *>(plain.constData()), int(plain.size()));
    EVP_EncryptFinal_ex(context, reinterpret_cast<unsigned char *>(cipher.data()) + length, &finalLength);
    EVP_CIPHER_CTX_free(context);
    cipher.truncate(length + finalLength);
    return prefix + cipher;
}

// Chromium's time for a moment: microseconds since 1601-01-01 UTC.
qint64 chromeTime(const QDateTime &time) {
    return (time.toMSecsSinceEpoch() + 11644473600000LL) * 1000;
}

}

class WebAppWrapTests : public QObject {
    Q_OBJECT

private slots:
    // MARK: Input

    void normalizesAddresses() {
        QCOMPARE(InputNormalizer::normalizeUrl("example.com").toString(), QString("https://example.com"));
        QCOMPARE(InputNormalizer::normalizeUrl("  https://example.com/app ").toString(), QString("https://example.com/app"));
        QCOMPARE(InputNormalizer::normalizeUrl("localhost:3000").toString(), QString("http://localhost:3000"));
        QCOMPARE(InputNormalizer::normalizeUrl("127.0.0.1:8080/x").toString(), QString("http://127.0.0.1:8080/x"));
        QVERIFY(!InputNormalizer::normalizeUrl("").isValid());
        QVERIFY(!InputNormalizer::normalizeUrl("ftp://example.com").isValid());
        QVERIFY(!InputNormalizer::normalizeUrl("not a site").isValid());
    }

    void makesSlugs() {
        QCOMPARE(InputNormalizer::slug("My Cool App!"), QString("my-cool-app"));
        QCOMPARE(InputNormalizer::slug("Café Crème"), QString("cafe-creme"));
        QCOMPARE(InputNormalizer::slug("!!!"), QString("app"));
        QCOMPARE(InputNormalizer::fileSafeName(" a/b:c "), QString("a-b-c"));
    }

    void splitsFoldersFromNames() {
        const AppLocation location("WebAppWrap/ Photos ");
        QCOMPARE(location.name, QString("Photos"));
        QCOMPARE(location.folders, QStringList{"WebAppWrap"});
        QCOMPARE(location.slug(), QString("photos"));
        QCOMPARE(AppLocation("../x/./Y").name, QString("Y"));
    }

    void readsColors() {
        QCOMPARE(HexColor::fromHex("#1a1a1a")->hexString(), QString("#1A1A1A"));
        QCOMPARE(HexColor::fromHex("fff")->hexString(), QString("#FFFFFF"));
        QVERIFY(!HexColor::fromHex("#12345"));
        QCOMPARE(HexColor::fromCss("#11223344")->hexString(), QString("#112233"));
        QCOMPARE(HexColor::fromCss("rgb(255, 0, 0)")->hexString(), QString("#FF0000"));
        QCOMPARE(HexColor::fromCss("rgba(0 128 255 / 50%)")->hexString(), QString("#0080FF"));
        QCOMPARE(HexColor::fromCss("rgb(100%, 0%, 0%)")->hexString(), QString("#FF0000"));
        QCOMPARE(HexColor::fromCss("White")->hexString(), QString("#FFFFFF"));
        QVERIFY(!HexColor::fromCss("rebeccapurple"));
        QVERIFY(HexColor::defaultTitleBar().isDark());
        QVERIFY(!HexColor(1, 1, 1).isDark());
    }

    // MARK: Links

    void routesLinks() {
        const LinkPolicy policy(QUrl("https://app.example.com"), {"accounts.google.com", "https://www.github.com/x"});
        using D = LinkPolicy::Destination;
        QCOMPARE(policy.destination(QUrl("https://app.example.com/inbox")), D::App);
        QCOMPARE(policy.destination(QUrl("https://www.app.example.com")), D::App);
        QCOMPARE(policy.destination(QUrl("https://eu.app.example.com")), D::App);
        QCOMPARE(policy.destination(QUrl("https://example.com")), D::Browser);
        QCOMPARE(policy.destination(QUrl("https://accounts.google.com/o/oauth2")), D::App);
        QCOMPARE(policy.destination(QUrl("https://gist.github.com")), D::App);
        QCOMPARE(policy.destination(QUrl("https://notgithub.com")), D::Browser);
        QCOMPARE(policy.destination(QUrl("mailto:me@example.com")), D::System);
        QCOMPARE(policy.destination(QUrl("zoommtg://zoom.us/join")), D::System);
        QCOMPARE(policy.destination(QUrl("about:blank")), D::App);
        QCOMPARE(policy.destination(QUrl("blob:https://app.example.com/x")), D::App);
        QCOMPARE(LinkPolicy::normalizeDomain("https://WWW.Example.com/page"), QString("example.com"));
        QCOMPARE(LinkPolicy::normalizeDomain("no spaces allowed"), QString());
    }

    // MARK: Batch files

    void readsJson5() {
        QString error;
        const QJsonValue value = Json5::parse(json5Sample, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        const QJsonObject object = value.toArray().first().toObject();
        QCOMPARE(object.value("name").toString(), QString("A"));
        QCOMPARE(object.value("zoom").toDouble(), 0.5);
        QCOMPARE(object.value("hex").toDouble(), 16.0);
        QVERIFY(Json5::parse("[1, 2", &error).isUndefined());
        QVERIFY(error.contains("line 1"));
    }

    void parsesBatchFiles() {
        QString error;
        const auto entries = BatchFile::parse(batchFile, QDir("/tmp/batch"), &error);
        QVERIFY2(entries, qPrintable(error));
        QCOMPARE(entries->size(), 2);
        const BatchEntry &photos = entries->first();
        QCOMPARE(photos.location.name, QString("Photos"));
        QCOMPARE(photos.url.toString(), QString("https://kphotos.vercel.app"));
        QCOMPARE(photos.image, QString("/tmp/batch/icons/p.svg"));
        QCOMPARE(photos.zoom, 0.8);
        QCOMPARE(photos.iconBackground->hexString(), QString("#FFFFFF"));
        QCOMPARE(photos.titleBarColor->hexString(), QString("#1A1A1A"));
        QVERIFY(!entries->last().titleBarColor);
    }

    void listsEveryBatchProblem() {
        QString error;
        const auto entries = BatchFile::parse(batchWithProblems, QDir("/tmp"), &error);
        QVERIFY(!entries);
        QVERIFY2(error.contains("unknown field “colour”"), qPrintable(error));
        QVERIFY(error.contains("Entry 2: “name” is missing; “url” is missing"));
        QVERIFY(error.contains("“zoom” must be a number from 0.5 to 3"));
        QVERIFY(error.contains("“titleBarColor” must be a color"));
        QVERIFY(error.contains("Entry 5 isn’t an object."));
        QVERIFY(!BatchFile::parse("[]", QDir("/tmp"), &error));
        QVERIFY(!BatchFile::parse("{}", QDir("/tmp"), &error));
    }

    void findsDuplicateBatchNames() {
        QString error;
        QVERIFY(!BatchFile::parse(batchDuplicateNames, QDir("/tmp"), &error));
        QVERIFY2(error.contains("More than one entry"), qPrintable(error));
    }

    // MARK: The list

    void savesAndLoadsTheList() {
        QTemporaryDir dir;
        AppRegistry registry(dir.path());
        QCOMPARE(registry.load()->size(), 0);

        AppRecord record{QUuid::createUuid(), AppSettings{"Linear", "linear.app", std::nullopt, 1, std::nullopt, std::nullopt, QString("#112233")}, std::nullopt};
        record.lastBuild = BuildStamp{QString("abc"), QDateTime::fromString("2026-11-24T10:00:00Z", Qt::ISODate), record.settings};
        QVERIFY(registry.save({record}));
        const auto loaded = registry.load();
        QCOMPARE(loaded->size(), 1);
        QCOMPARE(loaded->first(), record);

        // The same format as the macOS list: uppercase IDs, ISO dates, absent optionals.
        QFile file(registry.listFile());
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QJsonObject saved = QJsonDocument::fromJson(file.readAll()).object().value("apps").toArray().first().toObject();
        QCOMPARE(saved.value("id").toString(), record.idString());
        QVERIFY(saved.value("id").toString() == saved.value("id").toString().toUpper());
        QCOMPARE(saved.value("lastBuild").toObject().value("date").toString(), QString("2026-11-24T10:00:00Z"));
        QVERIFY(!saved.value("settings").toObject().contains("icon"));
    }

    void readsMacListFormat() {
        QTemporaryDir dir;
        QFile file(dir.path() + "/apps.json");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(macAppsJson);
        file.close();
        const auto records = AppRegistry(dir.path()).load();
        QCOMPARE(records->size(), 1);
        QCOMPARE(records->first().settings.location().name, QString("Photos"));
        QCOMPARE(records->first().settings.icon, std::optional<QString>("e621-1234.svg"));
        QVERIFY(!records->first().lastBuild);
    }

    void decidesWhatToRebuild() {
        using R = AppRecord::BuildReason;
        AppRecord record{QUuid::createUuid(), AppSettings{"A", "a.com"}, std::nullopt};
        QCOMPARE(record.buildReason(QString("v1"), true), std::optional(R::NeverBuilt));
        record.lastBuild = BuildStamp{QString("v1"), QDateTime::currentDateTimeUtc(), record.settings};
        QCOMPARE(record.buildReason(QString("v1"), true), std::nullopt);
        QCOMPARE(record.buildReason(QString("v1"), false), std::optional(R::Missing));
        QCOMPARE(record.buildReason(QString("v2"), true), std::optional(R::NewVersion));
        QCOMPARE(record.buildReason(std::nullopt, true), std::optional(R::UnknownVersion));
        record.settings.url = "b.com";
        QCOMPARE(record.buildReason(QString("v1"), true), std::optional(R::Changed));
    }

    void keepsAndRetiresIcons() {
        QTemporaryDir dir;
        AppRegistry registry(dir.path() + "/list");
        const QString source = dir.path() + "/logo.svg";
        writeSvg(source, "red");
        const QUuid id = QUuid::createUuid();
        const QString name = registry.importIcon(source, id, std::nullopt);
        QVERIFY(QFile::exists(registry.iconPath(name)));
        QVERIFY(name.endsWith(".svg"));
        // The same image again keeps the stored copy.
        QCOMPARE(registry.importIcon(source, id, name), name);
        const QString retired = registry.retireIcon(name, "Folder/Photos", QDateTime(QDate(2026, 1, 2), QTime(3, 4, 5)));
        QVERIFY(retired.endsWith("/previous-icons/Photos 2026-01-02 03.04.05.svg"));
        QVERIFY(!QFile::exists(registry.iconPath(name)));
    }

    // MARK: Building

    void buildsAnApp() {
        QTemporaryDir dir;
        const Paths paths = Paths::under(dir.path());
        const AppBuilder builder(paths, "/opt/web app wrap/webappwrap", QString("v1"));
        BatchEntry entry;
        entry.location = AppLocation("Work/My Mail");
        entry.url = QUrl("https://mail.example.com");
        const BuildOutcome outcome = builder.build(entry, HexColor::fromRgb(0x12, 0x34, 0x56), "LIST-ID");
        QVERIFY2(outcome.ok, qPrintable(outcome.error));
        QCOMPARE(outcome.slug, QString("my-mail"));
        QVERIFY(!outcome.replaced);
        QVERIFY(builder.isInstalled("my-mail"));
        QCOMPARE(QImage(paths.appIcon("my-mail")).size(), QSize(512, 512));

        QFile desktop(paths.desktopFile("my-mail"));
        QVERIFY(desktop.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(desktop.readAll());
        QVERIFY(text.contains("Name=My Mail\n"));
        QVERIFY(text.contains("Exec=\"/opt/web app wrap/webappwrap\" run my-mail\n"));
        QVERIFY(text.contains("StartupWMClass=webappwrap-my-mail\n"));

        const auto config = WrapperConfig::load(paths, "my-mail");
        QVERIFY(config);
        QCOMPARE(config->name, QString("My Mail"));
        QCOMPARE(config->url, QUrl("https://mail.example.com"));
        QCOMPARE(config->titleBarColor.hexString(), QString("#123456"));
        QCOMPARE(config->listId, QString("LIST-ID"));
        QCOMPARE(builder.installed().size(), 1);

        // Building again replaces it; removing keeps sign-ins unless asked.
        QVERIFY(builder.build(entry, HexColor::defaultTitleBar()).replaced);
        QDir().mkpath(paths.profileFolder("my-mail"));
        QVERIFY(builder.remove("my-mail", false));
        QVERIFY(!builder.isInstalled("my-mail"));
        QVERIFY(QFile::exists(paths.profileFolder("my-mail")));
        QVERIFY(builder.remove("my-mail", true));
        QVERIFY(!QFile::exists(paths.appFolder("my-mail")));
    }

    void quotesExecArguments() {
        QCOMPARE(AppBuilder::quoteExecArgument("/usr/bin/webappwrap"), QString("/usr/bin/webappwrap"));
        QCOMPARE(AppBuilder::quoteExecArgument("/a b/$x"), QString("\"/a b/\\\\$x\""));
    }

    // MARK: Icons

    void drawsIcons() {
        QTemporaryDir dir;
        const QImage letter = IconRenderer::render(IconSource::generated("Linear"), 128);
        QCOMPARE(letter.size(), QSize(128, 128));
        QCOMPARE(qAlpha(letter.pixel(0, 0)), 0); // rounded corner
        QVERIFY(qAlpha(letter.pixel(64, 4)) > 0);

        const QString svg = dir.path() + "/logo.svg";
        writeSvg(svg, "#ff0000");
        const auto source = IconSource::load(svg, 1, HexColor::fromRgb(0, 0, 255), HexColor::fromRgb(0, 255, 0));
        QVERIFY(source);
        const QImage icon = IconRenderer::render(*source, 100);
        // The background, and the SVG filled with its one color.
        QCOMPARE(QColor(icon.pixel(50, 8)).name(), QString("#0000ff"));
        QCOMPARE(QColor(icon.pixel(50, 50)).name(), QString("#00ff00"));

        QString error;
        QVERIFY(!IconSource::load(dir.path() + "/missing.svg", 1, std::nullopt, std::nullopt, &error));
        QVERIFY(!error.isEmpty());
    }

    void cropsImageBorders() {
        // A red square on a wide white border needs zooming in to lose the border.
        QImage image(200, 200, QImage::Format_ARGB32);
        image.fill(Qt::white);
        for (int y = 50; y < 150; ++y)
            for (int x = 50; x < 150; ++x)
                image.setPixel(x, y, qRgb(255, 0, 0));
        const double zoom = IconRenderer::automaticZoom(image);
        QVERIFY2(zoom > 1.9 && zoom < 2.6, qPrintable(QString::number(zoom)));
        QImage full(100, 100, QImage::Format_ARGB32);
        full.fill(Qt::red);
        QCOMPARE(IconRenderer::automaticZoom(full), 1.0);
    }

    // MARK: Theme colors

    void findsThemeColors() {
        QCOMPARE(ThemeColor::fromHtml(htmlPlainAndDark)->hexString(), QString("#FF0000"));
        QCOMPARE(ThemeColor::fromHtml(htmlLightAndDark)->hexString(), QString("#FFFFFF"));
        QVERIFY(!ThemeColor::fromHtml("<meta name=description content=x>"));
        QCOMPARE(ThemeColor::manifestUrl(htmlManifestLink, QUrl("https://a.com/x/y")),
                 QUrl("https://a.com/app.webmanifest"));
        QCOMPARE(ThemeColor::fromManifest(manifestJson)->hexString(), QString("#000000"));
    }

    // MARK: From the Omarchy app template

    void themeFallsBack() {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#FFD60A"));
    }

    void translatorFallsBackToEnglish() {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void decryptsChromiumCookieValues() {
        const QByteArray key = BrowserCookies::deriveKey("secret");
        const QByteArray encrypted = chromeEncrypt("abc123", ".google.com", "secret");
        QCOMPARE(BrowserCookies::decrypt(encrypted, key, ".google.com", 24), QByteArray("abc123"));
        // The wrong password, or a value moved to another host, doesn't decrypt.
        QVERIFY(!BrowserCookies::decrypt(encrypted, BrowserCookies::deriveKey("other"), ".google.com", 24));
        QVERIFY(!BrowserCookies::decrypt(encrypted, key, ".youtube.com", 24));
        // v10 values use the fixed password whatever the keyring holds.
        QCOMPARE(BrowserCookies::decrypt(chromeEncrypt("v", "a.com", "peanuts", "v10"), key, "a.com", 24), QByteArray("v"));
        QCOMPARE(BrowserCookies::fromChromeTime(11644473600000000LL), QDateTime::fromMSecsSinceEpoch(0, QTimeZone::UTC));
    }

    void readsACookiesFile() {
        QTemporaryDir dir;
        const QString file = dir.filePath("Cookies");
        {
            QSqlDatabase database = QSqlDatabase::addDatabase("QSQLITE", "fixture");
            database.setDatabaseName(file);
            QVERIFY(database.open());
            QSqlQuery query(database);
            QVERIFY(query.exec("CREATE TABLE meta (key TEXT, value TEXT)"));
            QVERIFY(query.exec("INSERT INTO meta VALUES ('version', '24')"));
            QVERIFY(query.exec("CREATE TABLE cookies (host_key TEXT, name TEXT, value TEXT, encrypted_value BLOB, "
                               "path TEXT, expires_utc INTEGER, is_secure INTEGER, is_httponly INTEGER, "
                               "has_expires INTEGER, samesite INTEGER, top_frame_site_key TEXT)"));
            const qint64 future = chromeTime(QDateTime::currentDateTimeUtc().addDays(30));
            const qint64 past = chromeTime(QDateTime::currentDateTimeUtc().addDays(-1));
            auto add = [&](const QString &host, const QString &name, qint64 expires, bool hasExpires, const QString &partition) {
                query.prepare("INSERT INTO cookies VALUES (?, ?, '', ?, '/', ?, 1, 1, ?, 0, ?)");
                query.addBindValue(host);
                query.addBindValue(name);
                query.addBindValue(chromeEncrypt(name.toUtf8() + "-value", host, "secret"));
                query.addBindValue(expires);
                query.addBindValue(hasExpires ? 1 : 0);
                query.addBindValue(partition);
                QVERIFY(query.exec());
            };
            add(".google.com", "SID", future, true, "");
            add("accounts.google.com", "__Host-GAPS", future, true, "");
            add(".youtube.com", "SESSION", 0, false, "");
            add(".google.com", "OLD", past, true, "");
            add(".google.com", "EMBEDDED", future, true, "https://example.com");
            database.close();
        }
        QSqlDatabase::removeDatabase("fixture");

        QString error;
        const auto cookies = BrowserCookies::readCookiesFile(file, "secret", &error);
        QVERIFY2(cookies, qPrintable(error));
        QCOMPARE(cookies->size(), 3); // not the expired or the partitioned one
        QHash<QByteArray, BrowserCookies::Cookie> byName;
        for (const auto &cookie : *cookies)
            byName.insert(cookie.cookie.name(), cookie);
        QCOMPARE(byName["SID"].cookie.value(), QByteArray("SID-value"));
        QCOMPARE(byName["SID"].cookie.domain(), QString(".google.com"));
        QCOMPARE(byName["SID"].origin, QUrl("https://google.com/"));
        QVERIFY(byName["SID"].cookie.isSecure() && byName["SID"].cookie.isHttpOnly());
        // A host-only cookie has no domain and is set from its host.
        QCOMPARE(byName["__Host-GAPS"].cookie.domain(), QString());
        QCOMPARE(byName["__Host-GAPS"].origin, QUrl("https://accounts.google.com/"));
        QVERIFY(byName["SESSION"].cookie.isSessionCookie());

        QVERIFY(!BrowserCookies::readCookiesFile(file, "wrong", &error));
        QVERIFY(error.contains("keyring"));
    }

private:
    static void writeSvg(const QString &path, const QString &fill) {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QString(svgSquare)
                       .arg(fill).toUtf8());
    }

};

QTEST_MAIN(WebAppWrapTests)
#include "webappwrap_tests.moc"
