#ifndef CANMONITOR_H
#define CANMONITOR_H

#include <QMainWindow>
#include <QCanBus>
#include <QCanBusDevice>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStatusBar>
#include <QGroupBox>
#include <QCheckBox>

QT_BEGIN_NAMESPACE
namespace Ui { class CANTrafficMonitor; }
QT_END_NAMESPACE

class CANMonitor : public QMainWindow
{
    Q_OBJECT

public:
    explicit CANMonitor(QWidget *parent = nullptr);
    ~CANMonitor();

private slots:
    void connectToCAN();
    void disconnectFromCAN();
    void sendFrame();
    void framesReceived();
    void errorOccurred(QCanBusDevice::CanBusError error);
    void clearLog();
    void updateConnectionStatus();

private:
    void setupInterfaces();
    void appendMessage(const QString &msg, bool isError = false);

    Ui::CANTrafficMonitor *ui;
    QCanBusDevice *canDevice;
};

#endif // CANMONITOR_H