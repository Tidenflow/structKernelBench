#include "SpmvMainWidget.h"

#include <QFutureWatcher>
#include <QHeaderView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

SpmvMainWidget::SpmvMainWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    applyStyle();
}

void SpmvMainWidget::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Horizontal);

    // ═══════════════════ 左侧: 面板 + 表格 ═══════════════════
    auto* leftWidget  = new QWidget;
    auto* leftLayout  = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 4, 0);
    leftLayout->setSpacing(6);

    panel_ = new BenchmarkPanel;
    leftLayout->addWidget(panel_);

    table_ = new QTableWidget(0, 7);
    table_->setHorizontalHeaderLabels(
        {"Rows", "NNZ", "Backend", "Time(ms)", "GFLOPS", "Error", "Timestamp"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    leftLayout->addWidget(table_, 1);  // stretch=1, 表格占满剩余空间

    splitter->addWidget(leftWidget);

    // ═══════════════════ 右侧: 图表 ═══════════════════
    chart_ = new ResultChartView(
        "SpMV Performance / 稀疏矩阵向量乘性能对比",
        "矩阵行数 Rows",
        "吞吐量 Throughput (GFLOPS)");
    chart_->setMinimumWidth(500);
    splitter->addWidget(chart_);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 7);
    splitter->setSizes({300, 700});

    outerLayout->addWidget(splitter);

    connect(panel_, &BenchmarkPanel::runRequested,
            this, &SpmvMainWidget::onRunRequested);
    connect(panel_, &BenchmarkPanel::clearRequested,
            this, [this]() {
        table_->setRowCount(0);
        chart_->clearAll();
        emit statusMessage("已清除 Cleared");
    });
}

void SpmvMainWidget::applyStyle() {
    setStyleSheet(R"(
        QTableWidget {
            gridline-color: #dfe3e8;
            font-size: 12px;
            background-color: #ffffff;
            alternate-background-color: #f7f8fa;
            selection-background-color: #d9e1e8;
            selection-color: #20262d;
        }
        QTableWidget::item { padding: 4px; }
        QHeaderView::section {
            background-color: #eef1f4;
            border: 0;
            border-right: 1px solid #d7dce1;
            border-bottom: 1px solid #c9ced6;
            padding: 4px 6px;
            font-weight: 600;
        }
    )");
}

void SpmvMainWidget::onRunRequested(const RunConfig& cfg) {
    config_    = cfg;
    taskIndex_ = 0;
    taskTotal_ = cfg.backends.size();

    // 表格不清除 — 新结果追加在下面

    panel_->setRunning(true, 0, taskTotal_);
    emit statusMessage(
        QString("运行中: rows=%1, %2 个后端...")
            .arg(cfg.rows).arg(cfg.backends.size()));

    runNext();
}

void SpmvMainWidget::runNext() {
    if (taskIndex_ >= taskTotal_) {
        flushChart();
        panel_->setRunning(false);
        emit statusMessage(
            QString("完成 Done — %1/%1 项").arg(taskTotal_));
        return;
    }

    int     rows    = config_.rows;
    int     nnzRow  = config_.nnz_per_row;
    QString backend = config_.backends[taskIndex_];

    panel_->setRunning(true, taskIndex_, taskTotal_);

    std::string backendStr = backend.toStdString();

    auto* watcher = new QFutureWatcher<BenchmarkResult>(this);
    connect(watcher, &QFutureWatcher<BenchmarkResult>::finished,
            this, [this, watcher, backend]() {
                BenchmarkResult r = watcher->result();

                panel_->showResult(backend, r.rows, r.nnz,
                                   r.time_ms, r.gflops, r.max_error, r.validated);

                // 追加到表格（不清除旧行）
                int row = table_->rowCount();
                table_->insertRow(row);
                table_->setItem(row, 0, new QTableWidgetItem(QString::number(r.rows)));
                table_->setItem(row, 1, new QTableWidgetItem(QString::number(r.nnz)));
                table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.backend)));
                table_->setItem(row, 3, new QTableWidgetItem(QString::number(r.time_ms, 'f', 4)));
                table_->setItem(row, 4, new QTableWidgetItem(QString::number(r.gflops, 'f', 3)));
                table_->setItem(row, 5, new QTableWidgetItem(QString::number(r.max_error, 'e', 2)));
                table_->setItem(row, 6, new QTableWidgetItem(QString::fromStdString(r.timestamp)));
                table_->scrollToBottom();

                resultsBuffer_.push_back(r);

                ++taskIndex_;
                watcher->deleteLater();
                runNext();
            });

    auto future = QtConcurrent::run([rows, nnzRow, backendStr]() {
        BenchmarkResult result;
        run_spmv_benchmark(rows, nnzRow, backendStr, result);
        return result;
    });

    watcher->setFuture(future);
}

void SpmvMainWidget::flushChart() {
    for (auto& r : resultsBuffer_) {
        chart_->addResult(QString::fromStdString(r.backend), r.rows, r.gflops);
    }
    resultsBuffer_.clear();
}
