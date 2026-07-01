#pragma once

#include <QMainWindow>
#include <QTableWidget>
#include <vector>

#include "BenchmarkPanel.h"
#include "ResultChartView.h"
#include "spmv_runner.h"

// ============================================================
// MainWindow — 主窗口
//
// 布局: 左侧面板 | 中间表格 | 右侧图表
// 执行: 按勾选顺序依次跑, 全部完成后一次性画图
// ============================================================
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onRunRequested(const RunConfig& cfg);
    void runNext();

private:
    void setupUi();
    void applyStyle();
    void flushChart();

    BenchmarkPanel*   panel_;
    ResultChartView*  chart_;
    QTableWidget*     table_;

    RunConfig                     config_;
    int                           taskIndex_ = 0;
    int                           taskTotal_ = 0;
    std::vector<BenchmarkResult> resultsBuffer_;
};
