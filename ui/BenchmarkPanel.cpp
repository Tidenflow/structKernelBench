#include "BenchmarkPanel.h"

#include <QFormLayout>

#include <cmath>

BenchmarkPanel::BenchmarkPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
    applyStyle();
}

void BenchmarkPanel::setupUi() {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // ============================================================
    // 矩阵规模
    // ============================================================
    auto* sizeGroup = new QGroupBox("矩阵规模 / Matrix Size");
    auto* sizeForm  = new QFormLayout(sizeGroup);

    spinRows_ = new QSpinBox;
    spinRows_->setRange(1000, 1'000'000);
    spinRows_->setSingleStep(1000);
    spinRows_->setValue(10000);
    spinRows_->setSuffix(" 行");
    spinRows_->setToolTip("取值范围: 1,000 ~ 1,000,000");
    sizeForm->addRow("矩阵行数 Rows:", spinRows_);

    // 对数滑条 (0~600 → 10^3~10^6)
    sliderRows_ = new QSlider(Qt::Horizontal);
    sliderRows_->setRange(0, 600);
    sliderRows_->setValue(200);  // 默认 10^(3 + 200/200) = 10^4 = 10000
    sizeForm->addRow(sliderRows_);

    // 双向同步: 滑条 ↔ 输入框 (对数转换)
    connect(sliderRows_, &QSlider::valueChanged, this, [this](int sv) {
        int rows = static_cast<int>(std::pow(10.0, 3.0 + sv / 200.0));
        spinRows_->blockSignals(true);
        spinRows_->setValue(rows);
        spinRows_->blockSignals(false);
    });
    connect(spinRows_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int rows) {
        int sv = static_cast<int>((std::log10(static_cast<double>(rows)) - 3.0) * 200.0);
        sliderRows_->blockSignals(true);
        sliderRows_->setValue(std::clamp(sv, 0, 600));
        sliderRows_->blockSignals(false);
    });

    spinNnzPerRow_ = new QSpinBox;
    spinNnzPerRow_->setRange(1, 100);
    spinNnzPerRow_->setValue(10);
    sizeForm->addRow("每行非零元 nnz/row:", spinNnzPerRow_);

    mainLayout->addWidget(sizeGroup);

    // ============================================================
    // 后端选择
    // ============================================================
    auto* backendGroup = new QGroupBox("并行后端 / Backend");
    auto* backendLayout = new QVBoxLayout(backendGroup);

    chkBaseline_ = new QCheckBox("CSR-SpMV baseline (CPU 单线程)");
    chkBaseline_->setChecked(true);
    backendLayout->addWidget(chkBaseline_);

    chkOpenmp_ = new QCheckBox("OpenMP 多线程");
    chkOpenmp_->setEnabled(true);
    backendLayout->addWidget(chkOpenmp_);

    chkSimd_ = new QCheckBox("SIMD 编译优化 (-O3 -march=native)");
    chkSimd_->setEnabled(true);
    backendLayout->addWidget(chkSimd_);

    chkCuda_ = new QCheckBox("CUDA GPU");
#ifdef HAS_CUDA
    chkCuda_->setEnabled(true);
#else
    chkCuda_->setEnabled(false);
    chkCuda_->setToolTip("编译时未检测到 CUDA Toolkit");
#endif
    backendLayout->addWidget(chkCuda_);

    chkCusparse_ = new QCheckBox("cuSPARSE GPU (工业级参考)");
#ifdef HAS_CUDA
    chkCusparse_->setEnabled(true);
#else
    chkCusparse_->setEnabled(false);
    chkCusparse_->setToolTip("编译时未检测到 CUDA Toolkit");
