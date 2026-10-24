#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("PDF Handler"));
    QGuiApplication::setOrganizationName(QStringLiteral("PdfHandler"));
    QGuiApplication::setApplicationVersion(QStringLiteral(PDFH_VERSION));

    // Basic style + our own Theme keeps the UI identical on every OS.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QQmlApplicationEngine engine;
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("PdfHandler", "Main");

    return QGuiApplication::exec();
}
