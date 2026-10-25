#ifndef PARAMETERHISTORYWINDOW_H
#define PARAMETERHISTORYWINDOW_H

#include <QDialog>
#include <QString>
#include <QVector>

#include "../config/ParameterHistoryStore.h"

class QLabel;
class QPushButton;
class QSpinBox;
class LineViewWidget;

//! 单个遥测参数的曲线窗口：显示滑动窗口里的历史值，可改窗口长度、清空、导出 CSV。
//! 窗口是 CameraTab 的子对象，每轮遥测轮询后由 CameraTab 调 refresh() 更新。
class ParameterHistoryWindow : public QDialog
{
    Q_OBJECT
public:
    ParameterHistoryWindow(const QString &parameterName,
                           const ParameterDefinition &definition,
                           ParameterHistoryStore *store,
                           QWidget *parent = nullptr);
    ~ParameterHistoryWindow() override;

    QString parameterName() const { return m_parameterName; }
    LineViewWidget *plotWidget() const { return m_plot; }

    //! 把当前保留的样本写成 CSV；失败时把原因写到 error。
    //! 选路径的对话框在 onExportClicked() 里，这个方法单独留下来方便测试
    bool exportCsv(const QString &filePath, QString *error = nullptr) const;

public slots:
    void refresh();

private:
    void setupUi();
    void syncCapacity(int points);
    void updateStatus(const QVector<ParameterSample> &samples);
    void onExportClicked();
    void onClearClicked();

    QString m_parameterName;
    ParameterDefinition m_definition;
    ParameterHistoryStore *m_store = nullptr;
    LineViewWidget *m_plot = nullptr;
    QSpinBox *m_pointsSpin = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_clearButton = nullptr;
    QPushButton *m_exportButton = nullptr;
};

#endif // PARAMETERHISTORYWINDOW_H
