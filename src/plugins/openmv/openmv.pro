include(../../qtcreatorplugin.pri)
QT += serialport charts
HEADERS += openmvplugin.h openmvpluginio.h \
           openmvpluginfb.h
SOURCES += openmvplugin.cpp openmvpluginio.cpp \
           openmvpluginfb.cpp
RESOURCES += openmv.qrc
