QT += core widgets serialbus

TARGET = canmonitor
TEMPLATE = app
CONFIG += c++17

# Use Qt's UI compiler
FORMS += canmonitor.ui


SOURCES += \
    main.cpp \
    canmonitor.cpp

HEADERS += \
    canmonitor.h

# Platform-specific settings
macx {
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
    QMAKE_CXXFLAGS += -stdlib=libc++
    QMAKE_LFLAGS += -stdlib=libc++
    LIBS += -lc++
    LIBS += -L/usr/local/lib -lPCBUSB
}

linux {
    LIBS += -lsocketcan
}

win32 {
    # Windows-specific settings if needed
    LIBS += -lws2_32
}