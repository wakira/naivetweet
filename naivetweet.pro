TEMPLATE = app
CONFIG += console
CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt

#CONFIG += release

SOURCES += main.cpp \
	naivedb.cpp \
	diskfile.cpp \
	tweetop.cpp

HEADERS += \
	naivedb.h \
	bptree.hpp \
	kikutil.h \
	diskfile.h \
	tweetop.h


unix: LIBS += -lncursesw

OTHER_FILES += \
	db.xml \
	filescheme.txt \
	benchmark.xml
