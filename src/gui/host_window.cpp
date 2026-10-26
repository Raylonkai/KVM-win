#include "sharekeys/host_window.hpp"
#include "sharekeys/desktop_settings.hpp"
#include "sharekeys/desktop_application.hpp"
#include <QDateTime>
#include <QFileDialog>
#include <QHostInfo>
#include <QPlainTextEdit>
#include <QTextDocument>
#include <QApplication>
#include <QCloseEvent>
#include <QClipboard>
#include <QMenu>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QFormLayout>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsTextItem>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace sharekeys {
namespace {
QString shortcutName(int key) {
    if (key == 0x1B) return "Ctrl+Alt+Esc";
    if (key == 0x09) return "Ctrl+Alt+Tab";
    if ((key >= 0x41 && key <= 0x5A) || (key >= 0x30 && key <= 0x39)) return "Ctrl+Alt+" + QString(QChar(static_cast<char16_t>(key)));
    if (key >= 0x70 && key <= 0x87) return "Ctrl+Alt+F" + QString::number(key - 0x6F);
    return QString("Ctrl+Alt+VK 0x%1").arg(key, 2, 16, QLatin1Char('0'));
}
}
HostWindow::HostWindow(HostEngine& engine, QString directory, QString fingerprint, bool smokeTest, bool inputAvailable)
    : controller_(engine, this), directory_(std::move(directory)), inputAvailable_(inputAvailable), smokeTest_(smokeTest) {
    setWindowTitle("KVM-Win — Host"); resize(1040, 740);
    setStyleSheet("QWidget { background:#161c28; color:#e4eaf4; font-size:14px; } QLineEdit,QSpinBox,QTableWidget,QComboBox { background:#222d40; padding:6px; } QPushButton { background:#334b70; padding:10px; border-radius:5px; } QPushButton:hover { background:#41638d; } QHeaderView::section { background:#26344a; padding:8px; } QTabBar::tab { padding:12px; } QTabBar::tab:selected { background:#334b70; } QPushButton:disabled { background:#242d3c; color:#8994a6; } QCheckBox { spacing:8px; }");
    auto* root = new QWidget(this); auto* layout = new QVBoxLayout(root); setCentralWidget(root);
    auto* heading = new QLabel("KVM-Win  /  HOST"); heading->setStyleSheet("font-size:26px;font-weight:600;padding:8px"); layout->addWidget(heading);
    mode_ = new QLabel; mode_->setObjectName("routingStatus"); mode_->setTextFormat(Qt::PlainText); mode_->setMinimumHeight(72); layout->addWidget(mode_);
    mirrorBanner_ = std::make_unique<QLabel>("SHAREKEYS MIRROR ACTIVE", nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowDoesNotAcceptFocus);
    mirrorBanner_->setAttribute(Qt::WA_TransparentForMouseEvents);
    mirrorBanner_->setStyleSheet("background:#a75612;color:white;font-size:18px;font-weight:bold;padding:10px");
    mirrorBanner_->adjustSize(); mirrorBanner_->hide();
    auto* stop = new QPushButton("EMERGENCY STOP — return to Local control"); stop->setObjectName("emergencyStop"); stop->setStyleSheet("background:#a32f45;font-weight:bold"); layout->addWidget(stop);
    connect(stop, &QPushButton::clicked, this, [&] { controller_.emergencyStop(); });
    auto* tabs = new QTabWidget; layout->addWidget(tabs);
    auto* dashboard = new QWidget; auto* dash = new QVBoxLayout(dashboard);
    summary_ = new QLabel; summary_->setObjectName("dashboardSummary"); summary_->setTextFormat(Qt::PlainText); summary_->setWordWrap(true); dash->addWidget(summary_);
    auto* local = new QPushButton("Local control"); local->setObjectName("localMode"); dash->addWidget(local);
    connect(local, &QPushButton::clicked, this, [this] { controller_.emergencyStop(); });
    remoteButton_ = new QPushButton("Control selected PC"); remoteButton_->setObjectName("remoteMode"); dash->addWidget(remoteButton_);
    connect(remoteButton_, &QPushButton::clicked, this, [this] { controller_.controlDevice(selectedId().toStdString()); });
    mirrorButton_ = new QPushButton("Enable keyboard MIRROR"); mirrorButton_->setObjectName("mirrorMode"); dash->addWidget(mirrorButton_);
    connect(mirrorButton_, &QPushButton::clicked, this, [this] {
        if (controller_.status().mode == ControlMode::Mirror) controller_.emergencyStop();
        else controller_.setMode(ControlMode::Mirror);
    });
    auto* mouseMirror = new QCheckBox("Also mirror mouse (off each time ShareKeys starts)"); dash->addWidget(mouseMirror); mouseMirror->setEnabled(inputAvailable_);
    connect(mouseMirror, &QCheckBox::toggled, this, [&](bool enabled) { controller_.setMouseMirroring(enabled); });
    auto* identity = new QLabel("Host certificate SHA-256 (verify on each agent):\n" + fingerprint);
    identity->setTextInteractionFlags(Qt::TextSelectableByMouse); identity->setWordWrap(true); dash->addWidget(identity);
    auto* copyIdentity = new QPushButton("Copy host fingerprint"); dash->addWidget(copyIdentity);
    connect(copyIdentity, &QPushButton::clicked, this, [fingerprint] { QApplication::clipboard()->setText(fingerprint); });
    shortcutsLabel_ = new QLabel; shortcutsLabel_->setWordWrap(true); dash->addWidget(shortcutsLabel_);
#ifndef _WIN32
    dash->addWidget(new QLabel("Linux preview: networking and configuration work; input capture requires Windows."));
#endif
    dash->addStretch(); tabs->addTab(dashboard, "Dashboard");
    auto* manager = new QWidget; auto* managerLayout = new QVBoxLayout(manager);
    devices_ = new QTableWidget(0, 6); devices_->setObjectName("devices"); devices_->setHorizontalHeaderLabels({"Mirror", "PC name", "LAN IP", "Port", "Certificate SHA-256", "Status"});
    devices_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents); devices_->horizontalHeader()->setStretchLastSection(true);
    devices_->setSelectionBehavior(QAbstractItemView::SelectRows); devices_->setSelectionMode(QAbstractItemView::SingleSelection);
    managerLayout->addWidget(devices_);
    auto* form = new QFormLayout; name_ = new QLineEdit; address_ = new QLineEdit; pin_ = new QLineEdit; port_ = new QSpinBox; port_->setRange(1, 65535); port_->setValue(24870);
    form->addRow("Name", name_); form->addRow("Private LAN IP address", address_); form->addRow("Port", port_); form->addRow("Verified agent fingerprint", pin_); managerLayout->addLayout(form);
    auto* pair = new QPushButton("Pair and connect"); managerLayout->addWidget(pair); connect(pair, &QPushButton::clicked, this, [this] { addDevice(); });
    auto* actions = new QHBoxLayout; managerLayout->addLayout(actions);
    auto* reconnect = new QPushButton("Connect selected"); actions->addWidget(reconnect);
    connect(reconnect, &QPushButton::clicked, this, [this] {
        const auto row = devices_->currentRow(); if (row < 0) return;
        try { controller_.connectDevice({devices_->item(row, 1)->text().toStdString(), devices_->item(row, 2)->text().toStdString(), devices_->item(row, 4)->text().toStdString(), static_cast<std::uint16_t>(devices_->item(row, 3)->text().toUInt())}); }
        catch (const std::exception& error) { QMessageBox::warning(this, "Connection", error.what()); }
    });
    auto* disconnect = new QPushButton("Disconnect selected"); actions->addWidget(disconnect); connect(disconnect, &QPushButton::clicked, this, [this] { controller_.disconnect(selectedId().toStdString()); });
    auto* unpair = new QPushButton("Unpair selected"); actions->addWidget(unpair);
    connect(unpair, &QPushButton::clicked, this, [this] {
        const auto row = devices_->currentRow(); if (row < 0) return;
        controller_.disconnect(selectedId().toStdString(), true); positions_.remove(selectedId()); devices_->removeRow(row); applyTargets(); save(); redrawLayout();
    });
    connect(devices_, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) { if (!loading_ && item->column() == 0) { applyTargets(); save(); } });
    auto* discovery = new QPushButton("Automatic discovery — not implemented"); discovery->setEnabled(false); managerLayout->addWidget(discovery);
    auto* manual = new QLabel("Add PCs by private LAN IP. Verify certificate fingerprints directly on both machines before pairing."); manual->setWordWrap(true); managerLayout->addWidget(manual);
    tabs->addTab(manager, "Devices");
    auto* workspace = new QWidget; auto* work = new QVBoxLayout(workspace); scene_ = new QGraphicsScene(this); work->addWidget(new QGraphicsView(scene_));
    auto* coords = new QFormLayout; layoutX_ = new QSpinBox; layoutY_ = new QSpinBox; layoutX_->setRange(-5, 5); layoutY_->setRange(-5, 5); coords->addRow("Selected PC: column", layoutX_); coords->addRow("Row", layoutY_); work->addLayout(coords);
    auto* position = new QPushButton("Save selected PC position"); work->addWidget(position);
    connect(position, &QPushButton::clicked, this, [this] { if (selectedId().isEmpty()) return; positions_[selectedId()] = QJsonArray{layoutX_->value(), layoutY_->value()}; redrawLayout(); save(); });
    edgeSwitch_ = new QCheckBox("Enable screen-edge switching between adjacent grid cells"); work->addWidget(edgeSwitch_);
    work->addWidget(new QLabel("Each PC’s virtual desktop occupies one cell. Transitions center the pointer.\nEdges are ignored while mirroring; Ctrl+Alt+Esc always returns to Local."));
    tabs->addTab(workspace, "Workspace");
    auto* settings = new QWidget; auto* settingsLayout = new QFormLayout(settings);
    mirrorKey_ = new QSpinBox; emergencyKey_ = new QSpinBox; nextKey_ = new QSpinBox;
    for (auto* key : {mirrorKey_, emergencyKey_, nextKey_}) { key->setRange(1, 254); key->setDisplayIntegerBase(16); key->setPrefix("0x"); }
    mirrorKey_->setValue(0x4D); emergencyKey_->setValue(0x1B); nextKey_->setValue(0x09);
    settingsLayout->addRow("Ctrl+Alt+ mirror virtual key", mirrorKey_); settingsLayout->addRow("Ctrl+Alt+ emergency virtual key", emergencyKey_); settingsLayout->addRow("Ctrl+Alt+ next PC virtual key", nextKey_);
    auto* hotkeys = new QPushButton("Apply shortcuts"); settingsLayout->addRow(hotkeys);
    connect(hotkeys, &QPushButton::clicked, this, [this] {
        try { controller_.configureHotkeys(static_cast<std::uint8_t>(mirrorKey_->value()), static_cast<std::uint8_t>(emergencyKey_->value()), static_cast<std::uint8_t>(nextKey_->value())); appliedKeys_ = {mirrorKey_->value(), emergencyKey_->value(), nextKey_->value()}; save(); }
        catch (const std::exception& error) { QMessageBox::warning(this, "Shortcuts", error.what()); }
    });
    groupName_ = new QLineEdit; groups_ = new QComboBox;
    settingsLayout->addRow("Group name", groupName_); auto* saveGroup = new QPushButton("Save checked devices as group"); settingsLayout->addRow(saveGroup);
    connect(saveGroup, &QPushButton::clicked, this, [this] {
        if (groupName_->text().trimmed().isEmpty()) return;
        QJsonArray ids; for (int row = 0; row < devices_->rowCount(); ++row) if (devices_->item(row, 0)->checkState() == Qt::Checked) ids.append(devices_->item(row, 1)->text());
        groupsConfig_[groupName_->text().trimmed()] = ids; groups_->clear(); groups_->addItems(groupsConfig_.keys()); save();
    });
    settingsLayout->addRow("Saved groups", groups_); auto* useGroup = new QPushButton("Select group targets"); settingsLayout->addRow(useGroup);
    connect(useGroup, &QPushButton::clicked, this, [this] {
        const auto ids = groupsConfig_.value(groups_->currentText()).toArray(); loading_ = true;
        for (int row = 0; row < devices_->rowCount(); ++row) devices_->item(row, 0)->setCheckState(ids.contains(devices_->item(row, 1)->text()) ? Qt::Checked : Qt::Unchecked);
        loading_ = false; applyTargets(); save();
    });
    autoConnect_ = new QCheckBox("Connect saved PCs automatically when the host opens"); autoConnect_->setChecked(true); settingsLayout->addRow(autoConnect_);
    auto* startup = new QCheckBox("Start ShareKeys Host when I sign in to Windows"); startup->setChecked(loginStartupEnabled("ShareKeysHost")); settingsLayout->addRow(startup);
