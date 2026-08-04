#include "EventLogTab.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QDateTime>
#include <QMap>
#include <QPair>

namespace {
QPair<QColor, QColor> tagColors(const QString &tag) {
    // 태그별 배경/글자색을 Theme:: 의미색 5종(danger/warn/ok/accent/neutral)에서
    // 매번 새로 뽑는다 — 모드가 바뀌어도(setMode) 여기서 그대로 반영된다.
    const QMap<QString, QPair<QColor, QColor>> map = {
        {"TILT",   {Theme::DangerBg, Theme::DangerText}},
        {"RETRY",  {Theme::WarnBg,   Theme::Warn}},
        {"MQTT",   {Theme::WarnBg,   Theme::Warn}},
        {"CALIB",  {Theme::OkBg,     Theme::OkBright}},
        {"SCAN",   {Theme::AccentBg, Theme::AccentBright}},
        {"LSD",    {Theme::AccentBg, Theme::AccentBright}},
        {"MATCH",  {Theme::AccentBg, Theme::AccentBright}},
        {"CHECK",  {Theme::WarnBg,   Theme::Warn}},
        {"EXPORT", {Theme::OkBg,     Theme::OkBright}},
        {"OBJECT", {Theme::AccentBg, Theme::AccentBright}},
        {"MAP",    {Theme::AccentBg, Theme::AccentBright}},
        {"POWER",  {Theme::NeutralBg, Theme::NeutralFg}},
        {"LEVEL",  {Theme::DangerBg, Theme::DangerText}},
        {"RTSP",   {Theme::AccentBg, Theme::AccentBright}},
        {"ERROR",  {Theme::DangerBg, Theme::DangerText}},
    };
    return map.value(tag, {Theme::NeutralBg, Theme::NeutralFg});
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
}
