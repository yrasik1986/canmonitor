#include "canchartwidget.h"
#include <QtCharts/QChart>
#include <QtCharts/QValueAxis>
#include <QRandomGenerator>
#include <QDebug>

CANChartWidget::CANChartWidget(QWidget *parent)
    : QChartView(parent)
    , m_historySize(100)
    , m_frameCounter(0)
    , m_autoRange(true)
{
    QChart *chart = new QChart();
    chart->setTitle("CAN Signals Real-time Monitor");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    setChart(chart);
    setRenderHint(QPainter::Antialiasing);

    setupAxis();
}

void CANChartWidget::setupAxis()
{
    m_axisX = new QValueAxis();
    m_axisX->setTitleText("Frame Count");
    m_axisX->setRange(0, m_historySize);
    m_axisX->setLabelFormat("%d");

    m_axisY = new QValueAxis();
    m_axisY->setTitleText("Value");
    m_axisY->setRange(-10, 100);

    chart()->addAxis(m_axisX, Qt::AlignBottom);
    chart()->addAxis(m_axisY, Qt::AlignLeft);
}

void CANChartWidget::addSignal(const QString &name, const QString &color)
{
    if (m_signals.contains(name)) return;

    SignalSeries ss;
    ss.series = new QLineSeries();
    ss.series->setName(name);
    ss.data.reserve(m_historySize);
    ss.minVal = 0;
    ss.maxVal = 100;
    ss.lastVal = 0;

    // Генерируем случайный цвет, если не указан
    if (color.isEmpty()) {
        static QList<QColor> colors = {
            Qt::red, Qt::green, Qt::blue, Qt::cyan, Qt::magenta,
            Qt::yellow, Qt::darkRed, Qt::darkGreen, Qt::darkBlue
        };
        int idx = m_signals.size() % colors.size();
        ss.color = colors[idx].name();
    } else {
        ss.color = color;
    }

    ss.series->setColor(QColor(ss.color));
    ss.series->setPen(QPen(QColor(ss.color), 2));

    chart()->addSeries(ss.series);
    ss.series->attachAxis(m_axisX);
    ss.series->attachAxis(m_axisY);

    m_signals[name] = ss;
}

void CANChartWidget::updateValue(const QString &name, double value)
{
    if (!m_signals.contains(name)) return;

    SignalSeries &ss = m_signals[name];
    ss.data.append(value);
    ss.lastVal = value;

    // Обновляем min/max
    if (value < ss.minVal) ss.minVal = value;
    if (value > ss.maxVal) ss.maxVal = value;

    // Ограничиваем размер
    while (ss.data.size() > m_historySize) {
        double removed = ss.data.takeFirst();
        // Пересчитываем min/max (простой способ - пересчитать все)
        if (removed == ss.minVal || removed == ss.maxVal) {
            ss.minVal = *std::min_element(ss.data.begin(), ss.data.end());
            ss.maxVal = *std::max_element(ss.data.begin(), ss.data.end());
        }
    }

    // Обновляем серию
    ss.series->clear();
    for (int i = 0; i < ss.data.size(); ++i) {
        ss.series->append(i, ss.data[i]);
    }

    updateAxisRange();
}

void CANChartWidget::updateAxisRange()
{
    if (!m_autoRange) return;

    // Находим глобальные min/max по всем сигналам
    double globalMin = 0;
    double globalMax = 100;
    bool first = true;

    for (const SignalSeries &ss : m_signals) {
        if (!ss.data.isEmpty()) {
            if (first) {
                globalMin = ss.minVal;
                globalMax = ss.maxVal;
                first = false;
            } else {
                globalMin = qMin(globalMin, ss.minVal);
                globalMax = qMax(globalMax, ss.maxVal);
            }
        }
    }

    // Добавляем 10% запаса
    double range = globalMax - globalMin;
    if (range < 1.0) range = 1.0;
    globalMin = globalMin - range * 0.1;
    globalMax = globalMax + range * 0.1;

    m_axisY->setRange(globalMin, globalMax);
}

void CANChartWidget::clearData()
{
    for (auto &ss : m_signals) {
        ss.data.clear();
        ss.series->clear();
        ss.minVal = 0;
        ss.maxVal = 100;
        ss.lastVal = 0;
    }
    m_frameCounter = 0;
}