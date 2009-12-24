# -------------------------------------------------
# Project created by QtCreator 2009-08-31T19:58:15
# -------------------------------------------------
TARGET = LvkSpriteEditor
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    spritestate.cpp \
    lvkframe.cpp \
    inputimage.cpp \
    lvkanimation.cpp \
    lvkaframe.cpp \
    lvkframegraphicsgroup.cpp \
    lvkaction.cpp \
    qinputimagewidget.cpp \
    lvktablewidget.cpp
HEADERS += mainwindow.h \
    spritestate.h \
    lvkframe.h \
    inputimage.h \
    types.h \
    lvkanimation.h \
    lvkaframe.h \
    lvkframegraphicsgroup.h \
    lvkaction.h \
    settings.h \
    qinputimagewidget.h \
    lvktablewidget.h
FORMS += mainwindow.ui
RESOURCES += res.qrc

debug { 
    DEFINES += DEBUG \ 
# DEBUG_SHOW_ID_COLS
}

release {
    DEFINES += QT_NO_DEBUG_OUTPUT
}