#endif
    backendLayout->addWidget(chkCusparse_);

    mainLayout->addWidget(backendGroup);

    // ============================================================
    // 运行按钮 + 进度
    // ============================================================
    btnRun_ = new QPushButton("▶  运行 Benchmark");
    btnRun_->setMinimumHeight(42);
    btnRun_->setCursor(Qt::PointingHandCursor);
    mainLayout->addWidget(btnRun_);

    progress_ = new QProgressBar;
    progress_->setVisible(false);
    mainLayout->addWidget(progress_);

    // 清除按钮
    auto* btnClear = new QPushButton("清除结果 / Clear Results");
    btnClear->setMinimumHeight(34);
    btnClear->setCursor(Qt::PointingHandCursor);
    btnClear->setStyleSheet(
        "QPushButton { background-color: #eeeeee; color: #555; "
        "border: 1px solid #ccc; border-radius: 6px; font-size: 13px; }"
        "QPushButton:hover { background-color: #e0e0e0; }");
    mainLayout->addWidget(btnClear);
    connect(btnClear, &QPushButton::clicked, this, [this]() {
        emit clearRequested();
    });

    connect(btnRun_, &QPushButton::clicked, this, [this]() {
        QStringList backends;
        if (chkBaseline_->isChecked() && chkBaseline_->isEnabled())
            backends << "baseline";
        if (chkOpenmp_->isChecked() && chkOpenmp_->isEnabled())
            backends << "openmp";
        if (chkSimd_->isChecked() && chkSimd_->isEnabled())
            backends << "simd";
        if (chkCuda_->isChecked() && chkCuda_->isEnabled())
            backends << "cuda";
        if (chkCusparse_->isChecked() && chkCusparse_->isEnabled())
            backends << "cusparse";

        if (backends.isEmpty()) return;

        RunConfig cfg;
        cfg.rows        = spinRows_->value();
        cfg.nnz_per_row = spinNnzPerRow_->value();
        cfg.backends    = backends;

        emit runRequested(cfg);
    });

    // ============================================================
    // 最新结果
    // ============================================================
    auto* resultGroup = new QGroupBox("最新结果 / Latest Result");
    auto* resultForm  = new QFormLayout(resultGroup);

    labelBackend_ = new QLabel("—");
    labelTime_    = new QLabel("—");
    labelGflops_  = new QLabel("—");
    labelError_   = new QLabel("—");
    labelValid_   = new QLabel("—");

    resultForm->addRow("后端 Backend:",     labelBackend_);
    resultForm->addRow("耗时 Time (ms):",   labelTime_);
    resultForm->addRow("吞吐量 Throughput:", labelGflops_);
    resultForm->addRow("误差 Max Error:",   labelError_);
    resultForm->addRow("验证 Validation:",  labelValid_);

    mainLayout->addWidget(resultGroup);
    mainLayout->addStretch();
}

void BenchmarkPanel::applyStyle() {
    setStyleSheet(R"(
        QGroupBox {
            font-weight: bold;
            border: 1px solid #bbb;
            border-radius: 6px;
            margin-top: 10px;
            padding-top: 14px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
        QPushButton {
            background-color: #2979ff;
            color: white;
            border: none;
            border-radius: 8px;
            font-size: 15px;
            font-weight: bold;
        }
        QPushButton:hover     { background-color: #448aff; }
        QPushButton:pressed   { background-color: #2962ff; }
        QPushButton:disabled  { background-color: #b0bec5; }
        QProgressBar {
            border: 1px solid #bbb;
            border-radius: 4px;
            text-align: center;
        }
        QProgressBar::chunk {
            background-color: #2979ff;
            border-radius: 3px;
        }
        QCheckBox { spacing: 6px; }
    )");
}

void BenchmarkPanel::showResult(const QString& backend, int rows, int nnz,
                                double time_ms, double gflops,
                                double max_error, bool validated) {
    Q_UNUSED(rows); Q_UNUSED(nnz);
    labelBackend_->setText(backend);
    labelTime_->setText(QString::number(time_ms, 'f', 4) + " ms");
    labelGflops_->setText(QString::number(gflops, 'f', 3) + " GFLOPS");
    labelError_->setText(QString::number(max_error, 'e', 2));
    labelValid_->setText(validated ? "✓ PASS" : "✗ FAIL");
    labelValid_->setStyleSheet(validated
        ? "color: #2e7d32; font-weight: bold;" : "color: #c62828; font-weight: bold;");
}

void BenchmarkPanel::setRunning(bool running, int done, int total) {
    btnRun_->setEnabled(!running);
    if (running) {
        btnRun_->setText("⏳  运行中...");
        progress_->setVisible(true);
        progress_->setRange(0, total);
        progress_->setValue(done);
    } else {
        btnRun_->setText("▶  运行 Benchmark");
        progress_->setVisible(false);
    }
}
