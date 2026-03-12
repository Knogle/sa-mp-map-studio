#pragma once

#include <QImage>
#include <QString>
#include <QVector>

struct TxdTexture {
    QString name;
    QImage image;
};

class TxdReader {
public:
    bool load(const QString& path, QString* errorMessage = nullptr);
    bool loadFromBytes(const QByteArray& bytes, const QString& sourceName, QString* errorMessage = nullptr);

    [[nodiscard]] const QVector<TxdTexture>& textures() const noexcept;
    [[nodiscard]] const QString& sourcePath() const noexcept;

private:
    QString sourcePath_;
    QVector<TxdTexture> textures_;
};
