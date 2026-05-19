#ifndef CANCHARTWIDGET_H
#define CANCHARTWIDGET_H

#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QMap>
#include <QVector>

// Используем стандартные классы без макросов

class CANChartWidget : public QChartView
{
    Q_OBJECT

public:
    explicit CANChartWidget(QWidget *parent = nullptr);

    void addSignal(const QString &name, const QString &color = "");
    void updateValue(const QString &name, double value);
    void clearData();

    // Настройки
    void setHistorySize(int points) { m_historySize = points; }
    void setAutoRange(bool enabled) { m_autoRange = enabled; }

private:
    void setupAxis();
    void updateAxisRange();

    struct SignalSeries {
        QLineSeries *series;
        QVector<double> data;
        double minVal;
        double maxVal;
        double lastVal;
        QString color;
    };

    QMap<QString, SignalSeries> m_signals;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY;
    int m_historySize;
    int m_frameCounter;
    bool m_autoRange;
};

#endif // CANCHARTWIDGET_H