TEMPLATE	= app
LANGUAGE	= C++
QMAKE_CXXFLAGS += -std=c++11 -I../ -DENABLE_XMLCONF

CONFIG	+= qt warn_on thread release debug
QT += widgets

LIBS += -lupnpp

INCLUDEPATH += $$OUT_PWD/../src
INCLUDEPATH += ../src
INCLUDEPATH += ..

HEADERS	+= confgui.h mainwindow.h

SOURCES	+= confmain.cpp \
           confgui.cpp \
           ../src/conftree-fixed.cpp \
           ../src/smallut.cpp \
           ../src/pathut.cpp

