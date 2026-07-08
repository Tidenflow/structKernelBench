#pragma once

#include <QDialog>
#include <QString>

class QTabWidget;

class BackendInfoDialog : public QDialog {
public:
    BackendInfoDialog(const QString& kernelKey,
                      const QString& backendKey,
                      QWidget* parent = nullptr);

private:
    QString titleForBackend(const QString& kernelKey,
                            const QString& backendKey) const;
    QString backendDisplayName(const QString& backendKey) const;
    void loadVersionTabs(QTabWidget* tabs,
                         const QString& kernelKey,
                         const QString& backendKey);
};
