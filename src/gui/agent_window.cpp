#include "sharekeys/agent_window.hpp"
#include "sharekeys/security.hpp"
#include "sharekeys/desktop_application.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QFormLayout>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPushButton>
#include <QSpinBox>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVBoxLayout>

namespace sharekeys {
AgentWindow::AgentWindow(QString directory, Start start, bool smokeTest)
    : directory_(std::move(directory)), start_(std::move(start)) {
    setWindowTitle("KVM-Win — Agent"); resize(680, 540);
    setStyleSheet("QWidget{background:#161c28;color:#e4eaf4;font-size:14px} QLineEdit,QComboBox,QSpinBox{background:#26344a;padding:8px} QPushButton{background:#334b70;padding:12px;border-radius:5px}");
    auto* layout = new QVBoxLayout(this);
    auto* title = new QLabel("Set up this remote PC"); title->setStyleSheet("font-size:24px;font-weight:bold"); layout->addWidget(title);
    auto* intro = new QLabel("1. Copy this PC’s fingerprint into the Host’s Devices tab.\n2. Paste the Host’s fingerprint below and choose this PC’s LAN address.\n3. Click Save and start. Future launches can use the saved settings."); intro->setWordWrap(true); layout->addWidget(intro);
    const auto fingerprint = QString::fromStdString(security::fingerprint(nativePath(directory_) / "identity.crt"));
    auto* identity = new QLineEdit(fingerprint); identity->setReadOnly(true); layout->addWidget(identity);
    auto* copy = new QPushButton("Copy this PC’s fingerprint"); layout->addWidget(copy);
    connect(copy, &QPushButton::clicked, this, [fingerprint] { QApplication::clipboard()->setText(fingerprint); });
    auto* form = new QFormLayout; layout->addLayout(form);
    pin_ = new QLineEdit; pin_->setPlaceholderText("Verified host fingerprint — 64 lowercase hex characters"); form->addRow("Authorized host", pin_);
    address_ = new QComboBox; address_->setEditable(true);
    for (const auto& candidate : QNetworkInterface::allAddresses()) {
        if (candidate.protocol() != QAbstractSocket::IPv4Protocol || candidate.isLoopback()) continue;
        asio::error_code error; const auto ip = asio::ip::make_address(candidate.toString().toStdString(), error);
        if (!error && privateAddress(ip) && address_->findText(candidate.toString()) < 0) address_->addItem(candidate.toString());
    }
    form->addRow("This PC’s LAN address", address_);
    port_ = new QSpinBox; port_->setRange(1, 65535); port_->setValue(24870); form->addRow("Port", port_);
    autoListen_ = new QCheckBox("Listen automatically when the agent opens"); autoListen_->setChecked(true); layout->addWidget(autoListen_);
    auto* startup = new QCheckBox("Start ShareKeys Agent when I sign in to Windows"); startup->setChecked(loginStartupEnabled("ShareKeysAgent")); layout->addWidget(startup);
#ifndef _WIN32
    startup->setEnabled(false);
    layout->addWidget(new QLabel("Linux setup preview: native input injection requires Windows."));
#endif
    connect(startup, &QCheckBox::toggled, this, [this, startup](bool enabled) {
        try { setLoginStartup("ShareKeysAgent", enabled); }
        catch (const std::exception& error) { startup->blockSignals(true); startup->setChecked(!enabled); startup->blockSignals(false); QMessageBox::warning(this, "Startup", error.what()); }
    });
    auto* startButton = new QPushButton("Save and start listening"); startButton->setObjectName("startListening"); startButton->setEnabled(static_cast<bool>(start_)); layout->addWidget(startButton);
    connect(startButton, &QPushButton::clicked, this, [this] { startListening(); });
    auto* stop = new QPushButton("Stop input and disable automatic listening"); layout->addWidget(stop);
    connect(stop, &QPushButton::clicked, this, [this] { stopListening(true); });
    auto* revoke = new QPushButton("Unpair host and stop"); layout->addWidget(revoke);
    connect(revoke, &QPushButton::clicked, this, [this] {
        stopListening(false); pin_->clear(); autoListen_->setChecked(false);
        try { saveAgentSettings(directory_, settings()); status_->setText("Host unpaired. Input is stopped."); }
        catch (const std::exception& error) { QMessageBox::warning(this, "Unpair", error.what()); }
    });
    const auto preferences = loadDesktopPreferences(directory_);
    closeToTray_ = new QCheckBox("Closing the window keeps Agent listening in the tray"); closeToTray_->setObjectName("closeToTray"); closeToTray_->setChecked(preferences.closeToTray); layout->addWidget(closeToTray_);
    minimizeToTray_ = new QCheckBox("Minimize to the system tray"); minimizeToTray_->setChecked(preferences.minimizeToTray); layout->addWidget(minimizeToTray_);
    const auto persistPreferences = [this](bool) {
        try { saveDesktopPreferences(directory_, {closeToTray_->isChecked(), minimizeToTray_->isChecked()}); }
        catch (const std::exception& error) { QMessageBox::warning(this, "Desktop preferences", error.what()); }
    };
    connect(closeToTray_, &QCheckBox::toggled, this, persistPreferences);
    connect(minimizeToTray_, &QCheckBox::toggled, this, persistPreferences);
    layout->addWidget(new QLabel("Default: Close stops input and exits. Quit in the tray always stops input."));
    layout->addWidget(new QLabel(buildInformation()));
    status_ = new QLabel("Not listening. Complete pairing to enable remote input."); status_->setWordWrap(true); layout->addWidget(status_);
    const auto saved = loadAgentSettings(directory_);
    pin_->setText(saved.hostPin); port_->setValue(saved.port);
    if (!saved.address.isEmpty()) address_->setCurrentText(saved.address);
    if (!saved.hostPin.isEmpty()) autoListen_->setChecked(saved.autoListen);
    if (!smokeTest && QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(QApplication::windowIcon(), this);
        tray_->setToolTip("ShareKeys Agent"); auto* menu = new QMenu(this);
        menu->addAction("Open ShareKeys Agent", this, [this] { showNormal(); raise(); activateWindow(); });
        menu->addAction("Stop remote input", this, [this] { stopListening(true); });
        menu->addAction("Quit", this, [this] { stopListening(false); QApplication::quit(); });
        tray_->setContextMenu(menu); tray_->show();
        connect(tray_, &QSystemTrayIcon::activated, this, [this](auto reason) { if (reason == QSystemTrayIcon::DoubleClick) { showNormal(); raise(); activateWindow(); } });
    }
    auto* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [this] {
        if (service_) {
            const auto current = service_->status();
            status_->setText((current.connected ? "Connected to paired host — " : "Listening — ") + QString::fromStdString(current.message));
            if (tray_) tray_->setToolTip(current.connected ? "ShareKeys Agent: connected" : "ShareKeys Agent: listening");
        }
    }); timer->start(200);
    if (!smokeTest && saved.autoListen && !saved.hostPin.isEmpty() && start_) startListening();
}
AgentWindow::~AgentWindow() { service_.reset(); }
bool AgentWindow::hasTray() const { return tray_ && tray_->isVisible(); }
AgentSettings AgentWindow::settings() const { return {pin_->text().trimmed(), address_->currentText().trimmed(), port_->value(), autoListen_->isChecked()}; }
void AgentWindow::startListening() {
    try {
        if (!start_) throw std::runtime_error("Native input injection is only available on Windows.");
        const auto selected = settings();
        // Validate the intended listen configuration even if automatic start is off.
        if (!security::validPin(selected.hostPin.toStdString())) throw std::invalid_argument("Paste and verify the host fingerprint first.");
        service_.reset();
        auto next = start_(selected);
        saveAgentSettings(directory_, selected);
        service_ = std::move(next); status_->setText("Listening for the paired host.");
    } catch (const std::exception& error) { status_->setText(error.what()); QMessageBox::warning(this, "Agent setup", error.what()); }
}
void AgentWindow::stopListening(bool persist) {
    service_.reset(); status_->setText("Remote input stopped.");
    if (tray_) tray_->setToolTip("ShareKeys Agent: stopped");
    if (persist) {
        autoListen_->setChecked(false);
        try {
            // Persist only the stop flag; un-applied edits must not replace pairing.
            auto saved = loadAgentSettings(directory_); saved.autoListen = false; saveAgentSettings(directory_, saved);
        } catch (const std::exception& error) { QMessageBox::warning(this, "Stop settings", error.what()); }
    }
}
void AgentWindow::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized() && hasTray() && minimizeToTray_->isChecked())
        QTimer::singleShot(0, this, [this] { if (isMinimized()) hide(); });
}
void AgentWindow::closeEvent(QCloseEvent* event) {
    if (hasTray() && closeToTray_->isChecked()) { hide(); event->ignore(); }
    else { service_.reset(); event->accept(); }
}
}
