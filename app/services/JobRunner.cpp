#include "services/JobRunner.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJSEngine>
#include <QPointer>
#include <exception>

namespace {
JobRunner* g_instance = nullptr;
}

JobRunner::JobRunner(QObject* parent) : QObject(parent) {
    g_instance = this;
    m_pool.setMaxThreadCount(1);
}

JobRunner::~JobRunner() {
    if (m_cancel) {
        m_cancel->cancel();
    }
    m_pool.waitForDone();
    g_instance = nullptr;
}

JobRunner* JobRunner::instance() {
    if (g_instance == nullptr) {
        // Parented to the application so it lives exactly as long as the event loop.
        g_instance = new JobRunner(QCoreApplication::instance());
    }
    return g_instance;
}

JobRunner* JobRunner::create(QQmlEngine*, QJSEngine*) {
    auto* runner = instance();
    QJSEngine::setObjectOwnership(runner, QJSEngine::CppOwnership);
    return runner;
}

bool JobRunner::start(const QString& title, Work work) {
    if (m_busy) {
        return false;
    }
    m_busy = true;
    m_title = title;
    m_cancel = std::make_shared<pdfcore::CancelToken>();
    setProgress(0);
    emit busyChanged();

    QPointer<JobRunner> self(this);
    auto cancel = m_cancel;
    m_pool.start([self, cancel, title, work = std::move(work)] {
        QElapsedTimer throttle;
        throttle.start();
        pdfcore::JobContext ctx;
        ctx.cancel = cancel.get();
        ctx.progress = [self, &throttle](int done, int total) {
            // Coalesce updates: at most ~30 per second reach the GUI thread.
            if (total <= 0 || (throttle.elapsed() < 33 && done < total)) {
                return;
            }
            throttle.restart();
            const double value = static_cast<double>(done) / total;
            QMetaObject::invokeMethod(
                self,
                [self, value] {
                    if (self)
                        self->setProgress(value);
                },
                Qt::QueuedConnection);
        };

        JobOutcome outcome;
        QString error;
        bool cancelled = false;
        try {
            outcome = work(ctx);
        } catch (const pdfcore::PdfError& e) {
            cancelled = e.kind() == pdfcore::ErrorKind::Cancelled;
            error = QString::fromUtf8(e.what());
        } catch (const std::exception& e) {
            error = QString::fromUtf8(e.what());
        } catch (...) {
            error = QStringLiteral("Unexpected error");
        }

        QMetaObject::invokeMethod(
            self,
            [self, title, outcome, error, cancelled] {
                if (!self) {
                    return;
                }
                self->complete();
                if (error.isEmpty()) {
                    emit self->finished(title, outcome.outputs, outcome.warnings, outcome.summary);
                } else {
                    emit self->failed(title, cancelled ? QStringLiteral("Cancelled") : error,
                                      cancelled);
                }
            },
            Qt::QueuedConnection);
    });
    return true;
}

void JobRunner::cancel() {
    if (m_cancel) {
        m_cancel->cancel();
    }
}

void JobRunner::setProgress(double value) {
    if (!qFuzzyCompare(value + 1, m_progress + 1)) {
        m_progress = value;
        emit progressChanged();
    }
}

void JobRunner::complete() {
    setProgress(1);
    m_busy = false;
    m_cancel.reset();
    emit busyChanged();
}
