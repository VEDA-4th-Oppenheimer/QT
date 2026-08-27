#include "SettingsTab.h"

#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {

constexpr int kButtonHeight = 32;

QLabel *valueLabel(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setStyleSheet(Theme::mono(11) + QStringLiteral("color:%1;").arg(Theme::TextDim2.name()));
    return label;
}

// 설정 한 묶음 = 카드 하나. 제목 줄 + 내용. 예전에는 값·체크박스·버튼이 한
// 세로줄에 쭉 나열돼 어느 버튼이 어느 설정에 붙는지가 안 보였다.
QFrame *card(QWidget *parent, const QString &title, QVBoxLayout **body) {
    auto *frame = new QFrame(parent);
    frame->setObjectName("card");
    auto *outer = new QVBoxLayout(frame);
    outer->setContentsMargins(16, 13, 16, 14);
    outer->setSpacing(11);

    auto *head = new QLabel(title, frame);
    head->setStyleSheet(Theme::mono(10, 700)
                        + QStringLiteral("color:%1;letter-spacing:2px;").arg(Theme::Accent.name()));
    outer->addWidget(head);

    *body = new QVBoxLayout;
    (*body)->setSpacing(9);
    outer->addLayout(*body);
    return frame;
}

QFormLayout *newForm() {
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(8);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    return form;
}

// 카드 안의 버튼 줄. 버튼은 글자 길이만큼만 차지하고 왼쪽으로 정렬한다 —
// 폭에 맞춰 억지로 늘리면 짧은 라벨이 텅 빈 판때기처럼 보인다.
QHBoxLayout *buttonRow() {
    auto *row = new QHBoxLayout;
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(8);
    return row;
}

void styleAction(QPushButton *button) {
    button->setFixedHeight(kButtonHeight);
    button->setCursor(Qt::PointingHandCursor);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
}

}   // namespace

