#include "canmonitor.h"
#include "ui_canmonitor.h"
#include <QMessageBox>
#include <QDateTime>
#include <QTextCursor>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QSplitter>
#include <QTabWidget>
#include <QDebug>

CANMonitor::CANMonitor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::CANTrafficMonitor)
    , canDevice(nullptr)
    , m_dbcParser(new DBCParser(this))
    , m_chartWidget(nullptr)
    , m_decodedLog(nullptr)
    , m_dbcLoaded(false)
{
    ui->setupUi(this);

    setupInterfaces();

    setupDbcPanel();
    setupChartPanel();


    // Connect UI signals
    connect(ui->connectBtn, &QPushButton::clicked, this, &CANMonitor::connectToCAN);
    connect(ui->disconnectBtn, &QPushButton::clicked, this, &CANMonitor::disconnectFromCAN);
    connect(ui->sendBtn, &QPushButton::clicked, this, &CANMonitor::sendFrame);
    connect(ui->clearLogBtn, &QPushButton::clicked, this, &CANMonitor::clearLog);

    setWindowTitle("CAN Bus Monitor with DBC Support");
    statusBar()->showMessage("Ready - Not connected");
}

CANMonitor::~CANMonitor()
{
    if (canDevice && canDevice->state() == QCanBusDevice::ConnectedState) {
        canDevice->disconnectDevice();
    }
    delete canDevice;
    delete ui;
}

void CANMonitor::setupInterfaces()
{
#ifdef Q_OS_LINUX
    ui->interfaceCombo->addItem("can0");
    ui->interfaceCombo->addItem("can1");
    ui->interfaceCombo->addItem("vcan0");
    ui->interfaceCombo->setCurrentText("can0");
#elif defined(Q_OS_MAC)
    ui->interfaceCombo->addItem("usb0");
    ui->interfaceCombo->addItem("usb1");
    ui->interfaceCombo->setCurrentText("usb0");
#elif defined(Q_OS_WIN)
    ui->interfaceCombo->addItem("can0");
    ui->interfaceCombo->addItem("can1");
    ui->interfaceCombo->setCurrentText("can0");
#else
    ui->interfaceCombo->addItem("can0");
    ui->interfaceCombo->setCurrentText("can0");
#endif
}

void CANMonitor::setupDbcPanel()
{

    // Вкладка 1: Raw CAN Log (оригинальная)
    QWidget *rawTab = new QWidget();
    QVBoxLayout *rawLayout = new QVBoxLayout(rawTab);

    // Забираем существующий trafficGroup и перемещаем
    ui->trafficGroup->setParent(rawTab);
    rawLayout->addWidget(ui->trafficGroup);
    rawTab->setLayout(rawLayout);

    // Вкладка 2: Decoded Signals
    QWidget *decodedTab = new QWidget();
    QVBoxLayout *decodedLayout = new QVBoxLayout(decodedTab);

    m_decodedLog = new QTextEdit();
    m_decodedLog->setFont(QFont("Monospace", 9));
    m_decodedLog->setReadOnly(true);
    decodedLayout->addWidget(m_decodedLog);

    QHBoxLayout *dbcControls = new QHBoxLayout();
    QPushButton *loadDbcBtn = new QPushButton("Load DBC File");
    QPushButton *clearDecodedBtn = new QPushButton("Clear Decoded Log");
    QCheckBox *autoScrollDecoded = new QCheckBox("Auto-scroll");
    autoScrollDecoded->setChecked(true);

    connect(loadDbcBtn, &QPushButton::clicked, this, &CANMonitor::loadDbcFile);
    connect(clearDecodedBtn, &QPushButton::clicked, m_decodedLog, &QTextEdit::clear);
    connect(autoScrollDecoded, &QCheckBox::toggled, [this](bool checked) {
        // Используем существующий autoScrollCheck для decoded лога
        Q_UNUSED(checked);
    });

    dbcControls->addWidget(loadDbcBtn);
    dbcControls->addWidget(clearDecodedBtn);
    dbcControls->addWidget(autoScrollDecoded);
    dbcControls->addStretch();
    decodedLayout->addLayout(dbcControls);
    decodedTab->setLayout(decodedLayout);

    // Вкладка 3: Live Charts
    QWidget *chartTab = new QWidget();
    QVBoxLayout *chartLayout = new QVBoxLayout(chartTab);
    m_chartWidget = new CANChartWidget();
    m_chartWidget->setHistorySize(200);
    chartLayout->addWidget(m_chartWidget);

    QHBoxLayout *chartControls = new QHBoxLayout();
    QPushButton *clearChartBtn = new QPushButton("Clear Charts");
    QCheckBox *autoRangeCheck = new QCheckBox("Auto Range");
    autoRangeCheck->setChecked(true);

    connect(clearChartBtn, &QPushButton::clicked, m_chartWidget, &CANChartWidget::clearData);
    connect(autoRangeCheck, &QCheckBox::toggled, m_chartWidget, &CANChartWidget::setAutoRange);

    chartControls->addWidget(clearChartBtn);
    chartControls->addWidget(autoRangeCheck);
    chartControls->addStretch();
    chartLayout->addLayout(chartControls);
    chartTab->setLayout(chartLayout);

    // Добавляем вкладки
    ui->tabWidget->addTab(rawTab, "Raw CAN Log");
    ui->tabWidget->addTab(decodedTab, "Decoded Signals");
    ui->tabWidget->addTab(chartTab, "Live Charts");

}

