#include "widget.h"

#include <QApplication>

// 饼图控件独立演示程序入口
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    w.show();
    return a.exec();
}
