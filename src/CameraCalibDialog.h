#pragma once
#include <QDialog>
#include <QString>
#include <QVector>

class QListWidget;
class QLabel;
class QPushButton;

struct CameraCalibEntry {
    QString fileName;
    QString sessionId;
    qint64  bytes = 0;
    bool    processed = false;
    bool    queued = false;
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
    void sessionChosen(const QString &sessionId, const QString &fileName);
    void openLocalFileRequested();

private:
    QLabel *m_statusLabel = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_note = nullptr;
};
