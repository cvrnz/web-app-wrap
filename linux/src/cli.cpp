#include "cli.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

#include "appbuilder.h"
#include "batchfile.h"
#include "iconrenderer.h"
#include "paths.h"
#include "themecolor.h"
#include "version.h"

namespace {

const QStringList commands{"build", "batch", "icon", "list", "remove", "help", "version", "--help", "-h", "--version"};

QTextStream &out() {
    static QTextStream stream(stdout);
    return stream;
}

QTextStream &err() {
    static QTextStream stream(stderr);
    return stream;
}

int fail(const QString &message) {
    err() << "waw: " << message << Qt::endl;
    return 1;
}

// "--name Photos --url x --json" -> {name: Photos, url: x, json: ""}, with the rest positional.
struct Arguments {
    QHash<QString, QString> options;
    QStringList positional;
    QString error;
};

Arguments parse(const QStringList &arguments, const QStringList &valueOptions, const QStringList &flags) {
    Arguments parsed;
    for (qsizetype i = 0; i < arguments.size(); ++i) {
        const QString argument = arguments[i];
        if (!argument.startsWith("--")) {
            parsed.positional << argument;
            continue;
        }
        QString name = argument.mid(2), value;
        const qsizetype equals = name.indexOf('=');
        if (equals >= 0) {
            value = name.mid(equals + 1);
            name.truncate(equals);
        }
        if (flags.contains(name)) {
            parsed.options.insert(name, {});
        } else if (valueOptions.contains(name)) {
            if (equals < 0) {
                if (i + 1 >= arguments.size()) {
                    parsed.error = QString("--%1 needs a value.").arg(name);
                    return parsed;
                }
                value = arguments[++i];
            }
            parsed.options.insert(name, value);
        } else {
            parsed.error = QString("Unknown option --%1. Run “waw help” for the options.").arg(name);
            return parsed;
        }
    }
    return parsed;
}

const QStringList iconOptions{"name", "url", "image", "zoom", "icon-background", "icon-color", "title-bar-color", "out", "size"};

// The same checks as a batch entry, by building a one-entry batch from the options.
std::optional<BatchEntry> entryFromOptions(const Arguments &args, bool needsUrl, QString *error) {
    QJsonObject object{{"name", args.options.value("name")}};
    object.insert("url", needsUrl ? args.options.value("url") : args.options.value("url", "https://example.com"));
    if (args.options.contains("image"))
        object.insert("image", BatchFile::resolvePath(args.options.value("image"), QDir::current()));
    if (args.options.contains("zoom")) {
        bool ok = false;
        const double zoom = args.options.value("zoom").toDouble(&ok);
        object.insert("zoom", ok ? QJsonValue(zoom) : QJsonValue(args.options.value("zoom")));
    }
    if (args.options.contains("icon-background"))
        object.insert("iconBackground", args.options.value("icon-background"));
    if (args.options.contains("icon-color"))
        object.insert("iconColor", args.options.value("icon-color"));
    if (args.options.contains("title-bar-color"))
        object.insert("titleBarColor", args.options.value("title-bar-color"));
    QString problems;
    const auto entries = BatchFile::parse(QJsonDocument(QJsonArray{object}).toJson(), QDir::current(), &problems);
    if (!entries) {
        // Batch messages say "Entry 1 (name): …"; on the command line, say it about the options.
        problems.replace(QRegularExpression("^Entry 1( \\([^)]*\\))?: "), "");
        problems.replace("“name”", "--name").replace("“url”", "--url").replace("“image”", "--image")
            .replace("“zoom”", "--zoom").replace("“iconBackground”", "--icon-background")
            .replace("“iconColor”", "--icon-color").replace("“titleBarColor”", "--title-bar-color");
        *error = problems;
        return std::nullopt;
    }
    return entries->first();
}

HexColor titleBarColorFor(const BatchEntry &entry) {
    if (entry.titleBarColor)
        return *entry.titleBarColor;
    return ThemeColorFetcher::fetchBlocking(entry.url).value_or(HexColor::defaultTitleBar());
}

AppBuilder builder() {
    return AppBuilder(Paths::standard(), AppBuilder::defaultExecutable(), Version::commit());
}

QJsonObject outcomeJson(const BatchEntry &entry, const BuildOutcome &outcome) {
    QJsonObject object{{"name", entry.location.name}, {"ok", outcome.ok}, {"slug", outcome.slug},
                       {"desktopFile", outcome.desktopFile}, {"replaced", outcome.replaced}};
    if (!outcome.ok)
        object.insert("error", outcome.error);
    return object;
}

int buildCommand(const QStringList &arguments) {
    const Arguments args = parse(arguments, iconOptions, {"json"});
    if (!args.error.isEmpty())
        return fail(args.error);
    QString error;
    const auto entry = entryFromOptions(args, true, &error);
    if (!entry)
        return fail(error);
    const BuildOutcome outcome = builder().build(*entry, titleBarColorFor(*entry));
    if (args.options.contains("json"))
        out() << QJsonDocument(outcomeJson(*entry, outcome)).toJson(QJsonDocument::Indented);
    if (!outcome.ok)
        return fail(outcome.error);
    if (!args.options.contains("json"))
        out() << (outcome.replaced ? "Rebuilt " : "Built ") << entry->location.name << " (" << outcome.desktopFile << ")" << Qt::endl;
    return 0;
}

int batchCommand(const QStringList &arguments) {
    const Arguments args = parse(arguments, {}, {"json"});
    if (!args.error.isEmpty())
        return fail(args.error);
    if (args.positional.size() != 1)
        return fail("Give one JSON file: waw batch apps.json");
    QFile file(args.positional.first());
    if (!file.open(QIODevice::ReadOnly))
        return fail(QString("Couldn’t open %1: %2").arg(file.fileName(), file.errorString()));
    QString error;
    const auto entries = BatchFile::parse(file.readAll(), QFileInfo(file).absoluteDir(), &error);
    if (!entries)
        return fail(error);

    const AppBuilder appBuilder = builder();
    QJsonArray results;
    QStringList failed;
    for (const BatchEntry &entry : *entries) {
        if (!args.options.contains("json"))
            out() << "Building " << entry.location.name << "…" << Qt::endl;
        const BuildOutcome outcome = appBuilder.build(entry, titleBarColorFor(entry));
        results.append(outcomeJson(entry, outcome));
        if (!outcome.ok)
            failed << QString("%1: %2").arg(entry.location.name, outcome.error);
    }
    if (args.options.contains("json"))
        out() << QJsonDocument(results).toJson(QJsonDocument::Indented);
    if (!failed.isEmpty())
        return fail(QString("%1 of %2 apps didn’t build:\n%3").arg(failed.size()).arg(entries->size()).arg(failed.join('\n')));
    if (!args.options.contains("json"))
        out() << "Built " << entries->size() << (entries->size() == 1 ? " app." : " apps.") << Qt::endl;
    return 0;
}

int iconCommand(const QStringList &arguments) {
    const Arguments args = parse(arguments, iconOptions, {});
    if (!args.error.isEmpty())
        return fail(args.error);
    if (!args.options.contains("out"))
        return fail("--out is required, for example --out Linear.png");
    QString error;
    const auto entry = entryFromOptions(args, false, &error);
    if (!entry)
        return fail(error);
    const int size = args.options.value("size", "512").toInt();
    if (size < 16 || size > 2048)
        return fail("--size must be from 16 to 2048.");
    const auto source = AppBuilder::iconSource(*entry, &error);
    if (!source)
        return fail(error);
    const QString path = BatchFile::resolvePath(args.options.value("out"), QDir::current());
    if (!IconRenderer::render(*source, size).save(path))
        return fail(QString("Couldn’t write %1.").arg(path));
    out() << "Wrote " << path << Qt::endl;
    return 0;
}

int listCommand(const QStringList &arguments) {
    const Arguments args = parse(arguments, {}, {"json"});
    if (!args.error.isEmpty())
        return fail(args.error);
    const QList<WrapperConfig> apps = builder().installed();
    if (args.options.contains("json")) {
        QJsonArray array;
        for (const WrapperConfig &app : apps)
            array.append(QJsonObject{{"name", app.name}, {"slug", app.slug}, {"url", app.url.toString()},
                                     {"titleBarColor", app.titleBarColor.hexString()}, {"version", app.version},
                                     {"inList", !app.listId.isEmpty()}});
        out() << QJsonDocument(array).toJson(QJsonDocument::Indented);
        return 0;
    }
    if (apps.isEmpty())
        out() << "No apps yet. Make one with: waw build --name Linear --url linear.app" << Qt::endl;
    for (const WrapperConfig &app : apps)
        out() << app.name << "\t" << app.url.toString() << "\t(" << app.slug << ")" << Qt::endl;
    return 0;
}

int removeCommand(const QStringList &arguments) {
    const Arguments args = parse(arguments, {}, {"delete-data"});
    if (!args.error.isEmpty())
        return fail(args.error);
    if (args.positional.size() != 1)
        return fail("Give the app's name: waw remove Linear");
    const QString slug = AppLocation(args.positional.first()).slug();
    QString error;
    if (!builder().remove(slug, args.options.contains("delete-data"), &error))
        return fail(error);
    out() << "Removed " << args.positional.first()
          << (args.options.contains("delete-data") ? " and its sign-ins." : ". Its sign-ins are kept; add --delete-data to remove them too.")
          << Qt::endl;
    return 0;
}

}

