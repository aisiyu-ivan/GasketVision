#include "InspectionWindow.h"

#include <QApplication>

// 视觉质检 HMI 程序入口
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    InspectionWindow w;
    w.show();
    return app.exec();
}
