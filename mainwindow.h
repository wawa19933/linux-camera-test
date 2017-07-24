#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "camerahandler.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void paintEvent(QPaintEvent *event);
    void timerEvent(QTimerEvent *);
    void resizeEvent(QResizeEvent *event);
public slots:
    void captureFrame ();
    void startCapturing ();
    void stopCapturing ();

private:
    CameraHandler m_camera;
    QImage m_frame;
    int m_timerId {};
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
