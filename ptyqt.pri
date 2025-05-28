# ******************************************************************************
# COPYRIGHT (c) Since 2008 - by RG System Corporation.
# This software has been provided pursuant to a License Agreement
# containing restrictions on its use.  This software contains
# valuable trade secrets and proprietary information of
# RG System Corporation and is protected by law. It may
# not be copied or distributed in any form or medium, disclosed
# to third parties, reverse engineered or used in any manner not
# provided for in said License Agreement except with the prior
# written authorization from RG System Corporation.
# ******************************************************************************

LIBS += -lptyqt

INCLUDEPATH += ../ptyqt

win32 {
    CONFIG(debug, debug|release) {
        LIBS += -lwinptyd
        LIBS += -L../lumiere/ptyqt/debug
    } else {
        LIBS += -lwinpty
        LIBS += -L../lumiere/ptyqt/release
    }
}

unix {
    LIBS += -L../lumiere/ptyqt
}
