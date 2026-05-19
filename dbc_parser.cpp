#include "dbc_parser.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>

DBCParser::DBCParser(QObject *parent) : QObject(parent)
{
}

bool DBCParser::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_lastError = QString("Cannot open file: %1").arg(filePath);
        return false;
    }

    m_messages.clear();
    m_signalsByName.clear();

    QTextStream stream(&file);
    int lineNum = 0;

    while (!stream.atEnd()) {
        QString line = stream.readLine();
        lineNum++;

        // Пропускаем комментарии и пустые строки
        if (line.trimmed().isEmpty() || line.startsWith("VERSION") ||
            line.startsWith("NS_") || line.startsWith("BS_") ||
            line.startsWith("BU_") || line.startsWith("CM_") ||
            line.startsWith("BA_") || line.startsWith("VAL_")) {
            continue;
        }

        if (!parseLine(line)) {
            qDebug() << "Parse error at line" << lineNum << ":" << line;
            // Не возвращаем false, просто пропускаем проблемную строку
        }
    }

    qDebug() << "Loaded" << m_messages.size() << "CAN messages with"
             << m_signalsByName.size() << "signals";

    return !m_messages.isEmpty();
}

bool DBCParser::parseLine(const QString &line)
{
    // Формат BO_ для сообщения: BO_ 790 VehicleSpeed: 8 Vector__XXX
    QRegularExpression boRegex(R"(BO_\s+(\d+)\s+(\w+):\s+(\d+)\s+\w+)");
    QRegularExpressionMatch boMatch = boRegex.match(line);

    if (boMatch.hasMatch()) {
        CANMessage msg;
        msg.id = boMatch.captured(1).toUInt();
        msg.name = boMatch.captured(2);
        msg.dlc = boMatch.captured(3).toInt();
        m_messages[msg.id] = msg;
        return true;
    }

    // Формат SG_ для сигнала (Little-Endian версия)
    // SG_ Speed_kmh : 24|8@1+ (0.5,0) [0|250] "km/h" Vector__XXX
    QRegularExpression sgRegex(
        R"(SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@([01])([+-])\s+\(([-\d.]+),([-\d.]+)\)\s+\[([-\d.]+)\|([-\d.]+)\]\s+\"([^\"]*)\")"
        );

    QRegularExpressionMatch sgMatch = sgRegex.match(line);
    if (sgMatch.hasMatch()) {
        CANSignal signal;
        signal.name = sgMatch.captured(1);
        signal.startBit = sgMatch.captured(2).toInt();
        signal.bitLength = sgMatch.captured(3).toInt();
        bool isLittleEndian = sgMatch.captured(4).toInt() == 1;
        signal.isLittleEndian = isLittleEndian;
        signal.isSigned = (sgMatch.captured(5) == "-");
        signal.factor = sgMatch.captured(6).toDouble();
        signal.offset = sgMatch.captured(7).toDouble();
        signal.minValue = sgMatch.captured(8).toDouble();
        signal.maxValue = sgMatch.captured(9).toDouble();
        signal.unit = sgMatch.captured(10);

        // Находим ID сообщения (строка выше должна быть BO_)
        m_signalsByName[signal.name] = signal;

        // Добавляем сигнал в последнее сообщение
        if (!m_messages.isEmpty()) {
            auto lastMsgIt = m_messages.end();
            --lastMsgIt;
            lastMsgIt->signalMap[signal.startBit] = signal; // ИСПРАВЛЕНО: signals -> signalMap
        }

        return true;
    }

    return false;
}

double DBCParser::extractSignal(const QByteArray &data, const CANSignal &signal)
{
    if (data.isEmpty()) return 0.0;

    quint64 rawValue = 0;
    int byteIndex = signal.startBit / 8;
    int bitOffset = signal.startBit % 8;

    if (signal.isLittleEndian) {
        // Little-Endian: младшие байты сначала
        for (int i = 0; i < (signal.bitLength + 7) / 8 && (byteIndex + i) < data.size(); i++) {
            rawValue |= (static_cast<quint64>(static_cast<uchar>(data[byteIndex + i])) << (i * 8));
        }
        // Сдвигаем на битовое смещение
        rawValue >>= bitOffset;
    } else {
        // Big-Endian: старшие байты сначала
        for (int i = 0; i < (signal.bitLength + 7) / 8 && (byteIndex + i) < data.size(); i++) {
            rawValue = (rawValue << 8) | static_cast<uchar>(data[byteIndex + i]);
        }
        // Маскируем нужные биты
        if (bitOffset > 0) {
            rawValue >>= (8 - bitOffset);
        }
    }

    // Маскируем только нужные биты
    quint64 mask = (signal.bitLength == 64) ? ~0ULL : ((1ULL << signal.bitLength) - 1);
    rawValue &= mask;

    double physicalValue = rawValue * signal.factor + signal.offset;

    // Обработка знаковых значений (дополнительный код)
    if (signal.isSigned && (rawValue & (1ULL << (signal.bitLength - 1)))) {
        physicalValue = -(static_cast<double>(~rawValue & mask) + 1) * signal.factor + signal.offset;
    }

    return physicalValue;
}

QMap<QString, double> DBCParser::parseFrame(const QCanBusFrame &frame)
{
    QMap<QString, double> result;

    uint id = frame.frameId();
    if (!m_messages.contains(id)) {
        return result; // Неизвестный ID
    }

    const CANMessage &msg = m_messages[id];
    QByteArray data = frame.payload();

    for (const CANSignal &signal : msg.signalMap) { // ИСПРАВЛЕНО: signals -> signalMap
        if (signal.startBit / 8 < data.size()) {
            double value = extractSignal(data, signal);
            result[signal.name] = value;
        }
    }

    return result;
}

QStringList DBCParser::getSignalNames() const
{
    return m_signalsByName.keys();
}

QString DBCParser::getSignalUnit(const QString &signalName) const
{
    if (m_signalsByName.contains(signalName)) {
        return m_signalsByName[signalName].unit;
    }
    return QString();
}