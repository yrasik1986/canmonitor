QT += core widgets serialbus

TARGET = canmonitor
TEMPLATE = app
CONFIG += c++17

# Важно: добавьте эти флаги для macOS
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
    QMAKE_CXXFLAGS += -stdlib=libc++
    QMAKE_LFLAGS += -stdlib=libc++
    LIBS += -lc++
}

SOURCES += \
    main.cpp \
    Canmonitor.cpp

HEADERS += \
    Canmonitor.h

# macOS specific settings
macx {
    LIBS += -L/usr/local/lib -lPCBUSB
}

# Linux specific settings
linux {
    LIBS += -lsocketcan
}
