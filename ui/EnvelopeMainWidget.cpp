#include "EnvelopeMainWidget.h"

#include <QFormLayout>
#include <QFutureWatcher>
#include <QHeaderView>
#include <QSplitter>
#include <QtConcurrent/QtConcurrent>

#include <algorithm>
#include <cmath>

EnvelopeMainWidget::EnvelopeMainWidget(QWidget* parent) : QWidget(parent) {
    setupUi();
    applyStyle();
}

void EnvelopeMainWidget::setupUi() {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    auto* splitter = new QSplitter(Qt::Horizontal);

    // ============================================================
    // 左侧: 面板 + 表格
    // ============================================================
    auto* leftWidget  = new QWidget;
    auto* leftLayout  = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(10, 10, 4, 10);
    leftLayout->setSpacing(8);

    // ---- 规模设置 ----
    auto* sizeGroup = new QGroupBox("数据规模 / Data Size");
    auto* sizeForm  = new QFormLayout(sizeGroup);

    spinElements_ = new QSpinBox;
    spinElements_->setRange(1000, 10'000'000);
    spinElements_->setSingleStep(10000);
    spinElements_->setValue(100000);
    spinElements_->setSuffix(" 单元");
    sizeForm->addRow("单元数 Elements:", spinElements_);

    sliderElements_ = new QSlider(Qt::Horizontal);
    sliderElements_->setRange(0, 700);  // 10^3 ~ 10^7 对数
    sliderElements_->setValue(350);     // 默认 10^(3+350/175) = 10^5 = 100000
    sizeForm->addRow(sliderElements_);

    connect(sliderElements_, &QSlider::valueChanged, this, [this](int sv) {
        int n = static_cast<int>(std::pow(10.0, 3.0 + sv / 175.0));
        spinElements_->blockSignals(true);
        spinElements_->setValue(n);
        spinElements_->blockSignals(false);
    });
    connect(spinElements_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int n) {
        int sv = static_cast<int>((std::log10(static_cast<double>(n)) - 3.0) * 175.0);
        sliderElements_->blockSignals(true);
        sliderElements_->setValue(std::clamp(sv, 0, 700));
        sliderElements_->blockSignals(false);
    });

    spinLoadCases_ = new QSpinBox;
    spinLoadCases_->setRange(1, 1000);
    spinLoadCases_->setValue(10);
    spinLoadCases_->setToolTip("工况数: 1 ~ 1000");
    sizeForm->addRow("工况数 Load Cases:", spinLoadCases_);

    leftLayout->addWidget(sizeGroup);

    // ---- 后端选择 ----
    auto* backendGroup = new QGroupBox("并行后端 / Backend");
    auto* backendLayout = new QVBoxLayout(backendGroup);

    chkBaseline_ = new QCheckBox("CPU baseline (单线程)");
    chkBaseline_->setChecked(true);
    backendLayout->addWidget(chkBaseline_);

    chkOpenmp_ = new QCheckBox("OpenMP 多线程");
    backendLayout->addWidget(chkOpenmp_);

    chkSimd_ = new QCheckBox("SIMD 编译优化 (-O3 -march=native)");
    backendLayout->addWidget(chkSimd_);

    chkCuda_ = new QCheckBox("CUDA GPU");
#ifdef HAS_CUDA
    chkCuda_->setEnabled(true);
#else
    chkCuda_->setEnabled(false);
    chkCuda_->setToolTip("编译时未检测到 CUDA Toolkit");
#endif
    backendLayout->addWidget(chkCuda_);

    chkCub_ = new QCheckBox("CUB GPU (工业级参考 — 分段归约)");
#ifdef HAS_CUDA
    chkCub_->setEnabled(true);
#else
    chkCub_->setEnabled(false);
    chkCub_->setToolTip("编译时未检测到 CUDA Toolkit");
