#include "ResultChartView.h"

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QList>
#include <QMargins>
#include <QMouseEvent>
#include <QPainter>
#include <QPointF>
#include <QLegendMarker>
#include <QPen>
#include <QRectF>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtCharts/QLineSeries>
#include <QtCharts/QLogValueAxis>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace {
class InteractiveChartView : public QChartView {
public:
    explicit InteractiveChartView(QChart* chart, QWidget* parent = nullptr)
        : QChartView(chart, parent) {}

    std::function<void(double, const QPoint&)> onWheelZoom;
    std::function<void(const QRect&)> onRectZoom;
    std::function<void(const QPoint&)> onPan;
    std::function<void()> onReset;

protected:
    void wheelEvent(QWheelEvent* event) override {
        const int delta = event->angleDelta().y();
        if (delta == 0) {
            QChartView::wheelEvent(event);
            return;
        }

        if (onWheelZoom) {
            onWheelZoom(delta > 0 ? 1.0 / 1.25 : 1.25, event->position().toPoint());
        }
        event->accept();
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onReset) {
            onReset();
            event->accept();
            return;
        }
        QChartView::mouseDoubleClickEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton) {
            panning_ = true;
            lastPanPos_ = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            pressPos_ = event->pos();
            rubberBand_ = QRect(pressPos_, QSize());
            dragging_ = true;
            event->accept();
            return;
        }
        QChartView::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        if (panning_) {
            const QPoint delta = event->pos() - lastPanPos_;
            lastPanPos_ = event->pos();
            if (!delta.isNull() && onPan) {
                onPan(delta);
            }
            event->accept();
            return;
        }
        if (dragging_) {
            rubberBand_ = QRect(pressPos_, event->pos()).normalized();
            viewport()->update();
            event->accept();
            return;
        }
        QChartView::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() == Qt::MiddleButton && panning_) {
            panning_ = false;
            unsetCursor();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton && dragging_) {
            const QRect selected = QRect(pressPos_, event->pos()).normalized();
            dragging_ = false;
            rubberBand_ = QRect();
            viewport()->update();

            if (selected.width() >= 12 && selected.height() >= 12 && onRectZoom) {
                onRectZoom(selected);
            }
            event->accept();
            return;
        }
        QChartView::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent* event) override {
        QChartView::paintEvent(event);
        if (!dragging_ || rubberBand_.width() < 2 || rubberBand_.height() < 2) {
            return;
        }

        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(QPen(QColor("#385b8f"), 1));
        painter.setBrush(QColor(90, 120, 190, 45));
        painter.drawRect(rubberBand_);
    }

private:
    QPoint pressPos_;
    QPoint lastPanPos_;
    QRect rubberBand_;
    bool dragging_ = false;
    bool panning_ = false;
};

QToolButton* makeChartButton(const QString& text, const QString& tooltip) {
    auto* button = new QToolButton;
    button->setText(text);
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedSize(28, 24);
    return button;
}
}  // namespace

ResultChartView::ResultChartView(const QString& title,
                                 const QString& xLabel,
                                 const QString& yLabel,
                                 QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

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

    auto* toolbar = new QHBoxLayout;
    toolbar->setContentsMargins(0, 0, 0, 0);
    toolbar->setSpacing(4);
    toolbar->addStretch();

    auto* btnZoomIn = makeChartButton("+", "放大图表");
    auto* btnZoomOut = makeChartButton("-", "缩小图表");
    auto* btnReset = makeChartButton("1:1", "恢复完整范围");
    toolbar->addWidget(btnZoomIn);
    toolbar->addWidget(btnZoomOut);
    toolbar->addWidget(btnReset);
    layout->addLayout(toolbar);

    auto* interactiveView = new InteractiveChartView(chart_);
    interactiveView->onWheelZoom = [this](double factor, const QPoint& viewPos) {
        zoomAround(factor, viewPos);
    };
    interactiveView->onRectZoom = [this](const QRect& viewRect) {
        zoomToRect(viewRect);
    };
    interactiveView->onPan = [this](const QPoint& deltaPixels) {
        panPixels(deltaPixels);
    };
    interactiveView->onReset = [this]() {
        resetAxes();
    };

    chartView_ = interactiveView;
    chartView_->setRubberBand(QChartView::NoRubberBand);
    chartView_->setRenderHint(QPainter::Antialiasing);
    chartView_->setBackgroundBrush(QColor("#f3f5f7"));
    chartView_->setFrameShape(QFrame::NoFrame);
    layout->addWidget(chartView_);

    setStyleSheet(R"(
        QToolButton {
            background-color: #f8f9fa;
            border: 1px solid #c9ced6;
            border-radius: 2px;
            color: #30363d;
            font-size: 12px;
            font-weight: 600;
        }
        QToolButton:hover {
            background-color: #eef1f4;
        }
        QToolButton:pressed {
            background-color: #e3e7eb;
        }
    )");

    connect(btnZoomIn, &QToolButton::clicked, this, [this]() {
        zoomAround(1.0 / 1.25, chart_->plotArea().center().toPoint());
    });
    connect(btnZoomOut, &QToolButton::clicked, this, [this]() {
        zoomAround(1.25, chart_->plotArea().center().toPoint());
    });
    connect(btnReset, &QToolButton::clicked, this, [this]() {
        resetAxes();
    });

    setMinimumWidth(400);
}

