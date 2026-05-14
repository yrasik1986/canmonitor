#include "Canmonitor.h"
#include <QMessageBox>
#include <QDateTime>
#include <QThread>
#include <QGroupBox>

CANMonitor::CANMonitor(QWidget *parent)
    : QMainWindow(parent)
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
    setWindowTitle("CAN Monitor - PEAK USB");
    setMinimumSize(800, 600);

    // Central widget
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // === Connection panel ===
    QGroupBox *connGroup = new QGroupBox("Connection");
    QHBoxLayout *connLayout = new QHBoxLayout(connGroup);

    QLabel *interfaceLabel = new QLabel("Interface:");
    interfaceCombo = new QComboBox();
    interfaceCombo->addItem("can0");
    interfaceCombo->addItem("can1");
    interfaceCombo->setEditable(true);
    interfaceCombo->setCurrentText("can0");

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
    QHBoxLayout *sendLayout = new QHBoxLayout(sendGroup);

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
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);

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

    // Check if SocketCAN plugin is available
    if (!QCanBus::instance()->plugins().contains("socketcan")) {
        appendMessage("ERROR: SocketCAN plugin not found! Install qtconnectivity5-dev or similar", true);
        return;
    }

    // Create device
    canDevice = QCanBus::instance()->createDevice("socketcan", interfaceName);
    if (!canDevice) {
        appendMessage(QString("ERROR: Failed to create device for interface '%1'").arg(interfaceName), true);
        return;
    }

    // Configure bitrate - FIXED: Use the correct API
    QVariant bitrateValue = bitrate;
    canDevice->setConfigurationParameter(QCanBusDevice::BitRateKey, bitrateValue);

    // Connect signals
    connect(canDevice, &QCanBusDevice::framesReceived, this, &CANMonitor::framesReceived);
    connect(canDevice, &QCanBusDevice::errorOccurred, this, &CANMonitor::errorOccurred);

    // Connect to the device
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
    QStringList byteStrings = dataStr.split(' ', Qt::SkipEmptyParts);
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

    // Create and send frame - FIXED: Use setExtendedFrameFormat instead of setExtendedIdFormat
    QCanBusFrame frame(id, data);
    frame.setExtendedFrameFormat(false); // Use standard 11-bit ID (change to true if need 29-bit)

    if (!canDevice->writeFrame(frame)) {
        appendMessage(QString("ERROR: Failed to send frame - %1").arg(canDevice->errorString()), true);
    } else {
        // Log sent frame
        QString hexData = data.toHex(' ').toUpper();
        appendMessage(QString("[TX] ID: 0x%1  Data: %2").arg(id, 0, 16).arg(hexData));
    }
}

void CANMonitor::framesReceived()
{
    while (canDevice && canDevice->framesAvailable()) {
        QCanBusFrame frame = canDevice->readFrame();

        // Format timestamp
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

        // Format CAN ID
        QString idStr;
        if (frame.hasExtendedFrameFormat()) {
            idStr = QString("0x%1 (29-bit)").arg(frame.frameId(), 0, 16);
        } else {
            idStr = QString("0x%1").arg(frame.frameId(), 3, 16, QLatin1Char('0')).toUpper();
        }

        // Format data
        QByteArray data = frame.payload();
        QString dataStr;
        if (data.isEmpty()) {
            dataStr = "(RTR frame)";
        } else {
            dataStr = data.toHex(' ').toUpper();
        }

        // Check for errors
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
        appendMessage(QString("CAN BUS ERROR: %1").arg(canDevice->errorString()), true);
    } else if (error == QCanBusDevice::ConnectionError) {
        appendMessage(QString("CONNECTION ERROR: %1").arg(canDevice->errorString()), true);
        disconnectFromCAN();
    }
}

void CANMonitor::appendMessage(const QString &msg, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString formattedMsg = QString("[%1] %2").arg(timestamp).arg(msg);

    if (isError) {
        logArea->append(QString("<font color='red'>%1</font>").arg(formattedMsg.toHtmlEscaped()));
    } else {
        logArea->append(formattedMsg);
    }

    // Auto-scroll to bottom
    QTextCursor cursor = logArea->textCursor();
    cursor.movePosition(QTextCursor::End);
    logArea->setTextCursor(cursor);
}
