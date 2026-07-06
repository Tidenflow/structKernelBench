#pragma once

#include <QTableWidget>
#include <QWidget>
#include <vector>

#include "BenchmarkPanel.h"
#include "ResultChartView.h"
#include "CSR-SpMV/spmv_runner.h"

// ============================================================
// SpmvMainWidget — CSR-SpMV 算子主界面 (QWidget)
//
// 作为 Tab 页面嵌入顶层 QTabWidget。
// 布局: 左侧面板 | 中间表格 | 右侧图表
// 执行: 按勾选顺序依次跑, 全部完成后一次性画图
// ============================================================
class SpmvMainWidget : public QWidget {
    Q_OBJECT

public:
    explicit SpmvMainWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

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
