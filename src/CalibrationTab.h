#pragma once
#include <QWidget>
#include "Models.h"

class QPlainTextEdit;
class QLabel;
class QProgressBar;

// CALIBRATION 탭: 8단계 파이프라인 + 로그 + EXTRINSIC(translation/quaternion) + QUALITY 카드.
// "02. Point Cloud 이후 Camera Automatic Calibration" 문서의 실제 파이프라인/스키마를 따른다.
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

    QLabel *m_translationCell[3];
    QLabel *m_quatCell[4];

    QLabel *m_edgeRmseValue;
    QProgressBar *m_inlierBar;
    QLabel *m_inlierValue, *m_nccValue, *m_retryValue, *m_statusValue;
};
