#pragma once
#include "sharekeys/agent_service.hpp"
#include "sharekeys/desktop_settings.hpp"
#include <QWidget>
class QComboBox;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;
class QSystemTrayIcon;
namespace sharekeys {
class AgentWindow : public QWidget {
public:
    using Start = std::function<std::unique_ptr<AgentService>(const AgentSettings&)>;
    AgentWindow(QString directory, Start start, bool smokeTest = false);
    ~AgentWindow() override;
    bool running() const { return service_ != nullptr; }
    bool hasTray() const;
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void startListening();
    void stopListening(bool persist);
    AgentSettings settings() const;
    QString directory_;
    Start start_;
    std::unique_ptr<AgentService> service_;
    QComboBox* address_{};
    QLineEdit* pin_{};
    QSpinBox* port_{};
    QCheckBox* autoListen_{};
    QLabel* status_{};
    QSystemTrayIcon* tray_{};
};
}
