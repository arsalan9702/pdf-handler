#include "cli/Cli.hpp"
#include "services/AppController.hpp"
#include "services/IconProvider.hpp"
#include "services/ThumbnailProvider.hpp"

#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <cstring>

int main(int argc, char* argv[]) {
    // `pdf-handler --cli <command> ...` runs the pdfh command-line tool (used by the AppImage).
    if (argc > 1 && std::strcmp(argv[1], "--cli") == 0) {
        auto args = utf8Args(argc, argv);
        args.erase(args.begin());
        return runCli(args);
    }

    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("PDF Handler"));
    QGuiApplication::setOrganizationName(QStringLiteral("PdfHandler"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PDFH_VERSION));
    // Matches the .desktop file so Wayland/X11 docks show the right icon and name.
    QGuiApplication::setDesktopFileName(QStringLiteral("io.github.pdfhandler"));
    QGuiApplication::setWindowIcon(
        QIcon(QStringLiteral(":/qt/qml/PdfHandler/qml/brand/app-icon.svg")));

    QStringList files;
    for (const auto& arg : QCoreApplication::arguments().mid(1)) {
        if (!arg.startsWith(u'-') && QFileInfo(arg).isFile()) {
            files << QFileInfo(arg).absoluteFilePath();
        }
    }
    auto* controller = AppController::instance();
    if (controller->forwardToRunningInstance(files)) {
        return 0; // an existing window took the files
    }
    controller->listenForInstances();
    app.installEventFilter(controller);
    controller->openFiles(files);

    if (QFontDatabase::addApplicationFont(
            QStringLiteral(":/qt/qml/PdfHandler/qml/fonts/InterVariable.ttf")) >= 0) {
        QFont font(QStringLiteral("Inter"));
        font.setPixelSize(14);
        font.setHintingPreference(QFont::PreferNoHinting);
        QGuiApplication::setFont(font);
    }

    // Basic style + our own Theme keeps the UI identical on every OS.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("thumb"), new ThumbnailProvider);
    engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("PdfHandler", "Main");

    // Developer hook for visual checks and docs: PDFH_SCREENSHOT=<file.png> renders the window,
    // optionally on PDFH_PAGE=<tool key>, saves it after PDFH_DELAY ms (default 2500) and exits.
    if (const QString shot = qEnvironmentVariable("PDFH_SCREENSHOT"); !shot.isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().value(0));
        if (window != nullptr) {
            if (const QString page = qEnvironmentVariable("PDFH_PAGE"); !page.isEmpty()) {
                window->setProperty("currentKey", page);
            }
            const int delay = qEnvironmentVariableIntValue("PDFH_DELAY");
            QTimer::singleShot(delay > 0 ? delay : 2500, window, [window, shot] {
                window->grabWindow().save(shot);
                QCoreApplication::quit();
            });
        }
    }

    return QGuiApplication::exec();
}
