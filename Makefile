#############################################################################
# Makefile for building: naivetweet
#############################################################################

MAKEFILE      = Makefile

####### Compiler, tools and options

CC            = gcc
CXX           = g++
CXXFLAGS      = -pipe -march=x86-64 -mtune=generic -O2 -pipe -fstack-protector --param=ssp-buffer-size=4 -std=c++0x -Wall -W
LIBS          = -lncursesw 

####### Files

OBJECTS       = main.o \
		naivedb.o \
		diskfile.o \
		tweetop.o

####### Build rules

all: naivetweet

clean:
	rm $(OBJECTS) naivetweet

benchmark: naivedb.o diskfile.o
	$(CXX) $(CXXFLAGS) $(LIBS) benchmark.cpp naivedb.o diskfile.o -o benchmark
	./benchmark
	rm benchmark bmtable.dat bmtable_id.idx

cleandb:
	rm *.dat *.idx

####### Link

naivetweet: $(OBJECTS)
	$(CXX) $(LIBS) $(OBJECTS) -o naivetweet

####### Compile

main.o: main.cpp naivedb.h kikutil.h bptree.hpp diskfile.h tweetop.h
	$(CXX) -c $(CXXFLAGS) -o main.o main.cpp

naivedb.o: naivedb.cpp naivedb.h kikutil.h bptree.hpp diskfile.h
	$(CXX) -c $(CXXFLAGS) -o naivedb.o naivedb.cpp

diskfile.o: diskfile.cpp diskfile.h kikutil.h
	$(CXX) -c $(CXXFLAGS) -o diskfile.o diskfile.cpp

tweetop.o: tweetop.cpp tweetop.h naivedb.h kikutil.h bptree.hpp diskfile.h
	$(CXX) -c $(CXXFLAGS) -o tweetop.o tweetop.cpp
