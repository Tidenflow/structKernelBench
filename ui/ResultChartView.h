#pragma once

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QLogValueAxis>
#include <QValueAxis>
#include <QWidget>

#include <map>
#include <string>

// ============================================================
// ResultChartView — 右侧图表面板
//
// 多后端性能对比折线图，X 对数坐标。
// ============================================================
class ResultChartView : public QWidget {
    Q_OBJECT

public:
    explicit ResultChartView(const QString& title = "",
                             const QString& xLabel = "",
                             const QString& yLabel = "",
                             QWidget* parent = nullptr);

    void addResult(const QString& backend, int x, double y);
    void clearAll();

private:
    void setupUi();

    QChart*      chart_;
    QChartView*  chartView_;
    QValueAxis*  axisY_;
    QLogValueAxis* axisX_;

    // backend -> series/data points
    std::map<std::string, QLineSeries*> seriesMap_;
    std::map<std::string, std::map<double, double>> dataMap_;

    void updateAxes();
};
