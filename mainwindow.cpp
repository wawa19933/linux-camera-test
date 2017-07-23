#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "camerahandler.h"
#include <iostream>
#include <QLabel>
#include <QPainter>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
//    CameraHandler cam;
    //    qDebug("Camera: %s", qPrintable(cam.deviceName()));
}


MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent* event) {
    QPainter widgetPainter(this);
    widgetPainter.drawImage(0, 0, m_camera.readFrame().toImage());
}
