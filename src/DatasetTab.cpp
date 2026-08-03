#include "DatasetTab.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFrame>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QLocale>

namespace {
// 시안의 45° 스트라이프 플레이스홀더
class StripedPlaceholder : public QFrame {
public:
    StripedPlaceholder(const QString &text, QWidget *parent, const QColor &bg = QColor("#12171b"))
        : QFrame(parent), m_text(text), m_bg(bg) {
        setFixedHeight(170);
        setStyleSheet("border:1px solid #1c242b;border-radius:4px;");
    }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(rect(), QColor("#0f1418"));
        p.setPen(QPen(m_bg, 9));
        for (int x = -height(); x < width(); x += 18) p.drawLine(x, height(), x + height(), 0);
        QFont f("JetBrains Mono"); f.setPixelSize(10); p.setFont(f);
        p.setPen(Theme::TextFaint2);
        p.drawText(rect(), Qt::AlignCenter, m_text);
    }
private:
    QString m_text;
    QColor m_bg;
};
}

DatasetTab::DatasetTab(QWidget *parent) : QWidget(parent) {
    auto *root = new QHBoxLayout(this);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(10);

    auto *left = new QVBoxLayout;
    auto *head = new QHBoxLayout;
    auto *title = new QLabel("CAPTURE SETS", this);
    title->setStyleSheet(Theme::mono(11, 700) + "color:#3fbfcc;letter-spacing:1px;");
    auto *exportBtn = new QPushButton("EXPORT SET", this);
    exportBtn->setObjectName("accent");
    connect(exportBtn, &QPushButton::clicked, this, &DatasetTab::exportRequested);
    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(exportBtn);
    left->addLayout(head);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels({"SET ID", "CH", "POINTS", "NCC", "SIZE", "CAPTURED"});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 150);
    m_table->setColumnWidth(1, 74);
    m_table->setColumnWidth(2, 96);
    m_table->setColumnWidth(3, 80);
    m_table->setColumnWidth(4, 104);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    left->addWidget(m_table, 1);

    auto *right = new QVBoxLayout;
    right->setSpacing(10);
    auto *rgbLabel = new QLabel("RGB FRAME 1920×1080", this);
    rgbLabel->setStyleSheet(Theme::mono(10) + "color:#5f6c78;letter-spacing:1px;");
    right->addWidget(rgbLabel);
    right->addWidget(new StripedPlaceholder("RGB FRAME 1920×1080", this));

    auto *mapLabel = new QLabel("INDOOR 3D POINT MAP", this);
    mapLabel->setStyleSheet(Theme::mono(10) + "color:#5f6c78;letter-spacing:1px;");
    right->addWidget(mapLabel);
    auto *pointMap = new StripedPlaceholder("INDOOR 3D POINT MAP", this, QColor("#080b0e"));
    right->addWidget(pointMap);

    auto *aug = new QLabel(QString::fromUtf8("Augmentation / RT perturb ×12"), this);
    aug->setStyleSheet(Theme::mono(10) + "color:#5f6c78;");
    right->addWidget(aug);
    right->addStretch(1);

    auto *rightWrap = new QWidget(this);
    rightWrap->setLayout(right);
    rightWrap->setFixedWidth(320);

    auto *leftWrap = new QWidget(this);
    leftWrap->setLayout(left);

    root->addWidget(leftWrap, 1);
    root->addWidget(rightWrap);

    loadRows();
}

void DatasetTab::loadRows() {
    QVector<DatasetEntry> rows = scanFilesystem();
    if (rows.isEmpty()) rows = demoRows();

    m_table->setRowCount(rows.size());
    for (int r = 0; r < rows.size(); ++r) {
        const auto &d = rows[r];
        m_table->setItem(r, 0, new QTableWidgetItem(d.id));
        m_table->setItem(r, 1, new QTableWidgetItem(d.ch));
        m_table->setItem(r, 2, new QTableWidgetItem(d.points));
        auto *nccItem = new QTableWidgetItem(d.ncc);
        nccItem->setForeground(d.ncc.toDouble() >= 0.72 ? Theme::OkBright : Theme::DangerText);
        m_table->setItem(r, 3, nccItem);
        m_table->setItem(r, 4, new QTableWidgetItem(d.size));
        m_table->setItem(r, 5, new QTableWidgetItem(d.capturedAt));
    }
}

QVector<DatasetEntry> DatasetTab::scanFilesystem() const {
    QVector<DatasetEntry> out;
    QDir dir(QStringLiteral("datasets"));
    if (!dir.exists()) return out;
    for (const auto &entry : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile meta(entry.filePath() + "/meta.json");
        if (!meta.open(QIODevice::ReadOnly)) continue;
        const auto o = QJsonDocument::fromJson(meta.readAll()).object();
        DatasetEntry d;
        d.id          = entry.fileName();
        d.ch          = o.value("ch").toString();
        d.points      = QLocale().toString(o.value("points").toInt());
        d.ncc         = QString::number(o.value("ncc").toDouble(), 'f', 3);
        d.size        = QString("%1 MB").arg(o.value("size_mb").toInt());
        d.capturedAt  = o.value("captured").toString();
        out.push_back(d);
    }
    return out;
}

QVector<DatasetEntry> DatasetTab::demoRows() const {
    return {
        {"set_20260803_0941", "CH1", "18,432", "0.813", "412 MB", "2026-08-03 09:41"},
        {"set_20260803_0938", "CH1", "17,980", "0.684", "398 MB", "2026-08-03 09:38"},
        {"set_20260802_1712", "CH2", "19,104", "0.791", "431 MB", "2026-08-02 17:12"},
        {"set_20260802_1544", "CH3", "16,220", "0.744", "362 MB", "2026-08-02 15:44"},
        {"set_20260801_1103", "CH2", "18,776", "0.802", "420 MB", "2026-08-01 11:03"},
        {"set_20260731_1620", "CH4", "15,431", "0.652", "347 MB", "2026-07-31 16:20"},
    };
}
