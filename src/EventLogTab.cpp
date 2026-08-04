#include "EventLogTab.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QHash>
#include <QPair>

namespace {
// 로그가 쌓이는 만큼 행이 무한정 늘어나면(스캔 중 event/progress 만 초당 2건)
// 오래 켜둔 세션에서 메모리가 계속 불어난다. 최근 것부터 보는 화면이라
// 오래된 행은 잘라낸다.
constexpr int kMaxRows = 1000;

// 태그 -> 의미(카테고리) 매핑은 고정이라 정적 테이블로 한 번만 만들고,
// 실제 색은 호출 시점에 Theme:: 에서 꺼낸다(테마 전환 시 값이 바뀌므로).
// 이렇게 안 하고 QMap 을 함수 안에서 만들면 로그 한 줄마다 15개 항목을
// 새로 할당하게 된다.
enum class TagCat { Danger, Warn, Ok, Accent, Neutral };

TagCat tagCategory(const QString &tag) {
    static const QHash<QString, TagCat> kCat = {
        {"TILT",   TagCat::Danger},  {"LEVEL",  TagCat::Danger},
        {"ERROR",  TagCat::Danger},
        {"RETRY",  TagCat::Warn},    {"MQTT",   TagCat::Warn},
        {"CHECK",  TagCat::Warn},
        {"CALIB",  TagCat::Ok},      {"EXPORT", TagCat::Ok},
        {"SCAN",   TagCat::Accent},  {"LSD",    TagCat::Accent},
        {"MATCH",  TagCat::Accent},  {"OBJECT", TagCat::Accent},
        {"MAP",    TagCat::Accent},  {"RTSP",   TagCat::Accent},
        {"POWER",  TagCat::Neutral},
    };
    return kCat.value(tag, TagCat::Neutral);
}

QPair<QColor, QColor> tagColors(const QString &tag) {
    switch (tagCategory(tag)) {
    case TagCat::Danger:  return {Theme::DangerBg,  Theme::DangerText};
    case TagCat::Warn:    return {Theme::WarnBg,    Theme::Warn};
    case TagCat::Ok:      return {Theme::OkBg,      Theme::OkBright};
    case TagCat::Accent:  return {Theme::AccentBg,  Theme::AccentBright};
    case TagCat::Neutral: break;
    }
    return {Theme::NeutralBg, Theme::NeutralFg};
}
}

EventLogTab::EventLogTab(QWidget *parent) : QWidget(parent) {
    auto *l = new QVBoxLayout(this);
    l->setContentsMargins(10, 10, 10, 10);

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({"TIME", "TAG", "SOURCE", "MESSAGE"});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setColumnWidth(0, 150);
    m_table->setColumnWidth(1, 76);
    m_table->setColumnWidth(2, 70);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    l->addWidget(m_table);
}

void EventLogTab::appendEvent(const QString &tag, const QString &source, const QString &msg) {
    m_table->insertRow(0);
    m_table->setItem(0, 0, new QTableWidgetItem(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss")));

    auto *tagItem = new QTableWidgetItem(tag);
    const auto colors = tagColors(tag);
    tagItem->setBackground(colors.first);
    tagItem->setForeground(colors.second);
    tagItem->setTextAlignment(Qt::AlignCenter);
    m_table->setItem(0, 1, tagItem);

    m_table->setItem(0, 2, new QTableWidgetItem(source));
    m_table->setItem(0, 3, new QTableWidgetItem(msg));

    while (m_table->rowCount() > kMaxRows)
        m_table->removeRow(m_table->rowCount() - 1);
}
