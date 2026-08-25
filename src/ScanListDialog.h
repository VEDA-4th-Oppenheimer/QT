#pragma once
#include <QDialog>
#include <QVector>
#include "ScanFetcher.h"

class QListWidget;
class QLabel;
class QPushButton;

class ScanListDialog : public QDialog {
    Q_OBJECT
public:
    explicit ScanListDialog(QWidget *parent = nullptr);

    void setEntries(const QVector<ScanEntry> &entries, const QString &note);

signals:
    void refreshRequested();
    void scanChosen(const QString &name, const QString &localPath);
    void openLocalFileRequested();

private:
    QListWidget *m_list = nullptr;
    QLabel *m_note = nullptr;
};
