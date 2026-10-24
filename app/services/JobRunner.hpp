#pragma once

#include "core/Types.hpp"

#include <QObject>
#include <QStringList>
#include <QThreadPool>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;

#include <functional>
#include <memory>

// Result of a finished job, shown to the user as a toast.
struct JobOutcome {
    QStringList outputs;
    QStringList warnings;
    QString summary; // optional one-line result, e.g. "4.2 MB → 1.1 MB"
};

// Runs one PDF job at a time on a worker thread, never on the GUI thread. Exposed to QML as
// the `Jobs` singleton for progress, cancel and result notifications.
class JobRunner : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Jobs)
    QML_SINGLETON
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString title READ title NOTIFY busyChanged)

  public:
    using Work = std::function<JobOutcome(const pdfcore::JobContext&)>;

    explicit JobRunner(QObject* parent = nullptr);
    ~JobRunner() override;

    static JobRunner* instance();
    static JobRunner* create(QQmlEngine*, QJSEngine*);

    // Starts `work` unless a job is already running (returns false then).
    bool start(const QString& title, Work work);

    Q_INVOKABLE void cancel();

    [[nodiscard]] bool busy() const { return m_busy; }
    [[nodiscard]] double progress() const { return m_progress; }
    [[nodiscard]] QString title() const { return m_title; }

  signals:
    void busyChanged();
    void progressChanged();
    void finished(const QString& title, const QStringList& outputs, const QStringList& warnings,
                  const QString& summary);
    void failed(const QString& title, const QString& message, bool cancelled);

  private:
    void setProgress(double value);
    void complete();

    bool m_busy = false;
    double m_progress = 0;
    QString m_title;
    std::shared_ptr<pdfcore::CancelToken> m_cancel;
    // Owned (not the global pool) so it can be drained safely in the destructor, which runs
    // after QCoreApplication has torn down the global pool.
    QThreadPool m_pool;
};
