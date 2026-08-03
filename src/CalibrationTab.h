#pragma once
#include <QWidget>
#include "Models.h"

class QPlainTextEdit;
class QLabel;
class QProgressBar;

// CALIBRATION 탭: 8단계 파이프라인 + 로그 + EXTRINSIC RT 그리드 + QUALITY 카드
class CalibrationTab : public QWidget {
    Q_OBJECT
public:
    explicit CalibrationTab(QWidget *parent = nullptr);

    void setCalib(const CalibState &c);
    void appendLog(const QString &tag, const QString &msg);

signals:
    void exportRequested(const QString &format);   // "json" / "yaml"

private:
    QLabel *m_stepBadge[8];
    QLabel *m_stepState[8];
    QPlainTextEdit *m_log;

    QLabel *m_nccValue;
    QProgressBar *m_nccBar;
    QLabel *m_reprojValue, *m_inlierValue, *m_retryValue;
};
