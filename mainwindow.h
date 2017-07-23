#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "camerahandler.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
//    void capture();
    ~MainWindow();

    void paintEvent(QPaintEvent *event);
private:
    CameraHandler m_camera;
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
