#pragma once

#include "core/Types.hpp"

#include <QAbstractListModel>
#include <QStringList>
#include <QUrl>
#include <QtQml/qqmlregistration.h>
#include <vector>

// The PDFs a tool works on. Each file is opened off the GUI thread when added; encrypted files
// wait in NeedsPassword until unlock() succeeds, so every later job already has the password.
class DocumentListModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(DocumentList)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int readyCount READ readyCount NOTIFY statusChanged)
    Q_PROPERTY(bool allReady READ allReady NOTIFY statusChanged)
    Q_PROPERTY(int maxDocuments READ maxDocuments WRITE setMaxDocuments NOTIFY maxDocumentsChanged)

  public:
    enum Status { Loading, Ready, NeedsPassword, WrongPassword, Failed };
    Q_ENUM(Status)

    enum Roles {
        DocIdRole = Qt::UserRole + 1,
        PathRole,
        NameRole,
        FolderRole,
        PageCountRole,
        EncryptedRole,
        StatusRole,
        ErrorRole,
        WarningsRole,
        SizeRole,
        RangeRole,
    };

    struct Entry {
        pdfcore::DocId id = 0;
        QString path;
        QString password;
        int pageCount = 0;
        bool encrypted = false;
        Status status = Loading;
        QString error;
        QStringList warnings;
        qint64 size = 0;
        QString range; // per-document page range (merge)
    };

    explicit DocumentListModel(QObject* parent = nullptr);
    ~DocumentListModel() override;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds PDF files; anything else becomes a Failed row explaining why.
    Q_INVOKABLE void addUrls(const QList<QUrl>& urls);
    Q_INVOKABLE void unlock(int row, const QString& password);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void setRange(int row, const QString& range);
    // "" when `range` is valid for the document at `row`, otherwise a message for the user.
    Q_INVOKABLE QString rangeError(int row, const QString& range) const;
    Q_INVOKABLE QString path(int row) const;
    Q_INVOKABLE int pageCount(int row) const;

    [[nodiscard]] int readyCount() const;
    [[nodiscard]] bool allReady() const;
    [[nodiscard]] int maxDocuments() const { return m_maxDocuments; }
    void setMaxDocuments(int max);

    [[nodiscard]] const std::vector<Entry>& entries() const { return m_entries; }
    [[nodiscard]] const Entry* findById(pdfcore::DocId id) const;
    [[nodiscard]] pdfcore::SourceDoc source(int row) const;

  signals:
    void countChanged();
    void statusChanged();
    void maxDocumentsChanged();
    void documentReady(quint32 docId);
    void documentRemoved(quint32 docId);

  private:
    void inspect(pdfcore::DocId id, const QString& path, const QString& password);
    int rowOf(pdfcore::DocId id) const;
    void changed(int row);

    std::vector<Entry> m_entries;
    int m_maxDocuments = 0; // 0 = unlimited
};
