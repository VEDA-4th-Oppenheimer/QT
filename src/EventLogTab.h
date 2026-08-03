#pragma once
#include <QWidget>

class QTableWidget;

// EVENT LOG 탭: 시간 + 태그 배지(색상) + 소스 + 메시지
class EventLogTab : public QWidget {
    Q_OBJECT
public:
    explicit EventLogTab(QWidget *parent = nullptr);

    void appendEvent(const QString &tag, const QString &source, const QString &msg);

private:
    QTableWidget *m_table;
};
