#include "services/AppController.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QGuiApplication>
#include <QImage>
#include <QJSEngine>
#include <QLocalServer>
#include <QLocalSocket>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr auto kAppId = "io.github.pdfhandler";
constexpr auto kIconResource = ":/qt/qml/PdfHandler/qml/brand/app-icon.svg";

AppController* g_instance = nullptr;

QString serverName() {
    // One instance per user; the hash keeps the name short and free of odd characters.
    const QByteArray user = qgetenv("USER") + qgetenv("USERNAME");
    return QStringLiteral("pdf-handler-") +
           QString::fromLatin1(
               QCryptographicHash::hash(user, QCryptographicHash::Sha1).toHex().left(12));
}

QString dataHome() {
    const QString xdg = qEnvironmentVariable("XDG_DATA_HOME");
    return xdg.isEmpty() ? QDir::home().filePath(QStringLiteral(".local/share")) : xdg;
}

QString desktopFilePath() {
    return dataHome() + QStringLiteral("/applications/%1.desktop").arg(QLatin1String(kAppId));
}

QString cliPath() {
    return QDir::home().filePath(QStringLiteral(".local/bin/pdfh"));
}

// The file the launcher should start: the AppImage itself when running from one.
QString launchTarget() {
    const QString appImage = qEnvironmentVariable("APPIMAGE");
    return appImage.isEmpty() ? QCoreApplication::applicationFilePath() : appImage;
}

bool writeFile(const QString& path, const QByteArray& data, QString* error) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(data) != data.size()) {
        *error = QObject::tr("Could not write %1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

QString quoteExec(const QString& path) {
    QString q = path;
    q.replace(u'\\', QStringLiteral("\\\\")).replace(u'"', QStringLiteral("\\\""));
    return u'"' + q + u'"';
}

} // namespace

AppController::AppController(QObject* parent) : QObject(parent) {}

AppController* AppController::instance() {
    if (g_instance == nullptr) {
        g_instance = new AppController(QCoreApplication::instance());
    }
    return g_instance;
}

AppController* AppController::create(QQmlEngine*, QJSEngine*) {
    auto* app = instance();
    QJSEngine::setObjectOwnership(app, QJSEngine::CppOwnership);
    return app;
}

bool AppController::forwardToRunningInstance(const QStringList& files) {
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(500)) {
        return false;
    }
#ifdef Q_OS_WIN
    AllowSetForegroundWindow(ASFW_ANY); // let the running window come to the front
#endif
    socket.write(files.join(u'\n').toUtf8() + "\n\n");
    socket.waitForBytesWritten(2000);
    socket.disconnectFromServer();
    return true;
}

void AppController::listenForInstances() {
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);
    if (!m_server->listen(serverName())) {
        // A crashed instance can leave a stale socket file behind on Unix.
        QLocalServer::removeServer(serverName());
        m_server->listen(serverName());
    }
    connect(m_server, &QLocalServer::newConnection, this, [this] {
        while (auto* socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, [this, socket] {
                const auto text = QString::fromUtf8(socket->readAll());
                socket->deleteLater();
                openFiles(text.split(u'\n', Qt::SkipEmptyParts));
                for (auto* window : QGuiApplication::topLevelWindows()) {
                    window->show();
                    window->raise();
                    window->requestActivate();
                }
            });
        }
    });
}

void AppController::openFiles(const QStringList& paths) {
    for (const auto& p : paths) {
        if (QFileInfo(p).isFile()) {
            m_pending << QFileInfo(p).absoluteFilePath();
        }
    }
    if (!m_pending.isEmpty()) {
        emit filesOpened();
    }
}

QList<QUrl> AppController::takePendingFiles() {
    QList<QUrl> urls;
    for (const auto& p : std::as_const(m_pending)) {
        urls << QUrl::fromLocalFile(p);
    }
    m_pending.clear();
    return urls;
}

bool AppController::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::FileOpen) { // macOS Finder / Dock
        openFiles({static_cast<QFileOpenEvent*>(event)->file()});
        return true;
    }
    return QObject::eventFilter(watched, event);
}

QString AppController::version() const {
    return QCoreApplication::applicationVersion();
}

