QT += core gui network qml quick quickcontrols2 dbus svg webenginecore webenginequick

CONFIG += c++20 release
TARGET = webappwrap
TEMPLATE = app

# scripts/build writes waw_version.h (the commit) into the build folder.
INCLUDEPATH += $$OUT_PWD

HEADERS += \
    src/appbuilder.h \
    src/appdraft.h \
    src/applistmodel.h \
    src/appregistry.h \
    src/batchfile.h \
    src/cli.h \
    src/hexcolor.h \
    src/iconrenderer.h \
    src/inputnormalizer.h \
    src/json5.h \
    src/linkpolicy.h \
    src/notifier.h \
    src/paths.h \
    src/portalfilepicker.h \
    src/runtime.h \
    src/theme.h \
    src/themecolor.h \
    src/translator.h \
    src/version.h

SOURCES += \
    src/main.cpp \
    src/appbuilder.cpp \
    src/appdraft.cpp \
    src/applistmodel.cpp \
    src/appregistry.cpp \
    src/batchfile.cpp \
    src/cli.cpp \
    src/hexcolor.cpp \
    src/iconrenderer.cpp \
    src/inputnormalizer.cpp \
    src/json5.cpp \
    src/linkpolicy.cpp \
    src/notifier.cpp \
    src/paths.cpp \
    src/portalfilepicker.cpp \
    src/runtime.cpp \
    src/theme.cpp \
    src/themecolor.cpp \
    src/translator.cpp \
    src/version.cpp

RESOURCES += src/resources.qrc
