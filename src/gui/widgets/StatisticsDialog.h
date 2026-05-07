#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QImage>
#include <QVector>

class StatisticsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit StatisticsDialog(QWidget *parent = nullptr);
    ~StatisticsDialog() override;

    void setImageData(const QImage &image);
    void setSpectrumData(const QVector<double> &data);

private:
    void calculateImageStatistics(const QImage &image);
    void calculateSpectrumStatistics(const QVector<double> &data);
    void updateDisplay();

    QLabel *m_minLabel;
    QLabel *m_maxLabel;
    QLabel *m_avgLabel;
    QLabel *m_stdDevLabel;
    QLabel *m_pixelCountLabel;
    QLabel *m_dataTypeLabel;
    QPushButton *m_closeButton;

    double m_min;
    double m_max;
    double m_avg;
    double m_stdDev;
    quint64 m_pixelCount;
    QString m_dataType;
};

#endif // STATISTICSDIALOG_H