#endif
    backendLayout->addWidget(chkCub_);

    leftLayout->addWidget(backendGroup);

    // ---- 运行 + 进度 ----
    btnRun_ = new QPushButton("运行 Benchmark");
    btnRun_->setMinimumHeight(38);
    btnRun_->setCursor(Qt::PointingHandCursor);
    leftLayout->addWidget(btnRun_);

    progress_ = new QProgressBar;
    progress_->setVisible(false);
    leftLayout->addWidget(progress_);

    auto* btnClear = new QPushButton("清除结果 / Clear Results");
    btnClear->setMinimumHeight(32);
    btnClear->setCursor(Qt::PointingHandCursor);
    btnClear->setStyleSheet(
        "QPushButton { background-color: #f8f9fa; color: #30363d; "
        "border: 1px solid #c9ced6; border-radius: 3px; font-size: 13px; }"
        "QPushButton:hover { background-color: #eef1f4; }"
        "QPushButton:pressed { background-color: #e3e7eb; }");
    leftLayout->addWidget(btnClear);

    // ---- 最新结果 ----
    auto* resultGroup = new QGroupBox("最新结果 / Latest Result");
    auto* resultForm  = new QFormLayout(resultGroup);

    labelBackend_    = new QLabel("—");
    labelTime_       = new QLabel("—");
    labelThroughput_ = new QLabel("—");
    labelError_      = new QLabel("—");
    labelValid_      = new QLabel("—");

    resultForm->addRow("后端 Backend:",         labelBackend_);
    resultForm->addRow("耗时 Time (ms):",       labelTime_);
    resultForm->addRow("吞吐量 (M elem/s):",    labelThroughput_);
    resultForm->addRow("误差 Max Error:",       labelError_);
    resultForm->addRow("验证 Validation:",      labelValid_);

    leftLayout->addWidget(resultGroup);

    // ---- 结果表格 ----
    table_ = new QTableWidget(0, 7);
    table_->setHorizontalHeaderLabels(
        {"Elements", "LoadCases", "Backend", "Time(ms)", "M elem/s", "Error", "Timestamp"});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(true);
    leftLayout->addWidget(table_, 1);

    splitter->addWidget(leftWidget);

    // ============================================================
    // 右侧: 图表
    // ============================================================
    chart_ = new ResultChartView(
        "MaxStressEnvelope Performance / 多工况应力包络性能对比",
        "单元数 Elements",
        "吞吐量 (M elem/s)");
    chart_->setMinimumWidth(500);
    splitter->addWidget(chart_);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 7);
    splitter->setSizes({300, 700});

    outerLayout->addWidget(splitter);

    // ---- 信号连接 ----
    connect(btnRun_, &QPushButton::clicked, this, &EnvelopeMainWidget::onRunRequested);
    connect(btnClear, &QPushButton::clicked, this, [this]() {
        table_->setRowCount(0);
        chart_->clearAll();
        emit statusMessage("已清除 Cleared");
    });
}

