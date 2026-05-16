#include "Canmonitor.h"
#include <QMessageBox>
#include <QDateTime>
#include <QThread>
#include <QGroupBox>
#include <QTextCursor>
#include <QFile>
#include <QFile>
#include <QTextStream>
#include <QTextCursor>
#include <QDateTime>

CANMonitor::CANMonitor(QWidget *parent)
    : QMainWindow(parent), canDevice(nullptr)
{
    setupUI();
}

CANMonitor::~CANMonitor()
{
    if (canDevice && canDevice->state() == QCanBusDevice::ConnectedState) {
        canDevice->disconnectDevice();
    }
    delete canDevice;
}

void CANMonitor::setupUI()
{
    setWindowTitle("CAN Monitor");
    setMinimumSize(800, 600);

    // Central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // === Connection panel ===
    QGroupBox *connGroup = new QGroupBox("Connection");
    QHBoxLayout *connLayout = new QHBoxLayout();
    connGroup->setLayout(connLayout);

    QLabel *interfaceLabel = new QLabel("Interface:");
    interfaceCombo = new QComboBox();

#ifdef Q_OS_LINUX
    interfaceCombo->addItem("can0");
    interfaceCombo->addItem("can1");
    interfaceCombo->setCurrentText("can0");
#elif defined(Q_OS_MAC)
    interfaceCombo->addItem("usb0");
    interfaceCombo->addItem("usb1");
    interfaceCombo->setCurrentText("usb0");
#else
    interfaceCombo->addItem("can0");
    interfaceCombo->setCurrentText("can0");
#endif
    interfaceCombo->setEditable(true);

    QLabel *bitrateLabel = new QLabel("Bitrate (bps):");
    bitrateEdit = new QLineEdit("500000");

    connectBtn = new QPushButton("Connect");
    disconnectBtn = new QPushButton("Disconnect");
    disconnectBtn->setEnabled(false);

    connLayout->addWidget(interfaceLabel);
    connLayout->addWidget(interfaceCombo);
    connLayout->addWidget(bitrateLabel);
    connLayout->addWidget(bitrateEdit);
    connLayout->addWidget(connectBtn);
    connLayout->addWidget(disconnectBtn);
    connLayout->addStretch();

    mainLayout->addWidget(connGroup);

    // === Send panel ===
    QGroupBox *sendGroup = new QGroupBox("Send CAN Frame");
    QHBoxLayout *sendLayout = new QHBoxLayout();
    sendGroup->setLayout(sendLayout);

    QLabel *idLabel = new QLabel("CAN ID (hex):");
    sendIdEdit = new QLineEdit("123");
    sendIdEdit->setFixedWidth(80);

    QLabel *dataLabel = new QLabel("Data (hex bytes):");
    sendDataEdit = new QLineEdit("01 02 03 04");
    sendDataEdit->setMinimumWidth(250);

    sendBtn = new QPushButton("Send");
    sendBtn->setEnabled(false);

    sendLayout->addWidget(idLabel);
    sendLayout->addWidget(sendIdEdit);
    sendLayout->addWidget(dataLabel);
    sendLayout->addWidget(sendDataEdit);
    sendLayout->addWidget(sendBtn);
    sendLayout->addStretch();

    mainLayout->addWidget(sendGroup);

    // === Log area ===
    QGroupBox *logGroup = new QGroupBox("CAN Traffic");
    QVBoxLayout *logLayout = new QVBoxLayout();
    logGroup->setLayout(logLayout);

    logArea = new QTextEdit();
    logArea->setReadOnly(true);
    logArea->setFont(QFont("Monospace", 9));

    logLayout->addWidget(logArea);

    mainLayout->addWidget(logGroup);

    // === Status bar ===
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    statusBar->showMessage("Not connected");

    // === Connect signals ===
    connect(connectBtn, &QPushButton::clicked, this, &CANMonitor::connectToCAN);
    connect(disconnectBtn, &QPushButton::clicked, this, &CANMonitor::disconnectFromCAN);
    connect(sendBtn, &QPushButton::clicked, this, &CANMonitor::sendFrame);
}

void CANMonitor::connectToCAN()
{
    QString interfaceName = interfaceCombo->currentText();
    bool ok;
    int bitrate = bitrateEdit->text().toInt(&ok);

    if (!ok || bitrate <= 0) {
        appendMessage("ERROR: Invalid bitrate", true);
        return;
    }

    QString pluginName;
    QString errorMsg;

#ifdef Q_OS_LINUX
    pluginName = "socketcan";
    errorMsg = "SocketCAN";
#elif defined(Q_OS_MAC)
    pluginName = "peakcan";
    errorMsg = "PEAK CAN";
#else
    pluginName = "socketcan";
    errorMsg = "SocketCAN";
#endif

    // Check if plugin is available
    if (!QCanBus::instance()->plugins().contains(pluginName)) {
        appendMessage(QString("ERROR: %1 plugin not found!").arg(errorMsg), true);
#ifdef Q_OS_LINUX
        appendMessage("On Ubuntu install: sudo apt install libqt5serialbus5-plugins", true);
#elif defined(Q_OS_MAC)
        appendMessage("On macOS install: https://github.com/mac-can/PCBUSB-Library", true);
#endif
        return;
    }

    // Create device
    canDevice = QCanBus::instance()->createDevice(pluginName, interfaceName);
    if (!canDevice) {
        appendMessage(QString("ERROR: Failed to create device for interface '%1'").arg(interfaceName), true);
        return;
    }

    // Configure bitrate
    canDevice->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrate);

    // Connect signals
    connect(canDevice, &QCanBusDevice::framesReceived, this, &CANMonitor::framesReceived);
    connect(canDevice, &QCanBusDevice::errorOccurred, this, &CANMonitor::errorOccurred);

    // Connect to device
    if (!canDevice->connectDevice()) {
        appendMessage(QString("ERROR: Failed to connect - %1").arg(canDevice->errorString()), true);
        delete canDevice;
        canDevice = nullptr;
        return;
    }

    // Success
    appendMessage(QString("Connected to %1 at %2 bps").arg(interfaceName).arg(bitrate));
    connectBtn->setEnabled(false);
    disconnectBtn->setEnabled(true);
    sendBtn->setEnabled(true);
    statusBar->showMessage(QString("Connected to %1").arg(interfaceName));
}

