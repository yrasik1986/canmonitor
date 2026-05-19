#ifndef DBC_PARSER_H
#define DBC_PARSER_H

#include <QObject>
#include <QMap>
#include <QVariant>
#include <QCanBusFrame>

struct CANSignal {
    QString name;
    int startBit;
    int bitLength;
    bool isSigned;
    double factor;
    double offset;
    double minValue;
    double maxValue;
    QString unit;
    bool isLittleEndian;
};

struct CANMessage {
    uint id;
    QString name;
    int dlc;
    QMap<int, CANSignal> signalMap; // ИЗМЕНЕНО: signals -> signalMap (избегаем конфликта с макросом Qt)
};

class DBCParser : public QObject
{
    Q_OBJECT

public:
    explicit DBCParser(QObject *parent = nullptr);

    bool loadFromFile(const QString &filePath);
    QMap<QString, double> parseFrame(const QCanBusFrame &frame);
    QString getLastError() const { return m_lastError; }

    // Получить список всех сигналов
    QStringList getSignalNames() const;

    // Получить единицы измерения сигнала
    QString getSignalUnit(const QString &signalName) const;

private:
    bool parseLine(const QString &line);
    double extractSignal(const QByteArray &data, const CANSignal &signal);

    QMap<uint, CANMessage> m_messages;
    QMap<QString, CANSignal> m_signalsByName; // Для быстрого доступа
    QString m_lastError;
};

#endif // DBC_PARSER_H