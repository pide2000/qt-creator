TEMPLATE = app
#OPENMV-DIFF# TARGET = qtcreator.sh
TARGET = openmvide.sh

include(../qtcreator.pri)

OBJECTS_DIR =

#OPENMV-DIFF# PRE_TARGETDEPS = $$PWD/qtcreator.sh
PRE_TARGETDEPS = $$PWD/openmvide.sh

#OPENMV-DIFF# QMAKE_LINK = cp $$PWD/qtcreator.sh $@ && : IGNORE REST OF LINE:
QMAKE_LINK = cp $$PWD/openmvide.sh $@ && : IGNORE REST OF LINE:
QMAKE_STRIP =
CONFIG -= qt separate_debug_info gdb_dwarf_index

#OPENMV-DIFF# QMAKE_CLEAN = qtcreator.sh
QMAKE_CLEAN = openmvide.sh

target.path  = $$INSTALL_BIN_PATH
INSTALLS    += target

#OPENMV-DIFF# DISTFILES = $$PWD/qtcreator.sh
DISTFILES = $$PWD/openmvide.sh
