#include "canmonitor.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("CAN Bus Monitor");
    app.setApplicationVersion("1.1");
    app.setOrganizationName("CANMonitor");

    CANMonitor window;
    window.show();

    return app.exec();
}