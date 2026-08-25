#pragma once
#include <QDialog>
#include <QString>
#include <QVector>

class QListWidget;
class QLabel;
class QPushButton;

struct CameraCalibEntry {
    QString sessionId;
    QString lidarFileName;
    QString state;
    QString detail;
    QString resultFileName;
    QString downloadFileName;
    QString downloadUrl;
    qint64 lidarFileBytes = 0;
    qint64 resultFileBytes = 0;
    bool resultAvailable = false;
};

class CameraCalibDialog : public QDialog {
    Q_OBJECT
public:
    explicit CameraCalibDialog(QWidget *parent = nullptr);

    void setCameraInfo(const QString &host, const QString &status, const QString &currentSession);
    void setEntries(const QVector<CameraCalibEntry> &entries);
    void setErrorMessage(const QString &msg);

signals:
    void refreshRequested();
    void downloadRequested(const QString &sessionId,
                           const QString &downloadUrl,
                           const QString &downloadFileName);
    void openLocalFileRequested();

private:
    QLabel *m_statusLabel = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_note = nullptr;
    QPushButton *m_downloadButton = nullptr;
};
