#include "ResultChartView.h"

#include <QLegendMarker>
#include <QVBoxLayout>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QValueAxis>

ResultChartView::ResultChartView(QWidget* parent) : QWidget(parent) {
    setupUi();
}

void ResultChartView::setupUi() {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    chart_ = new QChart;
    chart_->setTitle("SpMV Performance / 稀疏矩阵向量乘性能对比");
    chart_->setAnimationOptions(QChart::SeriesAnimations);
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignBottom);

    // X 轴: 矩阵行数 (对数)
    axisX_ = new QLogValueAxis;
    axisX_->setTitleText("矩阵行数 Rows");
    axisX_->setLabelFormat("%.0e");
    axisX_->setBase(10);
    chart_->addAxis(axisX_, Qt::AlignBottom);

    // Y 轴: GFLOPS
    axisY_ = new QValueAxis;
    axisY_->setTitleText("吞吐量 Throughput (GFLOPS)");
    axisY_->setLabelFormat("%.1f");
    axisY_->setRange(0, 10);
    chart_->addAxis(axisY_, Qt::AlignLeft);

    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView_);

    setMinimumWidth(400);
}

// 后端名 → 图表显示名
static QString displayName(const QString& key) {
    if (key == "baseline") return "CSR-SpMV baseline (CPU)";
    if (key == "openmp")   return "CSR-SpMV OpenMP";
    if (key == "simd")     return "CSR-SpMV SIMD";
    if (key == "cuda")     return "CSR-SpMV CUDA";
    if (key == "cusparse") return "CSR-SpMV cuSPARSE (工业级)";
    return key;
}

void ResultChartView::addResult(const QString& backend, int rows, double gflops) {
    std::string key = backend.toStdString();

    auto it = seriesMap_.find(key);
    QLineSeries* series = nullptr;

    if (it == seriesMap_.end()) {
        series = new QLineSeries;
        series->setName(displayName(backend));
        series->setMarkerSize(8);
        series->setPointsVisible(true);

        chart_->addSeries(series);
        series->attachAxis(axisX_);
        series->attachAxis(axisY_);

        seriesMap_[key] = series;
    } else {
        series = it->second;
    }

    series->append(rows, gflops);

    // 自动调整 Y 轴
    double maxY = 1.0;
    for (auto& [_, s] : seriesMap_) {
        for (int i = 0; i < s->count(); ++i)
            maxY = std::max(maxY, s->at(i).y());
    }
    axisY_->setRange(0, maxY * 1.2);

    // 自动调整 X 轴
    double minX = 1e9, maxX = 1;
    for (auto& [_, s] : seriesMap_) {
        for (int i = 0; i < s->count(); ++i) {
            minX = std::min(minX, s->at(i).x());
            maxX = std::max(maxX, s->at(i).x());
        }
    }
    if (minX <= maxX) {
        axisX_->setRange(minX * 0.8, maxX * 1.2);
    }
}

void ResultChartView::clearAll() {
    for (auto& [_, series] : seriesMap_) {
        chart_->removeSeries(series);
        delete series;
    }
    seriesMap_.clear();
}
