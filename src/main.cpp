#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("VisionInspect"));
    QApplication::setOrganizationName(QStringLiteral("VisionInspect"));

    MainWindow w;
    w.show();
    return QApplication::exec();
}
