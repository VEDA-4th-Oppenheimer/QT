#include "ScanListDialog.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

ScanListDialog::ScanListDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString::fromUtf8("스캔 파일 선택 (Point Cloud)"));
    setMinimumSize(520, 560);
    setStyleSheet(QString("QDialog { background:%1; }").arg(Theme::Panel.name()));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QString::fromUtf8("스캔 파일 (Point Cloud .pcd)"), this);
    title->setStyleSheet(Theme::mono(12, 700) + QString("color:%1;letter-spacing:1px;").arg(Theme::Accent.name()));

    const QString btnCss = QString(
        "QPushButton { background:%1; border:1px solid %2; border-radius:4px; color:%3;"
        " font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px;"
        " padding:4px 10px; font-weight:600; }"
        "QPushButton:hover { background:%4; color:%5; border-color:%5; }")
        .arg(Theme::PanelHead.name(), Theme::Border.name(), Theme::Text2.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name());

    auto *btnLocal = new QPushButton(QString::fromUtf8("📁 로컬 파일 열기..."), this);
    btnLocal->setFixedHeight(28);
    btnLocal->setCursor(Qt::PointingHandCursor);
    btnLocal->setStyleSheet(btnCss);
    connect(btnLocal, &QPushButton::clicked, this, [this] {
        accept();
        emit openLocalFileRequested();
    });

    auto *btnRefresh = new QPushButton(QString::fromUtf8("새로고침"), this);
    btnRefresh->setFixedHeight(28);
    btnRefresh->setCursor(Qt::PointingHandCursor);
    btnRefresh->setStyleSheet(btnCss);
    connect(btnRefresh, &QPushButton::clicked, this, &ScanListDialog::refreshRequested);

    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(btnLocal);
    head->addWidget(btnRefresh);
    root->addLayout(head);

    m_note = new QLabel(QString::fromUtf8("서버 및 로컬 스캔 목록을 불러오는 중…"), this);
    m_note->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextMuted.name()));
    root->addWidget(m_note);

    m_list = new QListWidget(this);
    m_list->setStyleSheet(QString(
        "QListWidget { background:%1; border:1px solid %2; border-radius:4px;"
        " font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; color:%3; }"
        "QListWidget::item { padding:8px 10px; border-bottom:1px solid %4; }"
        "QListWidget::item:selected { background:%5; color:%6; font-weight:bold; }"
        "QListWidget::item:hover { background:%4; }")
        .arg(Theme::MapBg.name(), Theme::Border.name(), Theme::Text3.name())
        .arg(Theme::BorderRow.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name()));
    root->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *it) {
        if (!it) return;
        emit scanChosen(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString());
        accept();
    });
    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
        if (!it) return;
        emit scanChosen(it->data(Qt::UserRole).toString(), it->data(Qt::UserRole + 1).toString());
        accept();
    });

    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    auto *btnClose = new QPushButton(QString::fromUtf8("닫기"), this);
    btnClose->setFixedHeight(28);
    btnClose->setCursor(Qt::PointingHandCursor);
    btnClose->setStyleSheet(btnCss);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::reject);
    bottom->addWidget(btnClose);
    root->addLayout(bottom);
}

void ScanListDialog::setEntries(const QVector<ScanEntry> &entries, const QString &note) {
    m_list->clear();
    for (const ScanEntry &e : entries) {
        const QString size = e.size > 0 ? QStringLiteral("%1 KB").arg(e.size / 1024) : QStringLiteral("—");
        const QString when = e.mtime.isValid() ? e.mtime.toString("yyyy-MM-dd HH:mm") : QStringLiteral("");
        auto *it = new QListWidgetItem(QStringLiteral("%1\n  %2 · %3 · %4")
                                           .arg(e.name, size, when,
                                                e.isLocal() ? QString::fromUtf8("로컬") : QString::fromUtf8("서버")));
        it->setData(Qt::UserRole, e.name);
        it->setData(Qt::UserRole + 1, e.localPath);
        m_list->addItem(it);
    }
    m_note->setText(entries.isEmpty() ? QString::fromUtf8("스캔 파일이 없습니다 — %1").arg(note) : note);
}
