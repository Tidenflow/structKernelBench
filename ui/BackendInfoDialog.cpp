#include "BackendInfoDialog.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QTabWidget>
#include <QTextBrowser>
#include <QTextDocument>
#include <QVBoxLayout>

BackendInfoDialog::BackendInfoDialog(const QString& kernelKey,
                                     const QString& backendKey,
                                     QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(titleForBackend(kernelKey, backendKey));
    resize(760, 620);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* title = new QLabel(titleForBackend(kernelKey, backendKey));
    title->setObjectName("BackendInfoTitle");
    layout->addWidget(title);

    auto* tabs = new QTabWidget;
    tabs->setDocumentMode(true);
    loadVersionTabs(tabs, kernelKey, backendKey);
    layout->addWidget(tabs, 1);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    setStyleSheet(R"(
        QDialog {
            background-color: #f3f5f7;
            color: #20262d;
            font-size: 13px;
        }
        QLabel#BackendInfoTitle {
            font-size: 17px;
            font-weight: 700;
            color: #111820;
        }
        QTabWidget::pane {
            border: 1px solid #c9ced6;
            background-color: #ffffff;
        }
        QTabBar::tab {
            background-color: #e7ebef;
            color: #30363d;
            border: 1px solid #c9ced6;
            border-bottom: none;
            padding: 6px 14px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: #ffffff;
            color: #111820;
            font-weight: 600;
        }
        QTextBrowser {
            background-color: #ffffff;
            border: none;
            padding: 10px;
            selection-background-color: #d9e1e8;
        }
    )");
}

QString BackendInfoDialog::titleForBackend(const QString& kernelKey,
                                           const QString& backendKey) const
{
    return QString("%1 / %2 实现说明")
        .arg(kernelKey, backendDisplayName(backendKey));
}

QString BackendInfoDialog::backendDisplayName(const QString& backendKey) const
{
    if (backendKey == "baseline") return "baseline";
    if (backendKey == "openmp") return "OpenMP";
    if (backendKey == "simd") return "SIMD";
    if (backendKey == "cuda") return "CUDA";
    if (backendKey == "cusparse") return "cuSPARSE";
    if (backendKey == "thrust") return "Thrust";
    if (backendKey == "cub") return "CUB";
    return backendKey;
}

void BackendInfoDialog::loadVersionTabs(QTabWidget* tabs,
                                        const QString& kernelKey,
                                        const QString& backendKey)
{
    const QString dirPath = QString(":/backend_notes/%1/docs/%2")
        .arg(kernelKey, backendKey);
    QDir dir(dirPath);
    const QStringList versions = dir.entryList(
        QStringList() << "v*.md",
        QDir::Files,
        QDir::Name);

    if (versions.isEmpty()) {
        auto* browser = new QTextBrowser;
        browser->setMarkdown(QString("## 暂无说明\n\n未找到 `%1` 的文档。").arg(dirPath));
        tabs->addTab(browser, "说明");
        return;
    }

    for (const QString& versionFile : versions) {
        QFile file(dir.filePath(versionFile));
        QString markdown;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            markdown = QString::fromUtf8(file.readAll());
        } else {
            markdown = QString("## 读取失败\n\n无法读取 `%1`。").arg(file.fileName());
        }

        auto* browser = new QTextBrowser;
        browser->setOpenExternalLinks(true);
        browser->setMarkdown(markdown);
        browser->document()->setDefaultStyleSheet(R"(
            body { color: #20262d; line-height: 1.45; }
            h1 { font-size: 22px; margin-bottom: 10px; }
            h2 { font-size: 17px; margin-top: 18px; }
            h3 { font-size: 15px; margin-top: 14px; }
            code {
                background-color: #eef1f4;
                color: #111820;
                padding: 1px 4px;
            }
            pre {
                background-color: #f6f8fa;
                border: 1px solid #dfe3e8;
                padding: 8px;
            }
            table { border-collapse: collapse; }
            th, td {
                border: 1px solid #dfe3e8;
                padding: 4px 7px;
            }
        )");

        QString tabName = versionFile;
        tabName.chop(3);
        tabs->addTab(browser, tabName);
    }
}
