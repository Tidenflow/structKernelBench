#pragma once

#include <QCheckBox>
#include <QGroupBox>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

#include "ResultChartView.h"
#include "MaxStressEnvelope/envelope_runner.h"

// ============================================================
// EnvelopeMainWidget — 多工况最大应力包络算子 Tab 页面
//
// 布局: 左侧控制面板+表格 | 右侧图表
// ============================================================
class EnvelopeMainWidget : public QWidget {
    Q_OBJECT

public:
    explicit EnvelopeMainWidget(QWidget* parent = nullptr);

signals:
    void statusMessage(const QString& msg);

private slots:
    void onRunRequested();
    void runNext();

private:
    void setupUi();
    void applyStyle();
    void flushChart();

    // 参数
    QSpinBox*  spinElements_;
    QSlider*   sliderElements_;
    QSpinBox*  spinLoadCases_;

    // 后端选择
    QCheckBox* chkBaseline_;
    QCheckBox* chkOpenmp_;
    QCheckBox* chkSimd_;
    QCheckBox* chkCuda_;
    QCheckBox* chkThrust_;

    // 操作
    QPushButton*  btnRun_;
    QProgressBar* progress_;

    // 最新结果
    QLabel* labelBackend_;
    QLabel* labelTime_;
    QLabel* labelThroughput_;
    QLabel* labelError_;
    QLabel* labelValid_;

    ResultChartView* chart_;
    QTableWidget*    table_;

    QStringList                   backends_;
    int                           taskIndex_ = 0;
    int                           numElements_ = 0;
    int                           numLoadCases_ = 0;
    std::vector<EnvelopeResult>   resultsBuffer_;
};
