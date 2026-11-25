QT += core gui network sql svg testlib dbus
LIBS += -lcrypto
CONFIG += c++20 testcase
QMAKE_CXXFLAGS += -Wno-missing-field-initializers
TARGET = webappwrap_tests
TEMPLATE = app

INCLUDEPATH += ../src $$OUT_PWD/..

HEADERS += \
    ../src/appbuilder.h \
    ../src/appregistry.h \
    ../src/batchfile.h \
    ../src/browsercookies.h \
    ../src/hexcolor.h \
    ../src/iconrenderer.h \
    ../src/inputnormalizer.h \
    ../src/json5.h \
    ../src/linkpolicy.h \
    ../src/paths.h \
    ../src/theme.h \
    ../src/themecolor.h \
    ../src/translator.h \
    ../src/version.h

SOURCES += \
    webappwrap_tests.cpp \
    ../src/appbuilder.cpp \
    ../src/appregistry.cpp \
    ../src/batchfile.cpp \
    ../src/browsercookies.cpp \
    ../src/hexcolor.cpp \
    ../src/iconrenderer.cpp \
    ../src/inputnormalizer.cpp \
    ../src/json5.cpp \
    ../src/linkpolicy.cpp \
    ../src/paths.cpp \
    ../src/theme.cpp \
    ../src/themecolor.cpp \
    ../src/translator.cpp \
    ../src/version.cpp
