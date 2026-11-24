// WebAppWrap: turns a web app into its own desktop app, with tabs.
//
// One binary does everything. With no arguments it opens the window for making apps; `run
// <app>` runs an app, which is what each app's launcher entry does; and `build`, `batch` and the
// other commands (also reachable as `waw`) work without a display.

#include <QFileInfo>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTextStream>
#include <QUrl>
#include <QtWebEngineQuick/qtwebenginequickglobal.h>

#include "appbuilder.h"
#include "applistmodel.h"
#include "cli.h"
#include "paths.h"
#include "runtime.h"
#include "theme.h"
#include "translator.h"
#include "version.h"

namespace {

int runCommandLine(int argc, char *argv[], const QStringList &arguments) {
    // Drawing icons needs a GUI application, but not a display.
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    app.setApplicationName("webappwrap");
    Translator translator;
    app.installTranslator(&translator);
    if (arguments.isEmpty()) {
        QTextStream(stdout) << Cli::helpText();
        return 0;
    }
    if (!Cli::handles(arguments)) {
        QTextStream(stderr) << "waw: Unknown command “" << arguments.first() << "”. Run “waw help” for the commands.\n";
        return 1;
    }
    return Cli::run(arguments);
}

int runApp(int argc, char *argv[], const QString &slug) {
    const Paths paths = Paths::standard();
    QString error;
    const auto config = WrapperConfig::load(paths, slug, &error);
    if (!config) {
        QTextStream(stderr) << "webappwrap: " << error << "\n";
        return 1;
    }

    // Must come before the application exists.
    QtWebEngineQuick::initialize();
    QGuiApplication app(argc, argv);
    const QString desktopId = Paths::desktopId(config->slug);
    app.setApplicationName(desktopId);
    app.setApplicationDisplayName(config->name);
    // Matches the windows to the app's desktop entry, so Hyprland (Wayland app ID) and the
    // launcher give them the app's own name and icon.
    app.setDesktopFileName(desktopId);
    app.setWindowIcon(QIcon(paths.appIcon(config->slug)));

    Runtime runtime(paths, *config);
    // Launching a running app brings its window forward instead of starting a second copy.
    if (!runtime.claimSingleInstance())
        return 0;

    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    Theme theme;

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("app", &runtime);
    engine.load(QUrl("qrc:/qml/AppRoot.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}

int runWindow(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("webappwrap");
    app.setApplicationDisplayName("WebAppWrap");
    app.setDesktopFileName("webappwrap");
    app.setWindowIcon(QIcon::fromTheme("webappwrap"));

    QQuickStyle::setStyle("Material");
    Translator translator;
    app.installTranslator(&translator);
    Theme theme;
    AppListModel apps(Paths::standard(), AppBuilder::defaultExecutable(), Version::commit());

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.addImageProvider("icons", new IconImageProvider(&apps));
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("apps", &apps);
    engine.rootContext()->setContextProperty("helpText", Cli::helpText());
    engine.load(QUrl("qrc:/qml/MakerWindow.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    return app.exec();
}

}

int main(int argc, char *argv[]) {
    QStringList arguments;
    for (int i = 1; i < argc; ++i)
        arguments << QString::fromLocal8Bit(argv[i]);

    if (QFileInfo(QString::fromLocal8Bit(argv[0])).fileName() == "waw" || Cli::handles(arguments))
        return runCommandLine(argc, argv, arguments);
    if (arguments.value(0) == "run") {
        if (arguments.size() != 2) {
            QTextStream(stderr) << "webappwrap: Give the app to run: webappwrap run <app>\n";
            return 1;
        }
        return runApp(argc, argv, AppLocation(arguments[1]).slug());
    }
    if (!arguments.isEmpty() && arguments.first() != "open") {
        QTextStream(stderr) << "webappwrap: Unknown command “" << arguments.first() << "”.\n\n" << Cli::helpText();
        return 1;
    }
    return runWindow(argc, argv);
}
