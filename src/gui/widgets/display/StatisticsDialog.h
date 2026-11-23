#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>

#include "../../services/ImageStatistics.h"

class QLabel;
class QPushButton;

class StatisticsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StatisticsDialog(QWidget *parent = nullptr);
    ~StatisticsDialog() override;

    void setImageData(const QImage &image);
    void setSpectrumData(const QVector<double> &data);

private:
    void updateDisplay();

    QLabel *m_minLabel = nullptr;
    QLabel *m_maxLabel = nullptr;
    QLabel *m_avgLabel = nullptr;
    QLabel *m_stdDevLabel = nullptr;
    QLabel *m_pixelCountLabel = nullptr;
    QLabel *m_dataTypeLabel = nullptr;
    QPushButton *m_closeButton = nullptr;

    ImageStatistics m_statistics;
};

#endif // STATISTICSDIALOG_H
