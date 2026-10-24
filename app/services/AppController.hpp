#pragma once

#include <QObject>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

class QLocalServer;
class QQmlEngine;
class QJSEngine;

// Application-level glue exposed to QML as `App`: files opened from outside (command line,
// file manager "Open with", macOS Finder, a second launch), and desktop integration.
class AppController : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(App)
    QML_SINGLETON
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(QString platform READ platform CONSTANT)
    Q_PROPERTY(bool canInstallLauncher READ canInstallLauncher CONSTANT)
    Q_PROPERTY(bool launcherInstalled READ launcherInstalled NOTIFY launcherChanged)

  public:
    static AppController* instance();
    static AppController* create(QQmlEngine*, QJSEngine*);

    // Returns true when another instance accepted `files`; this process should then exit.
    bool forwardToRunningInstance(const QStringList& files);
    // Starts accepting files from later launches.
    void listenForInstances();

    void openFiles(const QStringList& paths);
    Q_INVOKABLE QList<QUrl> takePendingFiles();

    // Linux: adds a launcher entry, icons, "Open with" for PDFs and a `pdfh` command under
    // ~/.local. `makeDefault` also makes this the default PDF app. Returns "" or an error.
    Q_INVOKABLE QString installLauncher(bool makeDefault);
    Q_INVOKABLE QString uninstallLauncher();

    [[nodiscard]] QString version() const;
    [[nodiscard]] QString platform() const;
    [[nodiscard]] bool canInstallLauncher() const;
    [[nodiscard]] bool launcherInstalled() const;

  signals:
    void filesOpened();
    void launcherChanged();

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    explicit AppController(QObject* parent);

    QStringList m_pending;
    QLocalServer* m_server = nullptr;
};
