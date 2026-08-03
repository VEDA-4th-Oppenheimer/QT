#pragma once
#include <QWidget>
#include "Models.h"

class QTableWidget;

// RGB-D DATASET 탭: 캡처 세트 테이블 + 미리보기 패널.
// datasets/<set>/meta.json 을 스캔하고, 없으면 데모 예시 6건으로 대체한다.
class DatasetTab : public QWidget {
    Q_OBJECT
public:
    explicit DatasetTab(QWidget *parent = nullptr);

signals:
    void exportRequested();

private:
    void loadRows();
    QVector<DatasetEntry> scanFilesystem() const;
    QVector<DatasetEntry> demoRows() const;

    QTableWidget *m_table;
};
