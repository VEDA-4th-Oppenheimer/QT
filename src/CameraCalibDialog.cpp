#include "CameraCalibDialog.h"
#include "Theme.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QColor>

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

    auto *bottom = new QHBoxLayout;
    m_downloadButton = new QPushButton(QString::fromUtf8("선택 결과 다운로드"), this);
    m_downloadButton->setFixedHeight(28);
    m_downloadButton->setCursor(Qt::PointingHandCursor);
    m_downloadButton->setStyleSheet(btnCss);
    m_downloadButton->setEnabled(false);
    connect(m_downloadButton, &QPushButton::clicked, this, [this] {
        QListWidgetItem *item = m_list->currentItem();
        if (item == nullptr || !item->data(Qt::UserRole + 3).toBool()) return;

        emit downloadRequested(item->data(Qt::UserRole).toString(),
                               item->data(Qt::UserRole + 1).toString(),
                               item->data(Qt::UserRole + 2).toString());
    });
    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) {
        const bool available = current != nullptr &&
                               current->data(Qt::UserRole + 3).toBool();
        m_downloadButton->setEnabled(available);
        if (current == nullptr) {
            m_note->setText(QString::fromUtf8("다운로드할 결과를 선택하세요."));
        } else if (available) {
            m_note->setText(QString::fromUtf8("선택한 캘리브레이션 결과를 다운로드할 수 있습니다."));
        } else {
            m_note->setText(QString::fromUtf8("이 세션은 아직 다운로드할 결과가 없습니다."));
        }
    });

    bottom->addWidget(m_downloadButton);
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
    m_downloadButton->setEnabled(false);
    m_note->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::TextMuted.name()));
    for (const auto &e : entries) {
        const QString resultSize = e.resultFileBytes > 0
                                       ? QStringLiteral("%1 KB").arg((e.resultFileBytes + 1023) / 1024)
                                       : QStringLiteral("—");
        const QString resultName = e.resultFileName.isEmpty()
                                       ? QStringLiteral("calibration_result.json")
                                       : e.resultFileName;
        const QString state = e.state.isEmpty() ? QStringLiteral("-") : e.state;
        const QString availability = e.resultAvailable
                                         ? QString::fromUtf8("다운로드 가능")
                                         : QString::fromUtf8("결과 없음");
        auto *it = new QListWidgetItem(
            QString::fromUtf8("%1\n  LiDAR: %2\n  상태: %3 · 결과: %4 · %5 · %6")
                .arg(e.sessionId,
                     e.lidarFileName.isEmpty() ? QStringLiteral("-") : e.lidarFileName,
                     state,
                     resultName,
                     resultSize,
                     availability));
        it->setData(Qt::UserRole, e.sessionId);
        it->setData(Qt::UserRole + 1, e.downloadUrl);
        it->setData(Qt::UserRole + 2, e.downloadFileName);
        it->setData(Qt::UserRole + 3, e.resultAvailable);
        if (!e.resultAvailable) {
            it->setForeground(QColor(Theme::TextMuted));
        }
        m_list->addItem(it);
    }
    m_note->setText(entries.isEmpty() ? QString::fromUtf8("사용 가능한 캘리브레이션 세션이 없습니다.")
                                      : QString::fromUtf8("결과를 선택한 후 다운로드 버튼을 누르세요."));
}

void CameraCalibDialog::setErrorMessage(const QString &msg) {
    m_note->setText(msg);
    m_note->setStyleSheet(Theme::mono(10) + QString("color:%1;").arg(Theme::DangerText.name()));
}
