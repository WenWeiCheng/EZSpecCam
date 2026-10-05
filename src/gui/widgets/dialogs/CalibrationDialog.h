#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include <QDialog>
#include <QCheckBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>

class CalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CalibrationDialog(QWidget *parent = nullptr);
    ~CalibrationDialog() override;

    void setDarkFrameEnabled(bool enabled);
    bool darkFrameEnabled() const;

    void setDarkFramePath(const QString &path);
    QString darkFramePath() const;

    // When true, the path field is left empty but its placeholder advertises
    // that an in-memory dark frame is the active source. When the user later
    // picks a file via Browse, darkFramePath() still returns the picked path,
    // so the in-memory marker never leaks into persisted state.
    void setInMemoryDarkFrameUsed(bool used, int frameCount = 0);

    void setCustomBias(int bias);
    int customBias() const;

    int frameCount() const;
    void setFrameCount(int n);

    // 采集中：按钮和帧数都锁住，防止把正在跑的帧数改掉
    void setAcquireInProgress(bool busy);
    // 相机没连上、或相机正在采集时整条采集入口不可用
    void setAcquireEnabled(bool enabled);

signals:
    void applied(bool enabled, const QString &filePath, int customBias);
    void acquireRequested(int frameCount);

private slots:
    void onBrowseClicked();
    void onOkClicked();
    void onAcquireClicked();

private:
    QCheckBox *m_enableCheckBox;
    QLineEdit *m_pathLineEdit;
    QPushButton *m_browseButton;
    QSpinBox *m_biasSpinBox;
    QSpinBox *m_frameCountSpinBox;
    QPushButton *m_acquireButton;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
    bool m_acquireInProgress;
    bool m_acquireEnabled;
};

#endif // CALIBRATIONDIALOG_H