void ResultChartView::addResult(const QString& backend, int x, double y) {
    std::string key = backend.toStdString();

    auto lineIt = lineSeriesMap_.find(key);
    if (lineIt == lineSeriesMap_.end()) {
        static constexpr std::array<const char*, 8> kSeriesColors = {
            "#315f87", "#7a5c2e", "#5d7f3a", "#8a4f55",
            "#4f6f75", "#6c5d8d", "#8a6f38", "#55707f"
        };
        const QColor seriesColor(kSeriesColors[lineSeriesMap_.size() % kSeriesColors.size()]);

        auto* lineSeries = new QLineSeries;
        lineSeries->setName(backend + " line");
        lineSeries->setPointsVisible(false);
        QPen seriesPen(seriesColor);
        seriesPen.setWidthF(2.0);
        lineSeries->setPen(seriesPen);

        chart_->addSeries(lineSeries);
        lineSeries->attachAxis(axisX_);
        lineSeries->attachAxis(axisY_);
        for (auto* marker : chart_->legend()->markers(lineSeries)) {
            marker->setVisible(false);
        }

        auto* pointSeries = new QScatterSeries;
        pointSeries->setName(backend);
        pointSeries->setMarkerSize(8);
        pointSeries->setColor(seriesColor);
        pointSeries->setPointLabelsFormat("x=@xPoint\ny=@yPoint");
        pointSeries->setPointLabelsClipping(true);

        chart_->addSeries(pointSeries);
        pointSeries->attachAxis(axisX_);
        pointSeries->attachAxis(axisY_);

        lineSeriesMap_[key] = lineSeries;
        pointSeriesMap_[key] = pointSeries;
    }

    auto& pointsByX = dataMap_[key];
    pointsByX[static_cast<double>(x)] = y;

    refreshDataBounds();

    if (!isZoomed_) {
        updateAxes();
    } else {
        updateRenderedSeries();
        updatePointLabels();
    }
}

void ResultChartView::clearAll() {
    for (auto& [_, series] : lineSeriesMap_) {
        chart_->removeSeries(series);
        delete series;
    }
    for (auto& [_, series] : pointSeriesMap_) {
        chart_->removeSeries(series);
        delete series;
    }
    lineSeriesMap_.clear();
    pointSeriesMap_.clear();
    dataMap_.clear();
    isZoomed_ = false;
    resetAxes();
}

void ResultChartView::updateAxes() {
    refreshDataBounds();
    setAxisRange(fullMinX_, fullMaxX_, fullMinY_, fullMaxY_, false);
}

void ResultChartView::resetAxes() {
    isZoomed_ = false;
    updateAxes();
}

void ResultChartView::refreshDataBounds() {
    bool hasData = false;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;

    for (const auto& [_, pointsByX] : dataMap_) {
        for (const auto& [x, y] : pointsByX) {
            if (!hasData) {
                minX = maxX = x;
                minY = maxY = y;
                hasData = true;
            } else {
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
                minY = std::min(minY, y);
                maxY = std::max(maxY, y);
            }
        }
    }

    hasData_ = hasData;
    if (!hasData) {
        fullMinX_ = 1.0;
        fullMaxX_ = 10.0;
        fullMinY_ = 0.0;
        fullMaxY_ = 10.0;
        return;
    }

    const double safeMinX = std::max(1.0, minX);
    const double safeMaxX = std::max(safeMinX * 1.01, maxX);
    double logMinX = std::log10(safeMinX);
    double logMaxX = std::log10(safeMaxX);
    if (logMaxX <= logMinX) {
        logMinX -= 0.5;
        logMaxX += 0.5;
    }
    const double logPad = std::max(0.08, (logMaxX - logMinX) * 0.12);
    fullMinX_ = std::max(1.0, std::pow(10.0, logMinX - logPad));
    fullMaxX_ = std::pow(10.0, logMaxX + logPad);

    const double ySpan = std::max(1.0, maxY - minY);
    const double yPad = std::max(1.0, ySpan * 0.18);
    fullMinY_ = std::min(0.0, minY - yPad);
    fullMaxY_ = std::max(fullMinY_ + 1.0, maxY + yPad);
}

