#ifndef SHAREDICONS_H
#define SHAREDICONS_H

#include <QPixmap>

class SharedIcons
{
public:
    SharedIcons();
    QPixmap toColor(QImage &image, QRgb color);
    QPixmap rightArrow;
    QPixmap rightArrowWhite;
    QPixmap check;
    QPixmap checkWhite;
    QPixmap cuarzo;
    QPixmap cuarzoWhite;
};

#endif // SHAREDICONS_H
