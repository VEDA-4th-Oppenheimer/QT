#include "CameraCalibDialog.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

CameraCalibDialog::CameraCalibDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(QString::fromUtf8("CCTV 원격 캘리브레이션 결과 (RT) 선택"));
    setMinimumSize(540, 580);
    setStyleSheet(QString("QDialog { background:%1; }").arg(Theme::Panel.name()));

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(10);

    auto *head = new QHBoxLayout;
    auto *title = new QLabel(QString::fromUtf8("CCTV 원격 캘리브레이션 RT"), this);
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
    connect(btnRefresh, &QPushButton::clicked, this, &CameraCalibDialog::refreshRequested);

    head->addWidget(title);
    head->addStretch(1);
    head->addWidget(btnLocal);
    head->addWidget(btnRefresh);
    root->addLayout(head);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet(Theme::mono(11) + QString("color:%1; background:%2; padding:8px; border:1px solid %3; border-radius:4px;")
                                     .arg(Theme::Text2.name(), Theme::MapBg.name(), Theme::Border.name()));
    root->addWidget(m_statusLabel);

    m_note = new QLabel(QString::fromUtf8("카메라 세션 목록을 불러오는 중…"), this);
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

    auto handleSelect = [this](QListWidgetItem *it) {
        if (!it) return;
        const QString sessionId = it->data(Qt::UserRole).toString();
        const QString fileName = it->data(Qt::UserRole + 1).toString();
        emit sessionChosen(sessionId, fileName);
        accept();
    };

    connect(m_list, &QListWidget::itemActivated, this, handleSelect);
    connect(m_list, &QListWidget::itemClicked, this, handleSelect);

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

void CameraCalibDialog::setCameraInfo(const QString &host, const QString &status, const QString &currentSession) {
    m_statusLabel->setText(QString::fromUtf8("카메라 IP: %1 · 상태: %2\n최근 세션: %3")
                               .arg(host, status.isEmpty() ? QStringLiteral("-") : status,
                                    currentSession.isEmpty() ? QStringLiteral("-") : currentSession));
}

void CameraCalibDialog::setEntries(const QVector<CameraCalibEntry> &entries) {
    m_list->clear();
    for (const auto &e : entries) {
        const QString size = e.bytes > 0 ? QStringLiteral("%1 KB").arg(e.bytes / 1024) : QStringLiteral("—");
        const QString state = e.processed ? QString::fromUtf8("완료됨 (RT 생성)") : (e.queued ? QString::fromUtf8("처리 중") : QString::fromUtf8("대기"));
        auto *it = new QListWidgetItem(QStringLiteral("%1\n  세션: %2 · %3 · %4")
                                           .arg(e.fileName, e.sessionId, size, state));
        it->setData(Qt::UserRole, e.sessionId);
        it->setData(Qt::UserRole + 1, e.fileName);
        m_list->addItem(it);
    }
    m_note->setText(entries.isEmpty() ? QString::fromUtf8("사용 가능한 캘리브레이션 세션이 없습니다.")
                                      : QString::fromUtf8("목록에서 적용할 세션 항목을 클릭하세요."));
}

void CameraCalibDialog::setErrorMessage(const QString &msg) {
    m_note->setText(msg);
    m_note->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::DangerText.name()));
}
