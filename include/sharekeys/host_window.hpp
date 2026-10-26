#pragma once
#include "sharekeys/host_controller.hpp"
#include <QMainWindow>
#include <QJsonObject>
class QLabel;
class QTableWidget;
class QLineEdit;
class QSpinBox;
class QComboBox;
class QGraphicsScene;
class QCheckBox;
class QSystemTrayIcon;
class QPushButton;
class QPlainTextEdit;
namespace sharekeys {
class HostWindow : public QMainWindow {
public:
    HostWindow(HostEngine& engine, QString directory, QString fingerprint, bool smokeTest = false, bool inputAvailable = false);
    ~HostWindow() override;
    bool hasTray() const;
protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
private:
    void addDevice();
    void refresh(const HostStatus& state);
    void save();
    void load();
    void applyTargets();
    void redrawLayout();
    QString selectedId() const;
    HostController controller_;
    QString directory_;
    QLabel* mode_{};
    std::unique_ptr<QLabel> mirrorBanner_;
    QLabel* diagnostics_{};
    QLabel* summary_{};
    QLabel* shortcutsLabel_{};
    QPlainTextEdit* log_{};
    QString previousDiagnostics_;
    QPushButton *remoteButton_{}, *mirrorButton_{};
    QCheckBox *closeToTray_{}, *minimizeToTray_{};
    bool inputAvailable_{};
    std::array<int, 3> appliedKeys_{0x4D, 0x1B, 0x09};
    QTableWidget* devices_{};
    QLineEdit *name_{}, *address_{}, *pin_{}, *groupName_{};
    QSpinBox *port_{}, *mirrorKey_{}, *emergencyKey_{}, *nextKey_{}, *layoutX_{}, *layoutY_{};
    QComboBox* groups_{};
    QGraphicsScene* scene_{};
    QCheckBox* edgeSwitch_{};
    QCheckBox* autoConnect_{};
    QSystemTrayIcon* tray_{};
    bool smokeTest_{};
    QJsonObject groupsConfig_, positions_;
    bool loading_{};
};
}
