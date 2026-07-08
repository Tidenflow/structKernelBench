#pragma once

#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QLogValueAxis>
#include <QScatterSeries>
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

    // backend -> rendered series/data points
    std::map<std::string, QLineSeries*> lineSeriesMap_;
    std::map<std::string, QScatterSeries*> pointSeriesMap_;
    std::map<std::string, std::map<double, double>> dataMap_;

    void updateAxes();
    void resetAxes();
    void refreshDataBounds();
    void setAxisRange(double minX, double maxX, double minY, double maxY, bool zoomed);
    void zoomAround(double factor, const QPoint& viewPos);
    void zoomToRect(const QRect& viewRect);
    void panPixels(const QPoint& deltaPixels);
    void updateRenderedSeries();
    void updatePointLabels();

    bool isZoomed_ = false;
    bool hasData_ = false;
    double fullMinX_ = 1.0;
    double fullMaxX_ = 10.0;
    double fullMinY_ = 0.0;
    double fullMaxY_ = 10.0;
};
