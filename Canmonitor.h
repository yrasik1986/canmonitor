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

class CANMonitor : public QMainWindow
{
    Q_OBJECT

public:
    CANMonitor(QWidget *parent = nullptr);
    ~CANMonitor();

private slots:
    void connectToCAN();
    void disconnectFromCAN();
    void sendFrame();
    void framesReceived();
    void errorOccurred(QCanBusDevice::CanBusError error);

private:
    void setupUI();
    void appendMessage(const QString &msg, bool isError = false);

    QCanBusDevice *canDevice = nullptr;

    // UI Elements
    QComboBox *interfaceCombo;
    QLineEdit *bitrateEdit;
    QPushButton *connectBtn;
    QPushButton *disconnectBtn;
    QPushButton *sendBtn;
    QLineEdit *sendIdEdit;
    QLineEdit *sendDataEdit;
    QTextEdit *logArea;
    QStatusBar *statusBar;
};

#endif // CANMONITOR_H