SettingsTab::SettingsTab(const State &state, QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(20, 18, 20, 18);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("SETTINGS"), this);
    title->setStyleSheet(Theme::mono(13, 700) + QStringLiteral("color:%1;letter-spacing:2px;").arg(Theme::Accent.name()));
    root->addWidget(title);

    // 채널 선택 팝업 메뉴 생성 람다
    auto createChannelMenu = [this](QWidget *parent, auto signal) {
        auto *menu = new QMenu(parent);
        for (int ch = 1; ch <= 4; ++ch) {
            menu->addAction(QStringLiteral("CH%1 (센서 %2) 에 적용").arg(ch).arg(ch - 1), this, [this, signal, ch] {
                emit (this->*signal)(ch);
            });
        }
        menu->addSeparator();
        menu->addAction(QStringLiteral("파일 내 정의 채널 전체 자동 적용"), this, [this, signal] {
            emit (this->*signal)(0);
        });
        return menu;
    };

    auto addAction = [this](QHBoxLayout *row, const QString &text, auto signal) {
        auto *button = new QPushButton(text, this);
        styleAction(button);
        connect(button, &QPushButton::clicked, this, signal);
        row->addWidget(button);
    };

    // ── 화면 ────────────────────────────────────────────────────────────
    QVBoxLayout *screenBody = nullptr;
    root->addWidget(card(this, QStringLiteral("화면"), &screenBody));

    auto *themeRow = new QHBoxLayout;
    themeRow->setSpacing(0);          // 두 버튼을 붙여 세그먼트 하나처럼 보이게
    auto *themeGroup = new QButtonGroup(this);
    themeGroup->setExclusive(true);

    // 전역 QSS 에는 QPushButton:checked 규칙이 없어서 선택된 쪽이 눌린 것처럼
    // 보이지 않는다. 이 두 버튼에만 강조 규칙을 붙이고, 맞닿는 모서리는 각지게.
    const QString segBase = QStringLiteral(
        "QPushButton { background:%1; border:1px solid %2; color:%3;"
        " font-family:'JetBrains Mono','D2Coding',monospace; font-size:11px; letter-spacing:1px; }"
        "QPushButton:hover { background:%4; color:%5; }"
        "QPushButton:checked { background:%6; color:%7; border:1px solid %7; font-weight:700; }")
        .arg(Theme::PanelHead.name(), Theme::Border.name(), Theme::TextDim2.name())
        .arg(Theme::BorderSoft.name(), Theme::Text2.name())
        .arg(Theme::AccentBg.name(), Theme::AccentBright.name());

    auto addThemeButton = [&](const QString &text, Theme::Mode mode, const QString &radius) {
        auto *button = new QPushButton(text, this);
        button->setCheckable(true);
        button->setFixedSize(104, 30);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(segBase + radius);
        button->setChecked(state.theme == mode);
        themeGroup->addButton(button);
        connect(button, &QPushButton::clicked, this, [this, mode] { emit themeChangeRequested(mode); });
        themeRow->addWidget(button);
        return button;
    };
    m_themeBlack = addThemeButton(QStringLiteral("블랙"), Theme::Mode::Developer,
                                  QStringLiteral("QPushButton{border-top-left-radius:4px;border-bottom-left-radius:4px;}"));
    m_themeWhite = addThemeButton(QStringLiteral("화이트"), Theme::Mode::User,
                                  QStringLiteral("QPushButton{border-top-right-radius:4px;border-bottom-right-radius:4px;border-left:none;}"));
    themeRow->addStretch(1);

    auto *screenForm = newForm();
    m_topView = valueLabel(this);
    screenForm->addRow(QStringLiteral("테마"), themeRow);
    screenForm->addRow(QStringLiteral("TOP-VIEW"), m_topView);
    screenBody->addLayout(screenForm);

    // ── 카메라 ──────────────────────────────────────────────────────────
    QVBoxLayout *cameraBody = nullptr;
    root->addWidget(card(this, QStringLiteral("카메라"), &cameraBody));

    auto *cameraForm = newForm();
    m_camera = valueLabel(this);
    m_height = valueLabel(this);
    cameraForm->addRow(QStringLiteral("연결"), m_camera);
    cameraForm->addRow(QStringLiteral("센서 높이"), m_height);
    cameraBody->addLayout(cameraForm);

    auto *cameraButtons = buttonRow();
    addAction(cameraButtons, QStringLiteral("카메라 설정"), &SettingsTab::cameraSettingsRequested);
    addAction(cameraButtons, QStringLiteral("CCTV 재연결"), &SettingsTab::cameraReconnectRequested);
    addAction(cameraButtons, QStringLiteral("센서 높이 변경"), &SettingsTab::sensorHeightRequested);

    auto *btnLoadIntrinsic = new QPushButton(QStringLiteral("내부 파라미터 ▾"), this);
    styleAction(btnLoadIntrinsic);
    btnLoadIntrinsic->setMenu(createChannelMenu(btnLoadIntrinsic, &SettingsTab::loadIntrinsicProfileRequested));
    cameraButtons->addWidget(btnLoadIntrinsic);
    cameraButtons->addStretch(1);
    cameraBody->addLayout(cameraButtons);

    // ── 캘리브레이션 ────────────────────────────────────────────────────
    QVBoxLayout *calibBody = nullptr;
    root->addWidget(card(this, QStringLiteral("캘리브레이션"), &calibBody));

    auto *calibForm = newForm();
    m_calibLabel = valueLabel(this);
    calibForm->addRow(QStringLiteral("현재 RT"), m_calibLabel);
    calibBody->addLayout(calibForm);

    m_manualCalib = new QCheckBox(QStringLiteral("Manual RT 사용 (해제 시 Automatic RT)"), this);
    m_manualCalib->setCursor(Qt::PointingHandCursor);
    m_manualCalib->setChecked(state.manualCalib);
    connect(m_manualCalib, &QCheckBox::toggled, this, [this](bool checked) {
        setCalibMode(checked);
        emit calibModeToggled(checked);
    });
    calibBody->addWidget(m_manualCalib);

    auto *calibButtons = buttonRow();
    auto *btnAttachManualRt = new QPushButton(QStringLiteral("Manual RT 첨부 ▾"), this);
    styleAction(btnAttachManualRt);
    btnAttachManualRt->setMenu(createChannelMenu(btnAttachManualRt, &SettingsTab::loadManualRtRequested));
    calibButtons->addWidget(btnAttachManualRt);
    calibButtons->addStretch(1);
    calibBody->addLayout(calibButtons);

    // ── 진단 ────────────────────────────────────────────────────────────
    QVBoxLayout *diagBody = nullptr;
    root->addWidget(card(this, QStringLiteral("진단"), &diagBody));

    m_demo = new QCheckBox(QStringLiteral("Demo Mode (실기 대신 내장 시뮬레이터로 구동)"), this);
    m_demo->setCursor(Qt::PointingHandCursor);
    connect(m_demo, &QCheckBox::toggled, this, &SettingsTab::demoModeToggled);
    diagBody->addWidget(m_demo);

    root->addStretch(1);

    // ── 계정: 로그아웃만 오른쪽 끝에
    auto *bottom = new QHBoxLayout;
    bottom->addStretch(1);
    auto *logout = new QPushButton(QStringLiteral("로그아웃"), this);
    styleAction(logout);
    logout->setMinimumWidth(110);
    connect(logout, &QPushButton::clicked, this, &SettingsTab::logoutRequested);
    bottom->addWidget(logout);
    root->addLayout(bottom);

    setDemoMode(state.demoMode);
    setTopViewDetached(state.topViewDetached);
    setSensorHeight(state.sensorHeightMm);
    setCameraSummary(state.cameraHost, state.cameraChannels);
    setCalibMode(state.manualCalib);
}

void SettingsTab::setDemoMode(bool demo) {
    if (m_demo == nullptr) return;
    const QSignalBlocker blocker(m_demo);
    m_demo->setChecked(demo);
}

void SettingsTab::setTopViewDetached(bool detached) {
    if (m_topView != nullptr) {
        m_topView->setText(detached ? QStringLiteral("별도 창") : QStringLiteral("대시보드"));
    }
}

void SettingsTab::setSensorHeight(int mm) {
    if (m_height != nullptr) {
        m_height->setText(QStringLiteral("%1 m (%2 mm)").arg(mm / 1000.0, 0, 'f', 3).arg(mm));
    }
}

void SettingsTab::setCameraSummary(const QString &host, int channels) {
    if (m_camera != nullptr) {
        m_camera->setText(host.isEmpty()
                              ? QStringLiteral("미설정")
                              : QStringLiteral("%1 · %2채널").arg(host).arg(channels));
    }
}

void SettingsTab::setCalibMode(bool manual) {
    if (m_manualCalib != nullptr) {
        const QSignalBlocker blocker(m_manualCalib);
        m_manualCalib->setChecked(manual);
    }
    if (m_calibLabel != nullptr) {
        m_calibLabel->setText(manual ? QStringLiteral("Manual RT (차루코 실측)") : QStringLiteral("Automatic RT (기구 기하 기준)"));
    }
}