void CANMonitor::setupChartPanel()
{
    // Добавляем сигналы для отображения на графиках
    m_chartWidget->addSignal("Speed_kmh", "#00FF00");
    m_chartWidget->addSignal("Temp_Celsius", "#FF4444");
    m_chartWidget->addSignal("Angle", "#4444FF");
}

void CANMonitor::loadDbcFile()
{
    QString fileName = QFileDialog::getOpenFileName(this,
                                                    "Select DBC File", "", "DBC Files (*.dbc)");

    if (fileName.isEmpty()) return;

    if (!m_dbcParser->loadFromFile(fileName)) {
        appendMessage("ERROR: " + m_dbcParser->getLastError(), true);
        QMessageBox::critical(this, "DBC Load Error", m_dbcParser->getLastError());
        return;
    }

    m_dbcLoaded = true;
    appendMessage("DBC file loaded: " + fileName);
    appendMessage(QString("Found %1 signals").arg(m_dbcParser->getSignalNames().size()));

    // Показываем список сигналов
    for (const QString &signal : m_dbcParser->getSignalNames()) {
        appendMessage(QString("  - %1 (%2)").arg(signal, m_dbcParser->getSignalUnit(signal)));
    }

    statusBar()->showMessage("DBC loaded: " + fileName);
}

void CANMonitor::connectToCAN()
{
    QString interfaceName = ui->interfaceCombo->currentText();
    bool ok;
    int bitrate = ui->bitrateEdit->text().toInt(&ok);

    if (!ok || bitrate <= 0) {
        appendMessage("ERROR: Invalid bitrate value", true);
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
#elif defined(Q_OS_WIN)
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
    ui->connectBtn->setEnabled(false);
    ui->disconnectBtn->setEnabled(true);
    ui->sendBtn->setEnabled(true);
    statusBar()->showMessage(QString("Connected to %1 at %2 bps").arg(interfaceName).arg(bitrate));
}

void CANMonitor::disconnectFromCAN()
{
    if (canDevice && canDevice->state() == QCanBusDevice::ConnectedState) {
        canDevice->disconnectDevice();
        appendMessage("Disconnected from CAN bus");
    }

    delete canDevice;
    canDevice = nullptr;

    ui->connectBtn->setEnabled(true);
    ui->disconnectBtn->setEnabled(false);
    ui->sendBtn->setEnabled(false);
    statusBar()->showMessage("Disconnected");
}

void CANMonitor::sendFrame()
{
    if (!canDevice || canDevice->state() != QCanBusDevice::ConnectedState) {
        appendMessage("ERROR: Not connected to CAN bus", true);
        return;
    }

    // Parse CAN ID
    bool ok;
    uint id = ui->sendIdEdit->text().toUInt(&ok, 16);
    if (!ok) {
        appendMessage("ERROR: Invalid CAN ID format (use hex, e.g., 123 or 7FF)", true);
        return;
    }

    if (id > 0x7FF) {
        appendMessage("WARNING: ID > 0x7FF will be sent as 29-bit extended frame");
    }

    // Parse data bytes
    QString dataStr = ui->sendDataEdit->text();
    QStringList byteStrings;

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
            appendMessage(QString("ERROR: Invalid data byte format '%1' (use hex, e.g., 01 02 03)").arg(byteStr), true);
            return;
        }
        data.append(byte);
    }

    if (data.size() > 8) {
        appendMessage("ERROR: CAN frame supports maximum 8 bytes of data", true);
        return;
    }

    // Create and send frame
    QCanBusFrame frame;
    if (id > 0x7FF) {
        frame.setExtendedFrameFormat(true);
    }
    frame.setFrameId(id);
    frame.setPayload(data);

    if (!canDevice->writeFrame(frame)) {
        appendMessage(QString("ERROR: Failed to send frame - %1").arg(canDevice->errorString()), true);
    } else {
        QString hexData = data.isEmpty() ? "(RTR)" : data.toHex(' ').toUpper();
        appendMessage(QString("[TX] ID: 0x%1 (DLC: %2) Data: %3")
                          .arg(id, 0, 16).toUpper()
                          .arg(data.size())
                          .arg(hexData));
    }
}

