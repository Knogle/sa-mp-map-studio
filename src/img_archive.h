#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>

struct ImgArchiveEntry {
    QString name;
    quint32 sectorOffset = 0;
    quint32 sectorCount = 0;
};

class ImgArchive {
public:
    bool open(const QString& path, QString* errorMessage = nullptr);

    [[nodiscard]] bool hasEntry(const QString& name) const;
    [[nodiscard]] QByteArray readEntry(const QString& name, QString* errorMessage = nullptr) const;
    [[nodiscard]] const QString& sourcePath() const noexcept;

private:
    static QString normalizeName(const QString& name);

    QString sourcePath_;
    QHash<QString, ImgArchiveEntry> entries_;
};
