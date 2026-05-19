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
#include <QSplitter>
#include <QTabWidget>

#include "dbc_parser.h"
#include "canchartwidget.h"

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
    void loadDbcFile();
    void updateDecodedSignals(const QMap<QString, double> &dsignals);

private:
    void setupInterfaces();
    void setupDbcPanel();
    void setupChartPanel();
    void appendMessage(const QString &msg, bool isError = false);
    void appendDecodedMessage(const QMap<QString, double> &dsignals);

    Ui::CANTrafficMonitor *ui;
    QCanBusDevice *canDevice;
    DBCParser *m_dbcParser;
    CANChartWidget *m_chartWidget;
    QTextEdit *m_decodedLog;
    QMap<QString, QLabel*> m_signalLabels;
    bool m_dbcLoaded;
};

#endif // CANMONITOR_H