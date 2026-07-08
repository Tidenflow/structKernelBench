#include "ResultChartView.h"

#include <QColor>
#include <QFont>
#include <QList>
#include <QMargins>
#include <QPointF>
#include <QLegendMarker>
#include <QPen>
#include <QVBoxLayout>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <array>

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
    chart_->setAnimationOptions(QChart::NoAnimation);
    chart_->setBackgroundVisible(false);
    chart_->setDropShadowEnabled(false);
    chart_->setMargins(QMargins(8, 8, 8, 4));
    chart_->setPlotAreaBackgroundVisible(true);
    chart_->setPlotAreaBackgroundBrush(QColor("#ffffff"));
    chart_->legend()->setVisible(true);
    chart_->legend()->setAlignment(Qt::AlignBottom);
    chart_->legend()->setBackgroundVisible(false);
    chart_->legend()->setBorderColor(Qt::transparent);

    QFont titleFont;
    titleFont.setPointSize(10);
    titleFont.setWeight(QFont::Medium);
    chart_->setTitleFont(titleFont);

    QFont axisFont;
    axisFont.setPointSize(8);

    QPen gridPen(QColor("#e1e5ea"));
    gridPen.setWidthF(1.0);

    QPen axisPen(QColor("#9aa4af"));
    axisPen.setWidthF(1.0);

    axisX_ = new QLogValueAxis;
    axisX_->setTitleText(xLabel.isEmpty() ? "Size" : xLabel);
    axisX_->setLabelFormat("%.0e");
    axisX_->setBase(10);
    axisX_->setLabelsFont(axisFont);
    axisX_->setTitleFont(axisFont);
    axisX_->setGridLinePen(gridPen);
    axisX_->setLinePen(axisPen);
    chart_->addAxis(axisX_, Qt::AlignBottom);

    axisY_ = new QValueAxis;
    axisY_->setTitleText(yLabel.isEmpty() ? "Throughput" : yLabel);
    axisY_->setLabelFormat("%.1f");
    axisY_->setRange(0, 10);
    axisY_->setTickCount(6);
    axisY_->setLabelsFont(axisFont);
    axisY_->setTitleFont(axisFont);
    axisY_->setGridLinePen(gridPen);
    axisY_->setLinePen(axisPen);
    chart_->addAxis(axisY_, Qt::AlignLeft);

    chartView_ = new QChartView(chart_);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setBackgroundBrush(QColor("#f3f5f7"));
    chartView_->setFrameShape(QFrame::NoFrame);
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
        series->setMarkerSize(7);
        series->setPointsVisible(true);

        static constexpr std::array<const char*, 8> kSeriesColors = {
            "#315f87", "#7a5c2e", "#5d7f3a", "#8a4f55",
            "#4f6f75", "#6c5d8d", "#8a6f38", "#55707f"
        };
        QPen seriesPen(QColor(kSeriesColors[seriesMap_.size() % kSeriesColors.size()]));
        seriesPen.setWidthF(2.0);
        series->setPen(seriesPen);

        chart_->addSeries(series);
        series->attachAxis(axisX_);
        series->attachAxis(axisY_);

        seriesMap_[key] = series;
    } else {
        series = it->second;
    }

    auto& pointsByX = dataMap_[key];
    pointsByX[static_cast<double>(x)] = y;

    QList<QPointF> sortedPoints;
    sortedPoints.reserve(static_cast<qsizetype>(pointsByX.size()));
    for (const auto& [px, py] : pointsByX) {
        sortedPoints.append(QPointF(px, py));
    }
    series->replace(sortedPoints);

    updateAxes();
}

void ResultChartView::clearAll() {
    for (auto& [_, series] : seriesMap_) {
        chart_->removeSeries(series);
        delete series;
    }
    seriesMap_.clear();
    dataMap_.clear();
    axisX_->setRange(1, 10);
    axisY_->setRange(0, 10);
}

void ResultChartView::updateAxes() {
    bool hasData = false;
    double minX = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;

    for (const auto& [_, pointsByX] : dataMap_) {
        for (const auto& [x, y] : pointsByX) {
            if (!hasData) {
                minX = maxX = x;
                maxY = y;
                hasData = true;
            } else {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                maxY = std::max(maxY, y);
            }
        }
    }

    if (!hasData) {
        axisX_->setRange(1, 10);
        axisY_->setRange(0, 10);
        return;
    }

    axisY_->setRange(0, std::max(1.0, maxY * 1.18));
    axisX_->setRange(std::max(1.0, minX * 0.85), maxX * 1.15);
}
