# -------------------------------------------------
# Project created by QtCreator 2009-08-31T19:58:15
# -------------------------------------------------

#CONFIG += release
TARGET = LvkSpriteEditor
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    spritestate.cpp \
    lvkframe.cpp \
    inputimage.cpp \
    lvkanimation.cpp \
    lvkaframe.cpp \
    lvkanimationwidget.cpp \
    lvkaction.cpp \
    lvkinputimagewidget.cpp \
    lvktablewidget.cpp \
    spritestate2.cpp \
    dialogs.cpp \
    statecircularbuffer.cpp
HEADERS += mainwindow.h \
    spritestate.h \
    lvkframe.h \
    inputimage.h \
    types.h \
    lvkanimation.h \
    lvkaframe.h \
    lvkanimationwidget.h \
    lvkaction.h \
    settings.h \
    lvkinputimagewidget.h \
    lvktablewidget.h \
    spritestate2.h \
    dialogs.h \
    statecircularbuffer.h
FORMS += mainwindow.ui
RESOURCES += res.qrc

#DEFINES += DEBUG
#DEFINES += DEBUG_SHOW_ID_COLS
#DEFINES += DEBUG_UNDO
DEFINES += QT_NO_DEBUG_OUTPUT

macx:DEFINES += MAC_OS_X

