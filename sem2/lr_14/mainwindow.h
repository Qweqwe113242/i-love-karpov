#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "pixel.h"
#include "point.h"
#include "line.h"
#include "color.h"
#include "rectangle.h"
#include "ellipse.h"
#include "curve.h"
#include "polygon.h"
#include "arc.h"

#include "tooldialog.h"
#include "curvedialog.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_pushButtonClear_clicked();

    void on_pushButtonDraw_clicked();

private:
    Ui::MainWindow *ui;

};
#endif // MAINWINDOW_H
