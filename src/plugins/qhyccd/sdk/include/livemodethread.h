#ifndef LIVEMODETHREAD_H
#define LIVEMODETHREAD_H

#include <QThread>
#include "camstruct.h"

class LiveModeThread : public QThread
{
    Q_OBJECT
public:
    explicit LiveModeThread(QObject *parent = 0);

signals:
    void gotImageData();
    void showFPS();
    void showFrame();

public:
    void run();

    unsigned long interval;
};

#endif // LIVEMODETHREAD_H
