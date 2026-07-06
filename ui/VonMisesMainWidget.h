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
#include "VonMises/vonmises_runner.h"

// ============================================================
// VonMisesMainWidget — Von Mises 算子 Tab 页面
//
// 布局: 左侧控制面板+表格 | 右侧图表
// ============================================================
class VonMisesMainWidget : public QWidget {
    Q_OBJECT

public:
    explicit VonMisesMainWidget(QWidget* parent = nullptr);

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
    QCheckBox* chkBaseline_;
    QCheckBox* chkOpenmp_;
    QCheckBox* chkSimd_;
    QCheckBox* chkCuda_;
    QCheckBox* chkThrust_;

    // 操作
    QPushButton*  btnRun_;
    QProgressBar* progress_;

    // 结果
    QLabel* labelBackend_;
    QLabel* labelTime_;
    QLabel* labelThroughput_;
    QLabel* labelError_;
    QLabel* labelValid_;

    ResultChartView* chart_;
    QTableWidget*    table_;

    QStringList                  backends_;
    int                          taskIndex_ = 0;
    int                          numElements_ = 0;
    std::vector<VonMisesResult>  resultsBuffer_;
};
