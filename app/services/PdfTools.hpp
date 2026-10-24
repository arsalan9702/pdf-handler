#pragma once

#include <QObject>
#include <QUrl>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class DocumentListModel;
class ImageListModel;
class ProjectPageModel;
class QQmlEngine;
class QJSEngine;

// QML-facing entry points for every tool. Each method validates its input on the GUI thread,
// then hands the work to JobRunner; results arrive through the Jobs singleton's signals.
// Methods return false (and emit invalidInput) when nothing was started.
class PdfTools : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(Tools)
    QML_SINGLETON

  public:
    enum CompressPreset { Lossless, Balanced, Small };
    Q_ENUM(CompressPreset)
    enum SplitMode { EachPage, EveryN, ByRanges };
    Q_ENUM(SplitMode)

    explicit PdfTools(QObject* parent = nullptr) : QObject(parent) {}
    static PdfTools* create(QQmlEngine*, QJSEngine*);

    Q_INVOKABLE bool exportPages(ProjectPageModel* project, const QUrl& output,
                                 bool selectedOnly = false);
    Q_INVOKABLE bool extract(DocumentListModel* docs, int row, const QString& range,
                             const QUrl& output);
    Q_INVOKABLE bool merge(DocumentListModel* docs, const QUrl& output, bool bookmarkPerFile);
    Q_INVOKABLE bool split(DocumentListModel* docs, int row, int mode, int everyN,
                           const QString& ranges, const QString& nameTemplate, const QUrl& folder);
    // {names: [...]} or {error: "..."}, for the live preview under the split options.
    Q_INVOKABLE QVariantMap splitPreview(DocumentListModel* docs, int row, int mode, int everyN,
                                         const QString& ranges, const QString& nameTemplate);
    // One document: `target` is the output file. Several: `target` is a folder.
    Q_INVOKABLE bool compress(DocumentListModel* docs, int preset, const QUrl& target);
    Q_INVOKABLE bool protect(DocumentListModel* docs, const QString& userPassword,
                             const QString& ownerPassword, bool allowPrint, bool allowCopy,
                             bool allowModify, const QUrl& target);
    Q_INVOKABLE bool unlock(DocumentListModel* docs, const QUrl& target);
    Q_INVOKABLE QVariantMap readMetadata(DocumentListModel* docs, int row);
    Q_INVOKABLE bool writeMetadata(DocumentListModel* docs, int row, const QVariantMap& fields,
                                   const QUrl& output);
    Q_INVOKABLE bool imagesToPdf(ImageListModel* images, int pageSize, int orientation,
                                 double marginMm, const QUrl& output);
    Q_INVOKABLE bool pdfToImages(DocumentListModel* docs, int row, const QString& range, int dpi,
                                 const QString& format, int quality, const QUrl& folder);

    // Default save location: next to the document, "<name><suffix>.pdf".
    Q_INVOKABLE QUrl suggestedFile(DocumentListModel* docs, int row, const QString& suffix) const;
    Q_INVOKABLE QUrl folderOf(DocumentListModel* docs, int row) const;
    Q_INVOKABLE void reveal(const QString& path) const;
    Q_INVOKABLE void openFile(const QString& path) const;
    Q_INVOKABLE QString formatBytes(qint64 bytes) const;

  signals:
    void invalidInput(const QString& message);

  private:
    bool reject(const QString& message);
    bool requireReady(DocumentListModel* docs, int minimum = 1);
};
