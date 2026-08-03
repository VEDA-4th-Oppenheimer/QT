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
    static const QMap<QString, QPair<QColor, QColor>> kMap = {
        {"TILT",   {QColor("#3a1a16"), QColor("#ff8175")}},
        {"RETRY",  {QColor("#2e2413"), QColor("#e2a33c")}},
        {"MQTT",   {QColor("#2e2413"), QColor("#e2a33c")}},
        {"CALIB",  {QColor("#16241d"), QColor("#6fdcab")}},
        {"SCAN",   {QColor("#152229"), QColor("#8fd9e2")}},
        {"LSD",    {QColor("#152229"), QColor("#8fd9e2")}},
        {"MATCH",  {QColor("#152229"), QColor("#8fd9e2")}},
        {"CHECK",  {QColor("#2e2413"), QColor("#e2a33c")}},
        {"EXPORT", {QColor("#16241d"), QColor("#6fdcab")}},
        {"OBJECT", {QColor("#152229"), QColor("#8fd9e2")}},
        {"MAP",    {QColor("#152229"), QColor("#8fd9e2")}},
        {"POWER",  {QColor("#1b2127"), QColor("#a9b4bd")}},
        {"LEVEL",  {QColor("#3a1a16"), QColor("#ff8175")}},
        {"RTSP",   {QColor("#152229"), QColor("#8fd9e2")}},
    };
    return kMap.value(tag, {QColor("#1b2127"), QColor("#a9b4bd")});
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