namespace Cli {

bool handles(const QStringList &arguments) {
    return !arguments.isEmpty() && commands.contains(arguments.first());
}

QString helpText() {
    return QStringLiteral(R"(WebAppWrap turns a web app into its own desktop app, with tabs.

Usage:
  webappwrap                      open the WebAppWrap window
  webappwrap run <app>            run an app (what its launcher entry does)
  waw build --name <name> --url <address> [options]
  waw batch <apps.json>           build every app in a JSON, JSONC or JSON5 file
  waw icon --name <name> --out <file.png> [options]   just draw the icon
  waw list [--json]               the installed apps
  waw remove <name> [--delete-data]
  waw version

Options for build and icon:
  --name <name>                   the app's name (required)
  --url <address>                 its web address (required for build); https:// is optional
  --image <file>                  an SVG (or another image, cropped to fill the icon)
  --zoom <0.5-3>                  1 fits the whole SVG in the icon
  --icon-background <color>       behind an SVG; default #1C1C1C
  --icon-color <color>            fill the whole SVG with one color
  --title-bar-color <color>       default: the site's theme color
  --size <pixels>                 icon only; default 512
  --json                          build and batch: print the results as JSON

Apps made here aren't added to the window's list. "waw" is the same program as "webappwrap".
)");
}

int run(const QStringList &arguments) {
    const QString command = arguments.first();
    const QStringList rest = arguments.mid(1);
    if (command == "build")
        return buildCommand(rest);
    if (command == "batch")
        return batchCommand(rest);
    if (command == "icon")
        return iconCommand(rest);
    if (command == "list")
        return listCommand(rest);
    if (command == "remove")
        return removeCommand(rest);
    if (command == "version" || command == "--version") {
        out() << "WebAppWrap " << Version::display() << Qt::endl;
        return 0;
    }
    out() << helpText();
    return 0;
}

}
