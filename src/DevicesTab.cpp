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
    {"STM32 + DRV8825", "Pan/Tilt 2축 · 카메라와 동일 천장 마운트", "IDLE · θ 0..360° / 0.45°step"},
    {"RPi 4B",           "Serial parser / MQTT bridge",             "RUN · 42.3 °C"},
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
    (*dot)->setStyleSheet("color:#4bbd85;font-size:8px;");
    auto *nameLabel = new QLabel(name, card);
    nameLabel->setStyleSheet(Theme::mono(11, 700) + "color:#dbe2e8;");
    top->addWidget(*dot);
    top->addWidget(nameLabel);
    top->addStretch(1);
    l->addLayout(top);

    auto *descLabel = new QLabel(desc, card);
    descLabel->setStyleSheet("color:#7b8792;font-size:11px;");
    descLabel->setWordWrap(true);
    l->addWidget(descLabel);

    *value = new QLabel(card);
    (*value)->setStyleSheet(Theme::mono(12) + "color:#c7d1da;");
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

    cards->addWidget(deviceCard(this, "MPU6050", QString::fromUtf8("킷 수평 감지 (I²C, 100 Hz)"),
                                 &m_mpuDot, &m_mpuValue), 0, 0);
    cards->addWidget(deviceCard(this, "TOFSense-F2P", QString::fromUtf8("1D LiDAR · 실내 최대 25 m"),
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
    th->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    auto *qos = new QLabel(QString::fromUtf8("QoS 1 · keepalive 30 s"), this);
    qos->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
    tableHead->addWidget(th);
    tableHead->addStretch(1);
    tableHead->addWidget(qos);
    root->addLayout(tableHead);

    struct Row { const char *topic, *rate, *desc, *state; };
    const Row rows[9] = {
        {"cctv/ch1/h264",    "24.9 fps",  "CH1 영상 스트림 (H.264 payload)", "RX"},
        {"cctv/ch2/h264",    "25.0 fps",  "CH2 영상 스트림",                  "RX"},
        {"cctv/ch3/h264",    "25.0 fps",  "CH3 영상 스트림",                  "RX"},
        {"cctv/ch4/h264",    "—",         "CH4 영상 스트림 — 브로커 응답 없음", "LOST"},
        {"wiseai/+/objects", "10 Hz",     "WiseAI 감지 BBox + class",         "RX"},
        {"kit/lidar/scan",   "burst",     "360° grid scan point cloud",       "RX"},
        {"kit/imu/level",    "20 Hz",     "MPU6050 roll / pitch",             "RX"},
        {"kit/cmd/power",    "on-demand", "킷 전원 제어 (Qt -> STM32)",       "TX"},
        {"kit/cmd/rescan",   "on-demand", "자동 재시도 재스캔 명령",          "TX"},
    };

    m_table = new QTableWidget(9, 4, this);
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
        if (s == "TX") return Theme::Warn;
        return Theme::Ok;
    };
    for (int r = 0; r < 9; ++r) {
        m_table->setItem(r, 0, new QTableWidgetItem(QString::fromUtf8(rows[r].topic)));
        m_table->setItem(r, 1, new QTableWidgetItem(QString::fromUtf8(rows[r].rate)));
        m_table->setItem(r, 2, new QTableWidgetItem(QString::fromUtf8(rows[r].desc)));
        auto *stateItem = new QTableWidgetItem(QString::fromUtf8(rows[r].state));
        stateItem->setForeground(stateColor(rows[r].state));
        m_table->setItem(r, 3, stateItem);
        if (QString::fromUtf8(rows[r].topic) == "cctv/ch4/h264") m_ch4Row = r;
    }
    root->addWidget(m_table, 1);

    setImu({});
    setCalib({});
}

void DevicesTab::setImu(const ImuState &imu) {
    const bool level = imu.level();
    m_mpuDot->setStyleSheet(QString("color:%1;font-size:8px;").arg(level ? "#4bbd85" : "#e2a33c"));
    m_mpuValue->setText(level
        ? QString("LEVEL · R %1° / P %2°").arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1)
        : QString("TILT · R %1° / P %2°").arg(imu.roll, 0, 'f', 1).arg(imu.pitch, 0, 'f', 1));
    m_mpuValue->setStyleSheet(Theme::mono(12) + QString("color:%1;").arg(level ? "#6fdcab" : "#ff8175"));
}

void DevicesTab::setCalib(const CalibState &c) {
    m_tofValue->setText(QString("READY · %1 pts / %2% coverage")
                             .arg(c.scanPoints).arg(int(c.coverage * 100)));
}

void DevicesTab::setChannelOnline(int channel, bool online) {
    if (channel != 4 || m_ch4Row < 0) return;
    auto *item = m_table->item(m_ch4Row, 3);
    if (!item) return;
    item->setText(online ? "RX" : "LOST");
    item->setForeground(online ? Theme::Ok : Theme::DangerText);
    m_table->item(m_ch4Row, 1)->setText(online ? "25.0 fps" : QString::fromUtf8("—"));
}
