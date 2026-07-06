#include "ResultChartView.h"

#include <QLegendMarker>
#include <QVBoxLayout>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QValueAxis>

ResultChartView::ResultChartView(const QString& title,
                                 const QString& xLabel,
                                 const QString& yLabel,
                                 QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    chart_ = new QChart;
    chart_->setTitle(title.isEmpty() ? "Performance" : title);
    chart_->setAnimationOptions(QChart::SeriesAnimations);
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignBottom);

    axisX_ = new QLogValueAxis;
    axisX_->setTitleText(xLabel.isEmpty() ? "Size" : xLabel);
    axisX_->setLabelFormat("%.0e");
    axisX_->setBase(10);
    chart_->addAxis(axisX_, Qt::AlignBottom);

    axisY_ = new QValueAxis;
    axisY_->setTitleText(yLabel.isEmpty() ? "Throughput" : yLabel);
    axisY_->setLabelFormat("%.1f");
    axisY_->setRange(0, 10);
    chart_->addAxis(axisY_, Qt::AlignLeft);

    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    layout->addWidget(chartView_);

    setMinimumWidth(400);
}

void ResultChartView::addResult(const QString& backend, int x, double y) {
    std::string key = backend.toStdString();

    auto it = seriesMap_.find(key);
    QLineSeries* series = nullptr;

    if (it == seriesMap_.end()) {
        series = new QLineSeries;
        series->setName(backend);
        series->setMarkerSize(8);
        series->setPointsVisible(true);

        chart_->addSeries(series);
        series->attachAxis(axisX_);
        series->attachAxis(axisY_);

        seriesMap_[key] = series;
    } else {
        series = it->second;
    }

    series->append(x, y);

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
