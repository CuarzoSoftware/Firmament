#include "Firmament.h"

#include <QApplication>
#include <QFile>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationDomain("cuarzo.org");

    QFile styles(":/styles/Firmament.styles");
    styles.open(QFile::ReadOnly);
    a.setStyleSheet(styles.readAll());
    styles.close();

    Firmament paradiso = Firmament();
    paradiso.show();

    return a.exec();
}
