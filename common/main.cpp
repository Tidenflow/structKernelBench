#include <QApplication>
#include <QLabel>
#include <QMainWindow>
#include <QStatusBar>
#include <QTabWidget>

#include "SpmvMainWidget.h"
#include "VonMisesMainWidget.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("StructKernelBench");
    app.setApplicationVersion("0.2");

    QMainWindow window;
    window.setWindowTitle("StructKernelBench — 结构仿真核心算子性能分析");
    window.resize(1280, 780);
    window.statusBar()->showMessage("就绪 Ready");

    auto* tabs = new QTabWidget;
    tabs->setDocumentMode(true);

    // ---- CSR-SpMV tab ----
    auto* spmvWidget = new SpmvMainWidget;
    tabs->addTab(spmvWidget, "CSR-SpMV / 稀疏矩阵向量乘");
    QObject::connect(spmvWidget, &SpmvMainWidget::statusMessage,
                     &window, [&window](const QString& msg) {
                         window.statusBar()->showMessage(msg);
                     });

    // ---- Von Mises tab ----
    auto* vmWidget = new VonMisesMainWidget;
    tabs->addTab(vmWidget, "Von Mises / 等效应力");
    QObject::connect(vmWidget, &VonMisesMainWidget::statusMessage,
                     &window, [&window](const QString& msg) {
                         window.statusBar()->showMessage(msg);
                     });

    // TODO: MaxStressEnvelope
    auto* placeholder = new QLabel("敬请期待 Coming Soon");
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("font-size: 18px; color: #999;");
    tabs->addTab(placeholder, "MaxStress / 多工况包络");

    window.setCentralWidget(tabs);

    window.setStyleSheet(R"(
        QMainWindow { background-color: #fafafa; }
        QStatusBar  { background-color: #e8e8e8; }
    )");

    window.show();
    return app.exec();
}
