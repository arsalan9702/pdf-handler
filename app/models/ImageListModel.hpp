#pragma once

#include <QAbstractListModel>
#include <QUrl>
#include <QtQml/qqmlregistration.h>

// Ordered image files for "Images to PDF".
class ImageListModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(ImageList)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

  public:
    enum Roles { PathRole = Qt::UserRole + 1, NameRole, UrlRole, SizeRole };

    explicit ImageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    // Adds supported image files; returns how many were skipped as unsupported.
    Q_INVOKABLE int addUrls(const QList<QUrl>& urls);
    Q_INVOKABLE void remove(int row);
    Q_INVOKABLE void move(int from, int to);
    Q_INVOKABLE void clear();
    Q_INVOKABLE QString path(int row) const;

    [[nodiscard]] const QStringList& paths() const { return m_paths; }

  signals:
    void countChanged();

  private:
    QStringList m_paths;
};
