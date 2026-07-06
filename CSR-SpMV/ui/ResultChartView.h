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
// 用 QChart 绘制多后端性能对比折线图:
//   X 轴: 矩阵行数 (对数坐标)
//   Y 轴: 吞吐量 GFLOPS
//   每条线 = 一个后端
// ============================================================
class ResultChartView : public QWidget {
    Q_OBJECT

public:
    explicit ResultChartView(QWidget* parent = nullptr);

    // 追加一个数据点
    void addResult(const QString& backend, int rows, double gflops);

    // 清空所有数据
    void clearAll();

private:
    void setupUi();

    QChart*      chart_;
    QChartView*  chartView_;
    QValueAxis*  axisY_;
    QLogValueAxis* axisX_;

    // backend → series
    std::map<std::string, QLineSeries*> seriesMap_;
};
