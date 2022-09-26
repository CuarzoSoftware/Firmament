#include "SharedIcons.h"
#include <QPainter>

SharedIcons::SharedIcons()
{
    QRgb dark = qRgb(32,32,32);
    QRgb light = qRgb(255,255,255);

    QImage loader = QImage(":/img/right-arrow.png").scaled(QSize(32,32), Qt::KeepAspectRatio).convertToFormat(QImage::Format_ARGB32);
    rightArrow = toColor(loader, dark);
    rightArrowWhite = toColor(loader, light);

    loader = QImage(":/img/check.png").scaled(QSize(32,32), Qt::KeepAspectRatio).convertToFormat(QImage::Format_ARGB32);
    check = toColor(loader, dark);
    checkWhite = toColor(loader, light);

    loader = QImage(":/img/prisma.png").scaled(QSize(92,92), Qt::KeepAspectRatio).convertToFormat(QImage::Format_ARGB32);
    cuarzo = toColor(loader, dark);
    cuarzoWhite = toColor(loader, light);
}

QPixmap SharedIcons::toColor(QImage &image, QRgb color)
{
    QPainter painter(&image);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(image.rect(), color);
    painter.end();
    return QPixmap::fromImage(image);
}