void CANMonitor::framesReceived()
{
    while (canDevice && canDevice->framesAvailable()) {
        QCanBusFrame frame = canDevice->readFrame();

        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");

        // Форматируем ID
        QString idStr;
        if (frame.hasExtendedFrameFormat()) {
            idStr = QString("0x%1 (29-bit)").arg(frame.frameId(), 0, 16);
        } else {
            idStr = QString("0x%1").arg(frame.frameId(), 3, 16, QLatin1Char('0')).toUpper();
        }

        // Данные
        QByteArray data = frame.payload();
        QString dataStr;
        if (frame.frameType() == QCanBusFrame::RemoteRequestFrame) {
            dataStr = "(RTR frame)";
        } else if (data.isEmpty()) {
            dataStr = "(Empty)";
        } else {
            dataStr = data.toHex(' ').toUpper();
        }

        // Вывод в raw лог
        QString frameType = (frame.frameType() == QCanBusFrame::ErrorFrame) ? " [ERROR]" : "";
        appendMessage(QString("[RX] %1  ID: %2  DLC: %3  Data: %4%5")
                          .arg(timestamp)
                          .arg(idStr)
                          .arg(data.size())
                          .arg(dataStr)
                          .arg(frameType),
                      frame.frameType() == QCanBusFrame::ErrorFrame);

        // Декодируем через DBC, если загружен
        if (m_dbcLoaded) {
            QMap<QString, double> dsignals = m_dbcParser->parseFrame(frame);
            if (!dsignals.isEmpty()) {
                updateDecodedSignals(dsignals);
                appendDecodedMessage(dsignals);
            }
        }
    }
}

void CANMonitor::updateDecodedSignals(const QMap<QString, double> &dsignals)
{
    // Обновляем графики
    for (auto it = dsignals.begin(); it != dsignals.end(); ++it) {
        m_chartWidget->updateValue(it.key(), it.value());
    }

    // Обновляем статус-бар с последним значением скорости
    if (dsignals.contains("Speed_kmh")) {
        statusBar()->showMessage(QString("Speed: %1 km/h | Connected")
                                     .arg(dsignals["Speed_kmh"], 0, 'f', 1));
    }
}

void CANMonitor::appendDecodedMessage(const QMap<QString, double> &dsignals)
{
    if (!m_decodedLog) return;

    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QStringList signalStrings;

    for (auto it = dsignals.begin(); it != dsignals.end(); ++it) {
        QString unit = m_dbcParser->getSignalUnit(it.key());
        QString unitStr = unit.isEmpty() ? "" : QString(" %1").arg(unit);
        signalStrings.append(QString("%1 = %2%3").arg(it.key()).arg(it.value(), 0, 'f', 2).arg(unitStr));
    }

    QString message = QString("[%1] %2").arg(timestamp).arg(signalStrings.join(" | "));
    m_decodedLog->append(message);

    // Авто-скролл
    if (ui->autoScrollCheck->isChecked()) {
        m_decodedLog->moveCursor(QTextCursor::End);
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

void CANMonitor::clearLog()
{
    ui->logArea->clear();
    appendMessage("Log cleared");
}

void CANMonitor::updateConnectionStatus()
{
    if (canDevice && canDevice->state() == QCanBusDevice::ConnectedState) {
        statusBar()->showMessage("Connected to CAN bus");
    } else {
        statusBar()->showMessage("Disconnected");
    }
}

void CANMonitor::appendMessage(const QString &msg, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    QString formattedMsg = QString("[%1] %2").arg(timestamp).arg(msg);

    if (isError) {
        ui->logArea->append(QString("<font color='#FF0000'>%1</font>").arg(formattedMsg.toHtmlEscaped()));
    } else {
        ui->logArea->append(formattedMsg);
    }

    // Auto-scroll if enabled
    if (ui->autoScrollCheck->isChecked()) {
        ui->logArea->moveCursor(QTextCursor::End);
    }
}