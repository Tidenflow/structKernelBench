#include <QApplication>
#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("StructKernelBench");
    app.setApplicationVersion("0.2");

    MainWindow window;
    window.show();

    return app.exec();
}