void ResultChartView::setAxisRange(double minX, double maxX, double minY, double maxY, bool zoomed) {
    if (!hasData_) {
        axisX_->setRange(1, 10);
        axisY_->setRange(0, 10);
        isZoomed_ = false;
        return;
    }

    const double fullLogMinX = std::log10(fullMinX_);
    const double fullLogMaxX = std::log10(fullMaxX_);
    const double minLogSpan = std::max(0.08, (fullLogMaxX - fullLogMinX) * 0.06);

    double logMinX = std::log10(std::max(1.0, minX));
    double logMaxX = std::log10(std::max(1.0, maxX));
    if (logMaxX < logMinX) std::swap(logMinX, logMaxX);

    double logSpan = logMaxX - logMinX;
    if (logSpan < minLogSpan) {
        const double center = (logMinX + logMaxX) * 0.5;
        logMinX = center - minLogSpan * 0.5;
        logMaxX = center + minLogSpan * 0.5;
    }

    if (logMinX < fullLogMinX) {
        logMaxX += fullLogMinX - logMinX;
        logMinX = fullLogMinX;
    }
    if (logMaxX > fullLogMaxX) {
        logMinX -= logMaxX - fullLogMaxX;
        logMaxX = fullLogMaxX;
    }
    logMinX = std::max(logMinX, fullLogMinX);
    logMaxX = std::min(logMaxX, fullLogMaxX);

    const double fullYSpan = std::max(1.0, fullMaxY_ - fullMinY_);
    const double minYSpan = fullYSpan * 0.06;
    if (maxY < minY) std::swap(minY, maxY);

    double ySpan = maxY - minY;
    if (ySpan < minYSpan) {
        const double center = (minY + maxY) * 0.5;
        minY = center - minYSpan * 0.5;
        maxY = center + minYSpan * 0.5;
    }

    if (minY < fullMinY_) {
        maxY += fullMinY_ - minY;
        minY = fullMinY_;
    }
    if (maxY > fullMaxY_) {
        minY -= maxY - fullMaxY_;
        maxY = fullMaxY_;
    }
    minY = std::max(minY, fullMinY_);
    maxY = std::min(maxY, fullMaxY_);

    axisX_->setRange(std::pow(10.0, logMinX), std::pow(10.0, logMaxX));
    axisY_->setRange(minY, maxY);
    isZoomed_ = zoomed;
    updateRenderedSeries();
    updatePointLabels();
}

void ResultChartView::zoomAround(double factor, const QPoint& viewPos) {
    if (!hasData_) return;

    factor = std::clamp(factor, 0.2, 5.0);
    const QPointF anchor = chart_->mapToValue(viewPos);

    const double curMinX = axisX_->min();
    const double curMaxX = axisX_->max();
    const double curMinY = axisY_->min();
    const double curMaxY = axisY_->max();

    const double logMinX = std::log10(curMinX);
    const double logMaxX = std::log10(curMaxX);
    const double logAnchorX = std::clamp(std::log10(std::max(1.0, anchor.x())), logMinX, logMaxX);
    const double xRatio = (logAnchorX - logMinX) / std::max(1e-9, logMaxX - logMinX);
    const double yRatio = (anchor.y() - curMinY) / std::max(1e-9, curMaxY - curMinY);

    const double newLogSpan = (logMaxX - logMinX) * factor;
    const double newLogMinX = logAnchorX - newLogSpan * xRatio;
    const double newLogMaxX = newLogMinX + newLogSpan;

    const double newYSpan = (curMaxY - curMinY) * factor;
    const double newMinY = anchor.y() - newYSpan * yRatio;
    const double newMaxY = newMinY + newYSpan;

    setAxisRange(std::pow(10.0, newLogMinX), std::pow(10.0, newLogMaxX), newMinY, newMaxY, true);
}

