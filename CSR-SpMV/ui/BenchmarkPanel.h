#pragma once

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// ============================================================
// RunConfig — 一次批量 benchmark 的配置
// ============================================================
struct RunConfig {
    int         rows;           // 单个矩阵行数
    int         nnz_per_row;
    QStringList backends;       // 勾选的后端列表
};

// ============================================================
// BenchmarkPanel — 左侧控制面板
//
// 参数: 矩阵行数 / 非零元 / 多后端勾选
// 操作: 运行按钮 + 进度条
// 显示: 最新一次结果
// ============================================================
class BenchmarkPanel : public QWidget {
    Q_OBJECT

public:
    explicit BenchmarkPanel(QWidget* parent = nullptr);

    void showResult(const QString& backend, int rows, int nnz,
                    double time_ms, double gflops, double max_error, bool validated);
    void setRunning(bool running, int done = 0, int total = 0);

signals:
    void runRequested(const RunConfig& config);
    void clearRequested();

private:
    void setupUi();
    void applyStyle();

    // 矩阵规模
    QSpinBox* spinRows_;
    QSlider*  sliderRows_;
    QSpinBox* spinNnzPerRow_;

    // 后端勾选
    QCheckBox* chkBaseline_;
    QCheckBox* chkOpenmp_;
    QCheckBox* chkSimd_;
    QCheckBox* chkCuda_;

    // 运行
    QPushButton*  btnRun_;
    QProgressBar* progress_;

    // 最新结果
    QLabel* labelBackend_;
    QLabel* labelTime_;
    QLabel* labelGflops_;
    QLabel* labelError_;
    QLabel* labelValid_;
};
