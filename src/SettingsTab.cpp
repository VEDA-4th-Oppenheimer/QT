#include "SettingsTab.h"

#include "Theme.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace {
QLabel *valueLabel(QWidget *parent) {
    auto *label = new QLabel(parent);
    label->setStyleSheet(Theme::mono(11) + QStringLiteral("color:%1;").arg(Theme::TextDim2.name()));
    return label;
}
}

SettingsTab::SettingsTab(const State &state, QWidget *parent) : QWidget(parent) {
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(16, 16, 16, 16);
    root->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("SETTINGS"), this);
    title->setStyleSheet(Theme::mono(13, 700) + QStringLiteral("color:%1;letter-spacing:2px;").arg(Theme::Accent.name()));
    root->addWidget(title);

    auto *form = new QFormLayout;
    form->setHorizontalSpacing(24);
    form->setVerticalSpacing(10);
    m_theme = valueLabel(this);
    m_camera = valueLabel(this);
    m_height = valueLabel(this);
    m_topView = valueLabel(this);
    m_calibLabel = valueLabel(this);
    form->addRow(QStringLiteral("테마"), m_theme);
    form->addRow(QStringLiteral("카메라"), m_camera);
    form->addRow(QStringLiteral("센서 높이"), m_height);
    form->addRow(QStringLiteral("TOP-VIEW"), m_topView);
    form->addRow(QStringLiteral("캘리브레이션 RT"), m_calibLabel);
    root->addLayout(form);

    m_demo = new QCheckBox(QStringLiteral("Demo Mode"), this);
    connect(m_demo, &QCheckBox::toggled, this, &SettingsTab::demoModeToggled);
    root->addWidget(m_demo);

    // 캘리브레이션 RT 모드 전환 스위치
    m_manualCalib = new QCheckBox(QStringLiteral("Manual RT 사용 (체크 해제 시 Automatic RT 사용)"), this);
    m_manualCalib->setChecked(state.manualCalib);
    connect(m_manualCalib, &QCheckBox::toggled, this, [this](bool checked) {
        setCalibMode(checked);
        emit calibModeToggled(checked);
    });
    root->addWidget(m_manualCalib);

    auto *buttons = new QHBoxLayout;
    auto addButton = [this, buttons](const QString &text, auto signal) {
        auto *button = new QPushButton(text, this);
        button->setFixedHeight(30);
        connect(button, &QPushButton::clicked, this, signal);
        buttons->addWidget(button);
    };
    addButton(QStringLiteral("카메라 설정"), &SettingsTab::cameraSettingsRequested);
    addButton(QStringLiteral("CCTV 재연결"), &SettingsTab::cameraReconnectRequested);
    addButton(QStringLiteral("센서 높이"), &SettingsTab::sensorHeightRequested);
    addButton(QStringLiteral("스캔 파일 열기"), &SettingsTab::openScanFileRequested);
    addButton(QStringLiteral("캘리브레이션 결과(result.json) 불러오기"), &SettingsTab::loadCalibResultRequested);
    addButton(QStringLiteral("TOP-VIEW 전체화면"), &SettingsTab::topViewFullScreenToggled);
    root->addLayout(buttons);

    auto *bottom = new QHBoxLayout;
    auto *dev = new QPushButton(QStringLiteral("Developer"), this);
    auto *user = new QPushButton(QStringLiteral("User"), this);
    dev->setFixedSize(68, 26);
    user->setFixedSize(50, 26);
    connect(dev, &QPushButton::clicked, this, [this] { emit themeChangeRequested(Theme::Mode::Developer); });
    connect(user, &QPushButton::clicked, this, [this] { emit themeChangeRequested(Theme::Mode::User); });
    bottom->addWidget(dev);
    bottom->addWidget(user);
    bottom->addStretch(1);

    auto *logout = new QPushButton(QStringLiteral("로그아웃"), this);
    logout->setFixedHeight(26);
    connect(logout, &QPushButton::clicked, this, &SettingsTab::logoutRequested);
    bottom->addWidget(logout);
    root->addLayout(bottom);
    root->addStretch(1);

    m_theme->setText(state.theme == Theme::Mode::Developer ? QStringLiteral("Developer") : QStringLiteral("User"));
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
