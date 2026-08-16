#include <QApplication>
#include <QFont>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("LinkPoint"));
    QApplication::setOrganizationName(QStringLiteral("LinkPoint"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));

    QFont font(QStringLiteral("Segoe UI"), 10);
    font.setStyleHint(QFont::System);
    app.setFont(font);

    MainWindow window;
    window.show();
    return app.exec();
}
