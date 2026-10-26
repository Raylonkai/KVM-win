#include "sharekeys/desktop_application.hpp"
#include "sharekeys/build_info.hpp"
#include <QApplication>
#include <QIcon>

// Resource initialization is explicit because this code lives in a static library.
static void initializeResources() { Q_INIT_RESOURCE(desktop); }
namespace sharekeys {
void initializeDesktopApplication() {
    initializeResources();
    QApplication::setApplicationVersion(SHAREKEYS_VERSION);
    QApplication::setWindowIcon(QIcon(":/sharekeys/app.png"));
    QApplication::setStyle("Fusion");
}
QString buildInformation() {
    return QString("KVM-Win / ShareKeys %1\n%2 | %3 | Qt %4\nBuild: %5")
        .arg(SHAREKEYS_VERSION, SHAREKEYS_PLATFORM, SHAREKEYS_COMPILER, qVersion(), SHAREKEYS_BUILD_CONFIG);
}
}
