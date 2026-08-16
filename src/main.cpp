#include <QApplication>
#include <QFont>
#include <QMessageBox>
#include <QPushButton>

#include "mainwindow.h"
#include "elevation.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("LinkPoint"));
    QApplication::setOrganizationName(QStringLiteral("LinkPoint"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.1"));

    QFont font(QStringLiteral("Segoe UI"), 10);
    font.setStyleHint(QFont::System);
    app.setFont(font);

    if (!isElevated()) {
        QMessageBox box;
        box.setWindowTitle(QStringLiteral("LinkPoint — Permisos de administrador"));
        box.setIcon(QMessageBox::Warning);
        box.setText(QStringLiteral("LinkPoint necesita permisos de administrador"));
        box.setInformativeText(QStringLiteral(
            "Para crear el punto de acceso, LinkPoint debe ejecutarse como administrador. "
            "¿Reiniciar ahora con privilegios elevados?"));
        QPushButton *restartButton =
            box.addButton(QStringLiteral("Reiniciar como administrador"), QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Continuar sin privilegios"), QMessageBox::RejectRole);
        box.setDefaultButton(restartButton);
        box.exec();

        if (box.clickedButton() == restartButton) {
            if (relaunchAsAdmin())
                return 0;
            QMessageBox::warning(nullptr,
                QStringLiteral("LinkPoint"),
                QStringLiteral("No se pudo solicitar la elevación de permisos. "
                               "Ejecuta la aplicación como administrador manualmente."));
        }
    }

    MainWindow window;
    window.show();
    return app.exec();
}
