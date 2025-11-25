#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    instance = this;
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setLabel2Visible(bool on)
{
    ui->label_2->setVisible(on);
}

void MainWindow::setLabel3Visible(bool on)
{
    ui->label_3->setVisible(on);
}

void MainWindow::showSpeed(int speed)
{
    ui->lcdNumber->display(speed);
}
