TEMPLATE = app
CONFIG += console
CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    naivedb.cpp \
    diskfile.cpp

HEADERS += \
    naivedb.h \
    bptree.hpp \
    kikutil.h \
    diskfile.h \
    spstring.hpp


unix: LIBS += -lncursesw

OTHER_FILES += \
    db.xml \
    filescheme.txt
