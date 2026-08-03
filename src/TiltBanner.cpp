#include "TiltBanner.h"
#include "Theme.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

TiltBanner::TiltBanner(QWidget *parent) : QFrame(parent) {
    setStyleSheet("background:#2a1210;border-bottom:1px solid #4a1f1a;");
    setFixedHeight(38);

    auto *l = new QHBoxLayout(this);
    l->setContentsMargins(18, 0, 18, 0);
    l->setSpacing(14);

    auto *mark = new QLabel("!", this);
    mark->setFixedSize(20, 20);
    mark->setAlignment(Qt::AlignCenter);
    mark->setStyleSheet("background:#e0574a;color:#2a1210;border-radius:3px;" + Theme::mono(13, 700));

    auto *title = new QLabel(QString::fromUtf8("TILT DETECTED — 킷이 수평이 아닙니다"), this);
    title->setStyleSheet(Theme::mono(12, 700) + "color:#ff8175;letter-spacing:1px;");

    m_detail = new QLabel(this);
    m_detail->setStyleSheet("color:#d9a9a3;font-size:12px;");

    auto *dismiss = new QPushButton("DISMISS", this);
    dismiss->setStyleSheet("background:transparent;border:1px solid #6b2f28;color:#e09b93;min-height:26px;");
    connect(dismiss, &QPushButton::clicked, this, &QWidget::hide);

    l->addWidget(mark);
    l->addWidget(title);
    l->addWidget(m_detail, 1);
    l->addWidget(dismiss);
    hide();
}

void TiltBanner::update(const ImuState &imu, double tolDeg) {
    const bool level = imu.level(tolDeg);
    if (level) {
        m_wasLevel = true;
        hide();
        return;
    }
    if (m_wasLevel) emit tiltOnset(imu);
    m_wasLevel = false;

    m_detail->setText(QString::fromUtf8("MPU6050 기준 Roll %1° / Pitch %2° (허용 ±%3°). "
                              "킷을 재설치한 뒤 칼리브레이션을 다시 시작하세요.")
                          .arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1).arg(tolDeg, 0, 'f', 1));
    show();
}