void CANMonitor::disconnectFromCAN()
{
    if (canDevice && canDevice->state() == QCanBusDevice::ConnectedState) {
        canDevice->disconnectDevice();
        appendMessage("Disconnected");
    }

    delete canDevice;
    canDevice = nullptr;

    connectBtn->setEnabled(true);
    disconnectBtn->setEnabled(false);
    sendBtn->setEnabled(false);
    statusBar->showMessage("Disconnected");
}

void CANMonitor::sendFrame()
{
    if (!canDevice || canDevice->state() != QCanBusDevice::ConnectedState) {
        appendMessage("ERROR: Not connected", true);
        return;
    }

    // Parse CAN ID
    bool ok;
    uint id = sendIdEdit->text().toUInt(&ok, 16);
    if (!ok) {
        appendMessage("ERROR: Invalid CAN ID format (use hex, e.g., 123)", true);
        return;
    }

    // Parse data bytes
    QString dataStr = sendDataEdit->text();
    QStringList byteStrings;

    // Работает и в Qt 5, и в Qt 6
#if QT_VERSION >= 0x060000
    byteStrings = dataStr.split(' ', Qt::SkipEmptyParts);
#else
    byteStrings = dataStr.split(' ', QString::SkipEmptyParts);
#endif

    QByteArray data;

    for (const QString &byteStr : byteStrings) {
        bool byteOk;
        uchar byte = static_cast<uchar>(byteStr.toUInt(&byteOk, 16));
        if (!byteOk) {
            appendMessage(QString("ERROR: Invalid data byte format '%1' (use hex, e.g., 01 02)").arg(byteStr), true);
            return;
        }
        data.append(byte);
    }

    if (data.size() > 8) {
        appendMessage("ERROR: CAN frame supports max 8 bytes of data", true);
        return;
    }

    // Create and send frame
    QCanBusFrame frame(id, data);
    frame.setExtendedFrameFormat(false);

    if (!canDevice->writeFrame(frame)) {
        appendMessage(QString("ERROR: Failed to send frame - %1").arg(canDevice->errorString()), true);
    } else {
        QString hexData = data.toHex(' ').toUpper();
        appendMessage(QString("[TX] ID: 0x%1  Data: %2").arg(id, 0, 16).arg(hexData));
    }
}

void CANMonitor::framesReceived()
{
    while (canDevice && canDevice->framesAvailable()) {
        QCanBusFrame frame = canDevice->readFrame();

        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

        QString idStr;
        if (frame.hasExtendedFrameFormat()) {
            idStr = QString("0x%1 (29-bit)").arg(frame.frameId(), 0, 16);
        } else {
            idStr = QString("0x%1").arg(frame.frameId(), 3, 16, QLatin1Char('0')).toUpper();
        }

        QByteArray data = frame.payload();
        QString dataStr;
        if (data.isEmpty()) {
            dataStr = "(RTR frame)";
        } else {
            dataStr = data.toHex(' ').toUpper();
        }

        if (frame.frameType() == QCanBusFrame::ErrorFrame) {
            appendMessage(QString("[ERR] %1 - Error frame").arg(timestamp), true);
        } else {
            appendMessage(QString("[RX] %1  ID: %2  [%3] %4")
                          .arg(timestamp)
                          .arg(idStr)
                          .arg(data.size())
                          .arg(dataStr));
        }
    }
}

void CANMonitor::errorOccurred(QCanBusDevice::CanBusError error)
{
    if (error == QCanBusDevice::ReadError || error == QCanBusDevice::WriteError) {
        appendMessage(QString("CAN BUS ERROR: %1").arg(canDevice ? canDevice->errorString() : "Unknown error"), true);
    } else if (error == QCanBusDevice::ConnectionError) {
        appendMessage(QString("CONNECTION ERROR: %1").arg(canDevice ? canDevice->errorString() : "Unknown error"), true);
        disconnectFromCAN();
    }
}

void CANMonitor::appendMessage(const QString &msg, bool isError)
{
    // Время с миллисекундами
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString formattedMsg = QString("[%1] %2").arg(timestamp).arg(msg);

    // Вывод в лог-поле (цвет для ошибок)
    if (isError) {
        logArea->append(QString("<font color='red'>%1</font>").arg(formattedMsg.toHtmlEscaped()));
    } else {
        logArea->append(formattedMsg);
    }

    // Автопрокрутка вниз
    logArea->moveCursor(QTextCursor::End);

    // // Логирование в файл (Qt6-style)
    // QFile logFile("can_log.txt");
    // if (logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    // {
    //     QTextStream out(&logFile);
    //     out << formattedMsg << "\n";
    //     logFile.close();
    // }
}
