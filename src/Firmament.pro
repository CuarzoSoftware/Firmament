QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

INCLUDEPATH += ./../../Heaven/src/lib
LIBS += -L./../../../build/build-Heaven -lHeaven-Server

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    code/Action.h \
    code/AppMenu.h \
    code/Client.h \
    code/Firmament.h \
    code/Menu.h \
    code/MenuItem.h \
    code/Object.h \
    code/Option.h \
    code/Select.h \
    code/Separator.h \
    code/SharedIcons.h \
    code/SystemMenu.h \
    code/Toggle.h \
    code/TopBar.h \
    code/TopBarItem.h

SOURCES += \
    code/Action.cpp \
    code/AppMenu.cpp \
    code/Client.cpp \
    code/Firmament.cpp \
    code/Menu.cpp \
    code/MenuItem.cpp \
    code/Object.cpp \
    code/Option.cpp \
    code/Select.cpp \
    code/Separator.cpp \
    code/SharedIcons.cpp \
    code/SystemMenu.cpp \
    code/Toggle.cpp \
    code/TopBar.cpp \
    code/TopBarItem.cpp \
    code/main.cpp

RESOURCES += \
    resources/Resources.qrc

