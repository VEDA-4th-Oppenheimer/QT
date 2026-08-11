#include "DevicesTab.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QFrame>
#include <QTableWidget>
#include <QHeaderView>

namespace {
struct StaticDevice { const char *name, *desc, *value; };
const StaticDevice kStatic[2] = {
    {"STM32 + DRV8825", "Pan/Tilt 2축 · 카메라와 동일 천장 스택 마운트", "IDLE · pan 0..180° / 1.0°step"},
    {"RPi 4B",           "통합 데몬(C, epoll) + Mosquitto 브로커",       "RUN · 42.3 °C"},
};

QFrame *deviceCard(QWidget *parent, const QString &name, const QString &desc,
                    QLabel **dot, QLabel **value) {
    auto *card = new QFrame(parent);
    card->setObjectName("card");
    auto *l = new QVBoxLayout(card);
    l->setContentsMargins(11, 9, 11, 9);
    l->setSpacing(5);

    auto *top = new QHBoxLayout;
    *dot = new QLabel(QChar(0x25CF), card);
    (*dot)->setStyleSheet(QString("color:%1;font-size:8px;").arg(Theme::Ok.name()));
    auto *nameLabel = new QLabel(name, card);
    nameLabel->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;").arg(Theme::Text2.name()));
    top->addWidget(*dot);
    top->addWidget(nameLabel);
    top->addStretch(1);
    l->addLayout(top);

    auto *descLabel = new QLabel(desc, card);
    descLabel->setStyleSheet(QString("color:%1;font-size:11px;").arg(Theme::TextMuted.name()));
    descLabel->setWordWrap(true);
    l->addWidget(descLabel);

    *value = new QLabel(card);
    (*value)->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg(Theme::Text3.name()));
    l->addWidget(*value);
    return card;
}
}

DevicesTab::DevicesTab(QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto *cards = new QGridLayout;
    cards->setSpacing(10);

    cards->addWidget(deviceCard(this, "MPU6050", QString::fromUtf8("수평 게이트 판정 (/dev/imu, I²C)"),
                                 &m_mpuDot, &m_mpuValue), 0, 0);
    cards->addWidget(deviceCard(this, "TOFSense-F2D", QString::fromUtf8("1D LiDAR · pan-tilt 격자 스캔 (100 Hz)"),
                                 &m_tofDot, &m_tofValue), 0, 1);
    QLabel *dummyDot; QLabel *dummyValue;
    for (int i = 0; i < 2; ++i) {
        auto *card = deviceCard(this, kStatic[i].name, kStatic[i].desc, &dummyDot, &dummyValue);
        dummyValue->setText(kStatic[i].value);
        cards->addWidget(card, 0, 2 + i);
    }
    root->addLayout(cards);

    auto *tableHead = new QHBoxLayout;
    auto *th = new QLabel("MQTT TOPICS", this);
    th->setStyleSheet(Theme::mono(11, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));
    auto *qos = new QLabel(QString::fromUtf8("QoS 1 · keepalive 30 s"), this);
    qos->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextFaint.name()));
    tableHead->addWidget(th);
    tableHead->addStretch(1);
    tableHead->addWidget(qos);
    root->addLayout(tableHead);

    // 영상은 MQTT가 아니라 RTSP 직결(RtspSource, 대시보드 타일 참고). 아래는
    // RPi develop 브랜치 실구현 기준 토픽(kit_id 세그먼트 없음 — Models.h 참고).
    struct Row { const char *topic, *rate, *desc, *state; };
    const Row rows[9] = {
        {"adts/cmd/scan",       "on-demand", "Qt -> 데몬: 스캔 시작 (retain 금지)",     "TX"},
        {"adts/cmd/stop",       "on-demand", "Qt -> 데몬: 스캔 중단",                    "TX"},
        {"adts/cmd/home",       "on-demand", "Qt -> 데몬: 스캔 없이 홈만 수행 (IDLE 에서만)", "TX"},
        {"adts/cmd/disarm",     "on-demand", "Qt -> 데몬: 안전정지",                     "TX"},
        {"adts/cmd/rearm",      "on-demand", "Qt -> 데몬: DISARM 해제 (계약 외 확장)",   "TX"},
        {"adts/state/daemon",   "5s + 변경시","데몬 -> Qt: FSM·링크·IMU (retained, LWT)", "RX"},
        {"adts/state/scan",     "on-demand", "데몬 -> Qt: 스캔 결과 파일 경로 (retained)","RX"},
        {"adts/event/progress", "~2 Hz",     "데몬 -> Qt: 진행률 (QoS0, 유실 가정)",      "RX"},
        {"adts/event/error",    "on-demand", "데몬 -> Qt: 오류 코드/메시지",             "RX"},
    };

    constexpr int kRowCount = int(sizeof(rows) / sizeof(rows[0]));
    m_table = new QTableWidget(kRowCount, 4, this);
    m_table->setHorizontalHeaderLabels({"TOPIC", "RATE", "DESC", "STATE"});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 280);
    m_table->setColumnWidth(1, 90);
    m_table->setColumnWidth(2, 420);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    auto stateColor = [](const QString &s) {
        if (s == "LOST") return Theme::DangerText;
        if (s == "TODO") return Theme::Warn;
        if (s == "TX")   return Theme::AccentBright;
        return Theme::Ok;
    };
    for (int r = 0; r < kRowCount; ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(rows[r].topic)));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(rows[r].rate)));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(rows[r].desc)));
        auto *stateItem = new QTableWidgetItem(QString::fromUtf8(rows[r].state));
        stateItem->setForeground(stateColor(rows[r].state));
        m_table->setItem(r, 3, stateItem);
    }
    root->addWidget(m_table, 1);

    setImu({});
    setScanProgress({});
}

void DevicesTab::setImu(const ImuState &imu) {
    if (!imu.valid) {
        m_mpuDot->setStyleSheet(QString("color:%1;font-size:8px;").arg(Theme::TextFaint.name()));
        m_mpuValue->setText("N/A · 미구현");
        m_mpuValue->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg(Theme::TextFaint.name()));
        return;
    }
    const bool level = imu.level();
    m_mpuDot->setStyleSheet(QString("color:%1;font-size:8px;").arg((level ? Theme::Ok : Theme::Warn).name()));
    m_mpuValue->setText(level
        ? QString("LEVEL · R %1° / P %2°").arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1)
        : QString("TILT · R %1° / P %2°").arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    m_mpuValue->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg((level ? Theme::OkBright : Theme::DangerText).name()));
}

void DevicesTab::setScanProgress(const ScanProgress &p) {
    const int pct = p.expected > 0 ? int(qint64(p.points) * 100 / p.expected) : p.percent;
    m_tofValue->setText(QString("SCANNING · %1 / %2 pts (%3%)")
                             .arg(p.points).arg(p.expected).arg(pct));
}

void DevicesTab::setChannelOnline(int /*channel*/, bool /*online*/) {
    // 영상은 RTSP 직결이라 이 MQTT 토픽 테이블과는 무관하다. 채널 상태는
    // 대시보드 탭의 CameraTile 에 이미 표시되므로 여기서는 별도 처리하지 않는다.
}
