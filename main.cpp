#include <QApplication>
#include "Canmonitor.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CAN Monitor");
    app.setApplicationVersion("1.0");

    CANMonitor window;
    window.show();

    return app.exec();
}
