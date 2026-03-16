#include "MainWindow.h"

#include <QApplication>
#include <QDebug>
#include <QStyleFactory>
#include <QSurfaceFormat>
int main(int argc, char *argv[])
{
    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication a(argc, argv);
    // 验证实际使用的平台
    qDebug() << "Platform name:" << QGuiApplication::platformName();
    a.setStyle(QStyleFactory::create("fusion"));
    MainWindow w;
    w.show();
    return a.exec();
}
