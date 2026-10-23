#pragma once
#include "sharekeys/host_engine.hpp"
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
namespace sharekeys {
class HostWindow : public QMainWindow {
public:
    HostWindow(HostEngine& engine, QString directory, QString fingerprint, bool smokeTest = false);
    ~HostWindow() override;
    bool hasTray() const;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void addDevice();
    void refresh();
    void save();
    void load();
    void applyTargets();
    void redrawLayout();
    QString selectedId() const;
    HostEngine& engine_;
    QString directory_;
    QLabel* mode_{};
    std::unique_ptr<QLabel> mirrorBanner_;
    QLabel* diagnostics_{};
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