#ifndef _WIN32
    startup->setEnabled(false);
#endif
    connect(startup, &QCheckBox::toggled, this, [this, startup](bool enabled) {
        try { setLoginStartup("ShareKeysHost", enabled); }
        catch (const std::exception& error) { startup->blockSignals(true); startup->setChecked(!enabled); startup->blockSignals(false); QMessageBox::warning(this, "Startup", error.what()); }
    });
    settingsLayout->addRow(new QLabel("Connections reconnect with backoff. Input starts in Local mode.\nRemote control and keyboard mirroring require deliberate activation."));
    const auto preferences = loadDesktopPreferences(directory_);
    closeToTray_ = new QCheckBox("Closing the window keeps Host in the tray (returns to Local)"); closeToTray_->setObjectName("closeToTray"); closeToTray_->setChecked(preferences.closeToTray); settingsLayout->addRow(closeToTray_);
    minimizeToTray_ = new QCheckBox("Minimize to the system tray"); minimizeToTray_->setChecked(preferences.minimizeToTray); settingsLayout->addRow(minimizeToTray_);
    const auto persistPreferences = [this](bool) {
        try { saveDesktopPreferences(directory_, {closeToTray_->isChecked(), minimizeToTray_->isChecked()}); }
        catch (const std::exception& error) { QMessageBox::warning(this, "Desktop preferences", error.what()); }
    };
    connect(closeToTray_, &QCheckBox::toggled, this, persistPreferences);
    connect(minimizeToTray_, &QCheckBox::toggled, this, persistPreferences);
    settingsLayout->addRow(new QLabel("Default: Close exits. Tray behavior requires an available notification area."));
    tabs->addTab(settings, "Settings");
    auto* diagnosticPage = new QWidget; auto* diagnosticLayout = new QVBoxLayout(diagnosticPage);
    auto* about = new QLabel(buildInformation()); about->setTextInteractionFlags(Qt::TextSelectableByMouse); diagnosticLayout->addWidget(about);
    diagnostics_ = new QLabel; diagnostics_->setTextFormat(Qt::PlainText); diagnostics_->setWordWrap(true); diagnosticLayout->addWidget(diagnostics_);
    log_ = new QPlainTextEdit; log_->setReadOnly(true); log_->document()->setMaximumBlockCount(500); diagnosticLayout->addWidget(log_);
    auto* exportLog = new QPushButton("Save diagnostic history…"); diagnosticLayout->addWidget(exportLog);
    connect(exportLog, &QPushButton::clicked, this, [this] {
        const auto filename = QFileDialog::getSaveFileName(this, "Save diagnostics", "kvm-win-diagnostics.txt", "Text files (*.txt)");
        if (filename.isEmpty()) return;
        QSaveFile file(filename); const auto bytes = (buildInformation() + '\n' + log_->toPlainText()).toUtf8();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) QMessageBox::warning(this, "Diagnostics", "Could not save diagnostics.");
    });
    auto* limits = new QLabel("Manual fingerprint pairing • IPv4 LAN • Up to five PCs\nWindows input and installer require interactive acceptance testing.\nNo secure desktop / Ctrl+Alt+Delete / elevated-target bypass. RTT is not application confirmation."); limits->setWordWrap(true); diagnosticLayout->addWidget(limits);
    tabs->addTab(diagnosticPage, "About / Diagnostics");
    load(); redrawLayout(); refresh(controller_.status());
    connect(autoConnect_, &QCheckBox::toggled, this, [this] { save(); });
    if (!smokeTest_ && autoConnect_->isChecked()) {
        for (int row = 0; row < devices_->rowCount(); ++row) {
            try { controller_.connectDevice({devices_->item(row, 1)->text().toStdString(), devices_->item(row, 2)->text().toStdString(), devices_->item(row, 4)->text().toStdString(), static_cast<std::uint16_t>(devices_->item(row, 3)->text().toUInt())}); }
            catch (const std::exception& error) { devices_->item(row, 5)->setText(error.what()); }
        }
    }
    if (!smokeTest_ && QSystemTrayIcon::isSystemTrayAvailable()) {
        tray_ = new QSystemTrayIcon(QApplication::windowIcon(), this);
        tray_->setToolTip("ShareKeys Host"); auto* menu = new QMenu(this);
        menu->addAction("Open ShareKeys Host", this, [this] { showNormal(); raise(); activateWindow(); });
        menu->addAction("Emergency stop", this, [this] { controller_.emergencyStop(); });
        menu->addAction("Quit", this, [this] { controller_.emergencyStop(); QApplication::quit(); });
        tray_->setContextMenu(menu); tray_->show();
        connect(tray_, &QSystemTrayIcon::activated, this, [this](auto reason) { if (reason == QSystemTrayIcon::DoubleClick) { showNormal(); raise(); activateWindow(); } });
    }
    connect(edgeSwitch_, &QCheckBox::toggled, this, [this] { redrawLayout(); save(); });
    connect(&controller_, &HostController::statusChanged, this, &HostWindow::refresh);
}
HostWindow::~HostWindow() { controller_.emergencyStop(); }
bool HostWindow::hasTray() const { return tray_ && tray_->isVisible(); }
void HostWindow::closeEvent(QCloseEvent* event) {
    controller_.emergencyStop();
    if (hasTray() && closeToTray_->isChecked()) { hide(); event->ignore(); }
    else event->accept();
}
void HostWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized() && hasTray() && minimizeToTray_->isChecked())
        QTimer::singleShot(0, this, [this] { if (isMinimized()) hide(); });
}
QString HostWindow::selectedId() const { const auto row = devices_->currentRow(); return row < 0 ? QString{} : devices_->item(row, 1)->text(); }
void HostWindow::addDevice() {
    const auto name = name_->text().trimmed();
    if (devices_->rowCount() >= 5 || name.isEmpty()) { QMessageBox::warning(this, "Device", "Enter a name; at most five devices are supported."); return; }
    for (int row = 0; row < devices_->rowCount(); ++row) if (devices_->item(row, 1)->text() == name) { QMessageBox::warning(this, "Device", "Device names must be unique."); return; }
    try { controller_.connectDevice({name.toStdString(), address_->text().trimmed().toStdString(), pin_->text().trimmed().toStdString(), static_cast<std::uint16_t>(port_->value())}); }
    catch (const std::exception& error) { QMessageBox::warning(this, "Pairing", error.what()); return; }
    loading_ = true; const auto row = devices_->rowCount(); devices_->insertRow(row);
    const QStringList values{"", name, address_->text().trimmed(), QString::number(port_->value()), pin_->text().trimmed(), "connecting"};
    for (int col = 0; col < 6; ++col) { auto* item = new QTableWidgetItem(values[col]); item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | (col == 0 ? Qt::ItemIsUserCheckable : Qt::NoItemFlags)); devices_->setItem(row, col, item); }
    devices_->item(row, 0)->setCheckState(Qt::Unchecked); loading_ = false; devices_->selectRow(row); save(); redrawLayout();
}
void HostWindow::applyTargets() {
    std::vector<DeviceId> ids;
    for (int row = 0; row < devices_->rowCount(); ++row) if (devices_->item(row, 0)->checkState() == Qt::Checked) ids.push_back(devices_->item(row, 1)->text().toStdString());
    controller_.setTargets(std::move(ids));
}
void HostWindow::refresh(const HostStatus& state) {
    const bool mirror = state.mode == ControlMode::Mirror;
    if (mirror && !mirrorBanner_->isVisible()) {
        const auto area = screen()->availableGeometry();
        mirrorBanner_->move(area.center().x() - mirrorBanner_->width() / 2, area.top()); mirrorBanner_->show();
    } else if (!mirror) mirrorBanner_->hide();
    const auto text = mirror ? "MIRROR ACTIVE — keyboard broadcasts to checked PCs" : state.mode == ControlMode::Remote ? "REMOTE CONTROL — " + QString::fromStdString(state.active) : QString("LOCAL CONTROL");
    mode_->setText(text); mode_->setStyleSheet(mirror ? "background:#a75612;font-size:23px;font-weight:bold;padding:14px" : "background:#263c54;font-size:23px;padding:14px");
    setWindowTitle(mirror ? "MIRROR ACTIVE — KVM-Win Host" : "KVM-Win — Host");
    QString details = "Last event: " + QString::fromStdString(state.lastEvent) + "\n";
    int online{};
    for (const auto& device : state.devices) {
        if (device.online) ++online;
        details += QString::fromStdString(device.config.id + ": " + device.status) + " | retries: " + QString::number(device.retries)
            + " | RTT: " + (device.roundTripMs ? QString::number(*device.roundTripMs, 'f', 2) + " ms" : "pending") + '\n';
        for (int row = 0; row < devices_->rowCount(); ++row) if (devices_->item(row, 1)->text().toStdString() == device.config.id) devices_->item(row, 5)->setText(QString::fromStdString(device.status));
    }
    QStringList targets;
    bool selectedOnline = false;
    for (int row = 0; row < devices_->rowCount(); ++row) {
        const auto id = devices_->item(row, 1)->text();
        const bool onlineNow = std::any_of(state.devices.begin(), state.devices.end(), [&](const auto& d) { return d.online && QString::fromStdString(d.config.id) == id; });
        if (onlineNow && id == selectedId()) selectedOnline = true;
        if (onlineNow && devices_->item(row, 0)->checkState() == Qt::Checked) targets.append(id);
    }
    remoteButton_->setEnabled(state.healthy && inputAvailable_ && selectedOnline);
    mirrorButton_->setEnabled(state.healthy && inputAvailable_ && (mirror || !targets.isEmpty()));
    mirrorButton_->setText(mirror ? "Disable keyboard MIRROR" : "Enable keyboard MIRROR");
    summary_->setText(QString("Host: %1  •  Status: %2\nConnected PCs: %3 / 5  •  Mirror: %4\nSelected PC: %5\nOnline mirror targets: %6")
        .arg(QHostInfo::localHostName(), state.healthy ? "Ready" : "Engine stopped — restart required").arg(online)
        .arg(mirror ? "ON" : "OFF", selectedId().isEmpty() ? "none" : selectedId(), targets.isEmpty() ? "none" : targets.join(", ")));
    shortcutsLabel_->setText(QString("%1 — mirror   •   %2 — emergency stop   •   %3 — next PC\nInput goes to each PC’s focused window. Application execution is not confirmed.")
        .arg(shortcutName(appliedKeys_[0]), shortcutName(appliedKeys_[1]), shortcutName(appliedKeys_[2])));
    if (tray_) tray_->setToolTip(QString("KVM-Win: %1 | %2 connected").arg(mirror ? "MIRROR ACTIVE" : state.mode == ControlMode::Remote ? "REMOTE" : "LOCAL").arg(online));
    // Record transitions only, not fluctuating RTT or captured input.
    QString event = QString::number(static_cast<int>(state.mode)) + " | " + QString::fromStdString(state.lastEvent);
    for (const auto& device : state.devices) event += " | " + QString::fromStdString(device.config.id + ": " + device.status) + " retries=" + QString::number(device.retries);
    if (event != previousDiagnostics_) { previousDiagnostics_ = event; log_->appendPlainText(QDateTime::currentDateTimeUtc().toString(Qt::ISODate) + " | " + event); }
    diagnostics_->setText(QString("Connected: %1 / 5\n").arg(online) + details + "\nHeartbeat timeout: 3 s (checked every second). Typed keys are never logged.\nRTT measures protocol round trips; it does not confirm application execution.");
}
void HostWindow::save() {
    QJsonArray devices;
    for (int row = 0; row < devices_->rowCount(); ++row) devices.append(QJsonObject{{"name", devices_->item(row, 1)->text()}, {"address", devices_->item(row, 2)->text()}, {"port", devices_->item(row, 3)->text().toInt()}, {"pin", devices_->item(row, 4)->text()}, {"mirror", devices_->item(row, 0)->checkState() == Qt::Checked}});
    const auto serialized = QJsonDocument(QJsonObject{{"version", 1}, {"devices", devices}, {"groups", groupsConfig_}, {"positions", positions_}, {"edges", edgeSwitch_->isChecked()}, {"autoConnect", autoConnect_->isChecked()}, {"mirrorKey", appliedKeys_[0]}, {"emergencyKey", appliedKeys_[1]}, {"nextKey", appliedKeys_[2]}}).toJson();
    QSaveFile file(directory_ + "/host.json");
    if (!file.open(QIODevice::WriteOnly) || file.write(serialized) != serialized.size() || !file.commit()) QMessageBox::warning(this, "Configuration", "Could not save configuration.");
}
void HostWindow::load() {
    QFile file(directory_ + "/host.json"); if (!file.exists()) return;
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("cannot read host configuration");
    QJsonParseError error{}; const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject() || document.object().value("version").toInt() != 1) throw std::runtime_error("invalid host configuration");
    const auto config = document.object(); const auto list = config.value("devices").toArray();
    autoConnect_->setChecked(config.value("autoConnect").toBool(true));
    if (list.size() > 5) throw std::runtime_error("too many configured devices");
    loading_ = true;
    for (const auto& value : list) {
        const auto device = value.toObject(); const auto row = devices_->rowCount(); devices_->insertRow(row);
        const QStringList values{"", device.value("name").toString(), device.value("address").toString(), QString::number(device.value("port").toInt(24870)), device.value("pin").toString(), "disconnected"};
        for (int col = 0; col < 6; ++col) { auto* item = new QTableWidgetItem(values[col]); item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | (col == 0 ? Qt::ItemIsUserCheckable : Qt::NoItemFlags)); devices_->setItem(row, col, item); }
        devices_->item(row, 0)->setCheckState(device.value("mirror").toBool() ? Qt::Checked : Qt::Unchecked);
    }
    groupsConfig_ = config.value("groups").toObject(); positions_ = config.value("positions").toObject(); groups_->addItems(groupsConfig_.keys());
    edgeSwitch_->setChecked(config.value("edges").toBool(false));
    mirrorKey_->setValue(config.value("mirrorKey").toInt(0x4D)); emergencyKey_->setValue(config.value("emergencyKey").toInt(0x1B)); nextKey_->setValue(config.value("nextKey").toInt(0x09));
    controller_.configureHotkeys(static_cast<std::uint8_t>(mirrorKey_->value()), static_cast<std::uint8_t>(emergencyKey_->value()), static_cast<std::uint8_t>(nextKey_->value()));
    appliedKeys_ = {mirrorKey_->value(), emergencyKey_->value(), nextKey_->value()};
    loading_ = false; applyTargets();
}
void HostWindow::redrawLayout() {
    scene_->clear();
    std::map<DeviceId, GridPosition> positions;
    auto draw = [&](const QString& label, int x, int y, QColor color) {
        scene_->addRect(x * 160, y * 100, 140, 80, QPen(color), QBrush(color.darker(180)));
        auto* text = scene_->addText(label); text->setDefaultTextColor(Qt::white); text->setPos(x * 160 + 10, y * 100 + 20);
    };
    draw("HOST", 0, 0, QColor("#4d92d9"));
    for (int row = 0; row < devices_->rowCount(); ++row) {
        const auto name = devices_->item(row, 1)->text(); const auto position = positions_.value(name).toArray();
        const auto x = position.size() == 2 ? position[0].toInt() : row + 1;
        const auto y = position.size() == 2 ? position[1].toInt() : 0;
        positions.emplace(name.toStdString(), GridPosition{x, y});
        draw(name, x, y, QColor("#547758"));
    }
    try { controller_.configureLayout(std::move(positions), edgeSwitch_->isChecked()); }
    catch (const std::exception& error) {
        controller_.configureLayout({}, false);
        edgeSwitch_->blockSignals(true); edgeSwitch_->setChecked(false); edgeSwitch_->blockSignals(false);
        QMessageBox::warning(this, "Layout — edge switching disabled", error.what());
    }
}
}
