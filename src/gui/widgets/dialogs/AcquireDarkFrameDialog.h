#ifndef ACQUIREDARKFRAMEDIALOG_H
#define ACQUIREDARKFRAMEDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QFormLayout>
#include <QHBoxLayout>

class AcquireDarkFrameDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AcquireDarkFrameDialog(QWidget *parent = nullptr);
    ~AcquireDarkFrameDialog() override;

    int frameCount() const;
    void setFrameCount(int n);

    void setAcquireInProgress(bool busy);

signals:
    void startRequested(int frameCount);

private slots:
    void onStartClicked();

private:
    QSpinBox *m_frameCountSpinBox;
    QPushButton *m_startButton;
    QPushButton *m_closeButton;
    bool m_acquireInProgress;
};

#endif // ACQUIREDARKFRAMEDIALOG_H
