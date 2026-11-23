#include <QApplication>
#include <QSurfaceFormat>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // Legacy (fixed pipeline) OpenGL is used for the 3D view
    QSurfaceFormat fmt;
    fmt.setVersion(2, 1);
    fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
    fmt.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication a(argc, argv);
    QString iface = (argc > 1) ? QString(argv[1]) : QString("can0");
    MainWindow w(iface);
    w.show();
    return a.exec();
}
