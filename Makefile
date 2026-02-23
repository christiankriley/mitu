CXX = g++

APP_VERSION = 0.1.0
SCHEMA_VERSION = 1

CXXFLAGS = -std=c++20 -O3 -Wall \
           -DAPP_VERSION=\"$(APP_VERSION)\" \
           -DSCHEMA_VERSION=$(SCHEMA_VERSION)
HEADER = mitu.hpp

all: mitu

atlas: atlas.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) atlas.cpp -o atlas

mitu.db: atlas resources/geocoding/en/1.txt
	./atlas

mitu: main.cpp mitu.db $(HEADER)
	$(CXX) $(CXXFLAGS) main.cpp -o mitu

clean:
	rm -f atlas mitu mitu.db