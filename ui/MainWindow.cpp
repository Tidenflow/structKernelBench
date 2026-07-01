#include "MainWindow.h"

#include <QFutureWatcher>
#include <QHeaderView>
#include <QSplitter>
#include <QStatusBar>
#include <QtConcurrent/QtConcurrent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    applyStyle();
}

void MainWindow::setupUi() {
    setWindowTitle("StructKernelBench — 结构仿真核心算子性能分析");
    resize(1200, 750);
    statusBar()->showMessage("就绪 Ready");

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
    chart_ = new ResultChartView;
    chart_->setMinimumWidth(500);
    splitter->addWidget(chart_);

    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 3);

    setCentralWidget(splitter);

    connect(panel_, &BenchmarkPanel::runRequested,
            this, &MainWindow::onRunRequested);
    connect(panel_, &BenchmarkPanel::clearRequested,
            this, [this]() {
        table_->setRowCount(0);
        chart_->clearAll();
        statusBar()->showMessage("已清除 Cleared");
    });
}

void MainWindow::applyStyle() {
    setStyleSheet(R"(
        QMainWindow { background-color: #fafafa; }
        QTableWidget {
            gridline-color: #e0e0e0;
            font-size: 12px;
        }
        QTableWidget::item { padding: 4px; }
        QStatusBar { background-color: #e8e8e8; }
    )");
}

void MainWindow::onRunRequested(const RunConfig& cfg) {
    config_    = cfg;
    taskIndex_ = 0;
    taskTotal_ = cfg.backends.size();

    // 表格不清除 — 新结果追加在下面

    panel_->setRunning(true, 0, taskTotal_);
    statusBar()->showMessage(
        QString("运行中: rows=%1, %2 个后端...")
            .arg(cfg.rows).arg(cfg.backends.size()));

    runNext();
}

void MainWindow::runNext() {
    if (taskIndex_ >= taskTotal_) {
        flushChart();
        panel_->setRunning(false);
        statusBar()->showMessage(
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

void MainWindow::flushChart() {
    for (auto& r : resultsBuffer_) {
        chart_->addResult(QString::fromStdString(r.backend), r.rows, r.gflops);
    }
    resultsBuffer_.clear();
}
