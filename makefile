DIR=$(HOME)/polyoptim
HEADER1=$(DIR)/carma/include
HEADER2=$(DIR)/optim/include
CC=g++
LIBS=-larmadillo -lm
INSTALLFLAGS=-fPIC -Wall -g -I$(HEADER1) -I$(HEADER2) -shared -pthread -std=c++11 -O
TESTFLAGS=-g --std=c++11 -pthread
OPTIMFLAGS=-fPIC -g -Wall -shared -I$(HEADER2) -O3

carma:
	$(CC) $(INSTALLFLAGS) carma/src/carma.cpp \
	                      carma/src/player.cpp \
					      carma/src/kalman.cpp \
					      carma/src/kinematics.cpp \
	                      carma/src/extkalman.cpp \
	                      carma/src/table_tennis.cpp \
	                      carma/src/lookup.cpp \
	                      $(LIBS) -o libcarma.so
	#gcc -c -I$(HEADER) src/carma.c -o carma.o
	#g++ -Wall -c -I$(HEADER) src/carma.cpp $(LIBS) -o carma.o
	#ar rcs libcarma.a carma.o

optim:
	$(CC) $(OPTIMFLAGS) optim/src/optimpoly.c \
						optim/src/lazyoptim.c \
						optim/src/invkin.c \
					optim/src/kinematics.c \
					optim/src/utils.c \
	      -lm -o liboptim.so


test:
	$(CC) $(TESTFLAGS) carma/test/table_tennis.cpp \
	                 carma/test/kalman.cpp \
	                 optim/test/optim.cpp \
	                 optim/test/kinematics.cpp \
	                  -o unit_tests.o -lm -larmadillo \
	                   $(LIBS) -I$(HEADER1) -I$(HEADER2) -I/usr/local/include \
	                   /usr/local/lib/libboost_unit_test_framework.a ./liboptim.so -lnlopt ./libcarma.so            

clean:
	rm -rf *.a *.o *.so

.PHONY: all test clean carma optim