void ResultChartView::zoomToRect(const QRect& viewRect) {
    if (!hasData_) return;

    const QRectF plotArea = chart_->plotArea();
    const QRectF selected = QRectF(viewRect).intersected(plotArea);
    if (selected.width() < 12 || selected.height() < 12) {
        return;
    }

    const QPointF topLeft = chart_->mapToValue(selected.topLeft());
    const QPointF bottomRight = chart_->mapToValue(selected.bottomRight());
    const double minX = std::min(topLeft.x(), bottomRight.x());
    const double maxX = std::max(topLeft.x(), bottomRight.x());
    const double minY = std::min(topLeft.y(), bottomRight.y());
    const double maxY = std::max(topLeft.y(), bottomRight.y());

    setAxisRange(minX, maxX, minY, maxY, true);
}

void ResultChartView::panPixels(const QPoint& deltaPixels) {
    if (!hasData_ || !isZoomed_) return;

    const QRectF plotArea = chart_->plotArea();
    if (plotArea.width() <= 1.0 || plotArea.height() <= 1.0) return;

    const double logMinX = std::log10(axisX_->min());
    const double logMaxX = std::log10(axisX_->max());
    const double logSpan = logMaxX - logMinX;
    const double logDelta = -static_cast<double>(deltaPixels.x()) / plotArea.width() * logSpan;

    const double minY = axisY_->min();
    const double maxY = axisY_->max();
    const double ySpan = maxY - minY;
    const double yDelta = static_cast<double>(deltaPixels.y()) / plotArea.height() * ySpan;

    setAxisRange(std::pow(10.0, logMinX + logDelta),
                 std::pow(10.0, logMaxX + logDelta),
                 minY + yDelta,
                 maxY + yDelta,
                 true);
}

void ResultChartView::updateRenderedSeries() {
    const double viewMinX = axisX_->min();
    const double viewMaxX = axisX_->max();
    const double epsilon = std::max(1.0, viewMaxX - viewMinX) * 1e-9;

    for (auto& [key, lineSeries] : lineSeriesMap_) {
        QList<QPointF> linePoints;
        QList<QPointF> visiblePoints;

        auto dataIt = dataMap_.find(key);
        if (dataIt != dataMap_.end()) {
            const auto& pointsByX = dataIt->second;
            linePoints.reserve(static_cast<qsizetype>(pointsByX.size()));
            visiblePoints.reserve(static_cast<qsizetype>(pointsByX.size()));

            if (!isZoomed_) {
                for (const auto& [x, y] : pointsByX) {
                    const QPointF point(x, y);
                    linePoints.append(point);
                    visiblePoints.append(point);
                }
            } else {
                auto firstVisible = pointsByX.lower_bound(viewMinX - epsilon);
                auto lineBegin = firstVisible;
                if (lineBegin != pointsByX.begin()) {
                    --lineBegin;
                }

                auto lineEnd = pointsByX.upper_bound(viewMaxX + epsilon);
                if (lineEnd != pointsByX.end()) {
                    ++lineEnd;
                }

                for (auto it = lineBegin; it != lineEnd; ++it) {
                    linePoints.append(QPointF(it->first, it->second));
                    if (it->first >= viewMinX - epsilon && it->first <= viewMaxX + epsilon) {
                        visiblePoints.append(QPointF(it->first, it->second));
                    }
                }
            }
        }

        lineSeries->replace(linePoints);
        auto pointIt = pointSeriesMap_.find(key);
        if (pointIt != pointSeriesMap_.end()) {
            pointIt->second->replace(visiblePoints);
        }
    }
}

void ResultChartView::updatePointLabels() {
    if (!hasData_) {
        for (auto& [_, series] : pointSeriesMap_) {
            series->setPointLabelsVisible(false);
        }
        return;
    }

    const double fullLogSpan = std::log10(fullMaxX_) - std::log10(fullMinX_);
    const double viewLogSpan = std::log10(axisX_->max()) - std::log10(axisX_->min());
    const bool showLabels = isZoomed_ && fullLogSpan > 0.0 && viewLogSpan <= fullLogSpan * 0.55;

    QFont labelFont;
    labelFont.setPointSize(8);

    for (auto& [_, series] : pointSeriesMap_) {
        series->setPointLabelsFont(labelFont);
        series->setPointLabelsColor(series->color().darker(130));
        series->setPointLabelsVisible(showLabels);
    }
}
