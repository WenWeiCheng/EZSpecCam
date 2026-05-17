#ifndef CSVFORMATHANDLER_H
#define CSVFORMATHANDLER_H

#include <QObject>
#include <QString>

#include "../IImageFormatHandler.h"
#include "../SaveTypes.h"

class CsvFormatHandler : public IImageFormatHandler
{
    Q_OBJECT

public:
    explicit CsvFormatHandler(QObject *parent = nullptr);
    ~CsvFormatHandler() override = default;

    bool save(const SaveRequest &request) override;
    bool canHandle(const QString &filePath) const override;
    QStringList supportedExtensions() const override;
    QString displayName() const override;

private:
    bool exportSpectrumCsv(const QVector<double> &spectrum, const QString &path);
    bool exportImageCsv(const QImage &img, const QString &path);
    QString insertSuffix(const QString &filePath, const QString &suffix) const;
    bool saveMetadataJson(const QString &imgPath, const SaveRequest &request);
};

#endif // CSVFORMATHANDLER_H
