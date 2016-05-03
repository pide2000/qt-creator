TEMPLATE = subdirs

#OPENMV-DIFF#
#SUBDIRS = qtpromaker \
#     ../plugins/cpaster/frontend \
#     sdktool \
#     valgrindfake \
#     3rdparty \
#     qml2puppet \
#     buildoutputparser
#OPENMV-DIFF#

#OPENMV-DIFF#
#win32 {
#    SUBDIRS += qtcdebugger \
#        wininterrupt \
#        winrtdebughelper
#}
#OPENMV-DIFF#

#OPENMV-DIFF#
#mac {
#    SUBDIRS += iostool
#}
#OPENMV-DIFF#

#OPENMV-DIFF#
#isEmpty(LLVM_INSTALL_DIR):LLVM_INSTALL_DIR=$$(LLVM_INSTALL_DIR)
#exists($$LLVM_INSTALL_DIR) {
#    SUBDIRS += clangbackend
#}
#OPENMV-DIFF#

#OPENMV-DIFF#
#isEmpty(BUILD_CPLUSPLUS_TOOLS):BUILD_CPLUSPLUS_TOOLS=$$(BUILD_CPLUSPLUS_TOOLS)
#!isEmpty(BUILD_CPLUSPLUS_TOOLS) {
#    SUBDIRS += cplusplus-ast2png \
#        cplusplus-frontend \
#        cplusplus-mkvisitor \
#        cplusplus-update-frontend
#}
#OPENMV-DIFF#

#OPENMV-DIFF#
#QT_BREAKPAD_ROOT_PATH = $$(QT_BREAKPAD_ROOT_PATH)
#!isEmpty(QT_BREAKPAD_ROOT_PATH) {
#    SUBDIRS += qtcrashhandler
#} else {
#    linux-* {
#        # Build only in debug mode.
#        debug_and_release|CONFIG(debug, debug|release) {
#            SUBDIRS += qtcreatorcrashhandler
#        }
#    }
#}
#OPENMV-DIFF#
