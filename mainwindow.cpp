#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "camerahandler.h"
#include <iostream>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->buttonStart, &QPushButton::clicked, this, &MainWindow::startCapturing);
    connect(ui->buttonStop, SIGNAL(clicked()), SLOT(stopCapturing()));
    ui->label->setText(m_camera.deviceName());
}


MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::paintEvent(QPaintEvent* event) {
    QPainter widgetPainter(this);
    if (!m_frame.isNull())
        widgetPainter.drawImage(0, 0, m_frame);
    QMainWindow::paintEvent(event);
}

void MainWindow::timerEvent(QTimerEvent*) {
    m_frame = m_camera.getFrame();
    update();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    m_camera.setSize(event->size());
}

void MainWindow::captureFrame() {
    m_frame = m_camera.getFrame();
}

void MainWindow::startCapturing() {
    m_timerId = startTimer(50);
}

void MainWindow::stopCapturing() {
    killTimer(m_timerId);
}