QString AppController::platform() const {
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

bool AppController::canInstallLauncher() const {
#if defined(Q_OS_LINUX)
    return true;
#else
    return false; // the Windows installer and macOS bundle handle this
#endif
}

bool AppController::launcherInstalled() const {
    return QFileInfo::exists(desktopFilePath());
}

QString AppController::installLauncher(bool makeDefault) {
    QString error;
    QString target = launchTarget();

    // An AppImage in Downloads tends to get deleted; keep a copy in ~/Applications.
    if (!qEnvironmentVariable("APPIMAGE").isEmpty()) {
        const QString apps = QDir::home().filePath(QStringLiteral("Applications"));
        const QString dest = apps + QStringLiteral("/PDF-Handler.AppImage");
        if (QFileInfo(target).absoluteFilePath() != QFileInfo(dest).absoluteFilePath()) {
            QDir().mkpath(apps);
            QFile::remove(dest);
            if (!QFile::copy(target, dest)) {
                return tr("Could not copy the app to %1").arg(apps);
            }
        }
        QFile::setPermissions(dest, QFile::permissions(dest) | QFileDevice::ExeUser |
                                        QFileDevice::ExeGroup | QFileDevice::ExeOther);
        target = dest;
    }

    // Icons: scalable SVG plus rendered PNGs for desktops that prefer bitmaps.
    QFile svg(QString::fromLatin1(kIconResource));
    if (!svg.open(QIODevice::ReadOnly)) {
        return tr("The app icon is missing from this build");
    }
    const QByteArray svgData = svg.readAll();
    const QString icons = dataHome() + QStringLiteral("/icons/hicolor");
    if (!writeFile(icons + QStringLiteral("/scalable/apps/%1.svg").arg(QLatin1String(kAppId)),
                   svgData, &error)) {
        return error;
    }
    QSvgRenderer renderer(svgData);
    for (int size : {16, 32, 48, 64, 128, 256, 512}) {
        QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        renderer.render(&painter);
        painter.end();
        const QString path =
            icons + QStringLiteral("/%1x%1/apps/%2.png").arg(size).arg(QLatin1String(kAppId));
        QDir().mkpath(QFileInfo(path).absolutePath());
        image.save(path);
    }

    const QByteArray desktop =
        QStringLiteral("[Desktop Entry]\n"
                       "Type=Application\n"
                       "Name=PDF Handler\n"
                       "GenericName=PDF Toolkit\n"
                       "Comment=Merge, split, organize, compress and protect PDFs\n"
                       "Exec=%1 %F\n"
                       "Icon=%2\n"
                       "Terminal=false\n"
                       "Categories=Office;Graphics;Viewer;\n"
                       "MimeType=application/pdf;image/png;image/jpeg;\n"
                       "Keywords=pdf;merge;split;compress;encrypt;organize;\n"
                       "StartupWMClass=%2\n")
            .arg(quoteExec(target), QLatin1String(kAppId))
            .toUtf8();
    if (!writeFile(desktopFilePath(), desktop, &error)) {
        return error;
    }

    // `pdfh` on the PATH for terminal use.
    const QByteArray script =
        QStringLiteral("#!/bin/sh\nexec %1 --cli \"$@\"\n").arg(quoteExec(target)).toUtf8();
    if (writeFile(cliPath(), script, &error)) {
        QFile::setPermissions(cliPath(), QFile::permissions(cliPath()) | QFileDevice::ExeUser);
    }

    QProcess::execute(QStringLiteral("update-desktop-database"),
                      {dataHome() + QStringLiteral("/applications")});
    QProcess::execute(QStringLiteral("gtk-update-icon-cache"),
                      {QStringLiteral("-f"), QStringLiteral("-t"), icons});
    if (makeDefault) {
        QProcess::execute(QStringLiteral("xdg-mime"),
                          {QStringLiteral("default"),
                           QLatin1String(kAppId) + QStringLiteral(".desktop"),
                           QStringLiteral("application/pdf")});
    }
    emit launcherChanged();
    return {};
}

QString AppController::uninstallLauncher() {
    QFile::remove(desktopFilePath());
    QFile::remove(cliPath());
    const QString icons = dataHome() + QStringLiteral("/icons/hicolor");
    QFile::remove(icons + QStringLiteral("/scalable/apps/%1.svg").arg(QLatin1String(kAppId)));
    for (int size : {16, 32, 48, 64, 128, 256, 512}) {
        QFile::remove(icons +
                      QStringLiteral("/%1x%1/apps/%2.png").arg(size).arg(QLatin1String(kAppId)));
    }
    QProcess::execute(QStringLiteral("update-desktop-database"),
                      {dataHome() + QStringLiteral("/applications")});
    emit launcherChanged();
    return {};
}
