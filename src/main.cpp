#include "mainwindow.h"
#include "capture.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    
    MainWindow window;
    window.show();
    
    return app.exec();
}