void EnvelopeMainWidget::applyStyle() {
    setStyleSheet(R"(
        QWidget {
            color: #20262d;
            font-size: 13px;
        }
        QGroupBox {
            font-weight: 600;
            border: 1px solid #c9ced6;
            border-radius: 3px;
            margin-top: 9px;
            padding: 12px 8px 8px 8px;
            background-color: #ffffff;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 8px;
            padding: 0 4px;
            color: #2f3943;
        }
        QPushButton {
            background-color: #3f5365;
            color: white;
            border: 1px solid #354757;
            border-radius: 3px;
            font-size: 14px;
            font-weight: 600;
        }
        QPushButton:hover     { background-color: #4a6072; }
        QPushButton:pressed   { background-color: #314250; }
        QPushButton:disabled  { background-color: #d7dce1; color: #7b838c; border-color: #c2c8cf; }
        QSpinBox {
            min-height: 24px;
            border: 1px solid #c3c8cf;
            border-radius: 2px;
            padding: 2px 6px;
            background-color: #ffffff;
        }
        QSlider::groove:horizontal {
            height: 4px;
            background: #d8dde3;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            width: 14px;
            margin: -5px 0;
            border: 1px solid #7b8794;
            border-radius: 2px;
            background: #f7f8fa;
        }
        QSlider::sub-page:horizontal {
            background: #667888;
            border-radius: 2px;
        }
        QProgressBar {
            border: 1px solid #c3c8cf;
            border-radius: 2px;
            text-align: center;
            background-color: #f4f6f8;
        }
        QProgressBar::chunk {
            background-color: #5d7080;
            border-radius: 1px;
        }
        QCheckBox { spacing: 6px; }
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

void EnvelopeMainWidget::onRunRequested() {
    backends_.clear();
    if (chkBaseline_->isChecked() && chkBaseline_->isEnabled())
        backends_ << "baseline";
    if (chkOpenmp_->isChecked() && chkOpenmp_->isEnabled())
        backends_ << "openmp";
    if (chkSimd_->isChecked() && chkSimd_->isEnabled())
        backends_ << "simd";
    if (chkCuda_->isChecked() && chkCuda_->isEnabled())
        backends_ << "cuda";
    if (chkCub_->isChecked() && chkCub_->isEnabled())
        backends_ << "cub";

    if (backends_.isEmpty()) return;

    numElements_  = spinElements_->value();
    numLoadCases_ = spinLoadCases_->value();
    taskIndex_    = 0;

    btnRun_->setEnabled(false);
    btnRun_->setText("运行中...");
    progress_->setVisible(true);
    progress_->setRange(0, backends_.size());
    progress_->setValue(0);
    emit statusMessage(
        QString("运行中: %1 单元 × %2 工况, %3 个后端...")
            .arg(numElements_).arg(numLoadCases_).arg(backends_.size()));

    runNext();
}

void EnvelopeMainWidget::runNext() {
    if (taskIndex_ >= backends_.size()) {
        flushChart();
        btnRun_->setEnabled(true);
        btnRun_->setText("运行 Benchmark");
        progress_->setVisible(false);
        emit statusMessage(
            QString("完成 Done — %1/%1 项").arg(backends_.size()));
        return;
    }

    int     M       = numElements_;
    int     N       = numLoadCases_;
    QString backend = backends_[taskIndex_];
    progress_->setValue(taskIndex_);

    std::string backendStr = backend.toStdString();

    auto* watcher = new QFutureWatcher<EnvelopeResult>(this);
    connect(watcher, &QFutureWatcher<EnvelopeResult>::finished,
            this, [this, watcher, backend]() {
                EnvelopeResult r = watcher->result();

                labelBackend_->setText(backend);
                labelTime_->setText(QString::number(r.time_ms, 'f', 4) + " ms");
                labelThroughput_->setText(QString::number(r.throughput, 'f', 2) + " M elem/s");
                labelError_->setText(QString::number(r.max_error, 'e', 2));
                labelValid_->setText(r.validated ? "✓ PASS" : "✗ FAIL");
                labelValid_->setStyleSheet(r.validated
                    ? "color: #2e7d32; font-weight: bold;" : "color: #c62828; font-weight: bold;");

                int row = table_->rowCount();
                table_->insertRow(row);
                table_->setItem(row, 0, new QTableWidgetItem(QString::number(r.num_elements)));
                table_->setItem(row, 1, new QTableWidgetItem(QString::number(r.num_load_cases)));
                table_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.backend)));
                table_->setItem(row, 3, new QTableWidgetItem(QString::number(r.time_ms, 'f', 4)));
                table_->setItem(row, 4, new QTableWidgetItem(QString::number(r.throughput, 'f', 2)));
                table_->setItem(row, 5, new QTableWidgetItem(QString::number(r.max_error, 'e', 2)));
                table_->setItem(row, 6, new QTableWidgetItem(QString::fromStdString(r.timestamp)));
                table_->scrollToBottom();

                resultsBuffer_.push_back(r);

                ++taskIndex_;
                watcher->deleteLater();
                runNext();
            });

    auto future = QtConcurrent::run([M, N, backendStr]() {
        EnvelopeResult result;
        run_envelope_benchmark(M, N, backendStr, result);
        return result;
    });

    watcher->setFuture(future);
}

void EnvelopeMainWidget::flushChart() {
    for (auto& r : resultsBuffer_) {
        chart_->addResult(QString::fromStdString(r.backend), r.num_elements, r.throughput);
    }
    resultsBuffer_.clear();
}
