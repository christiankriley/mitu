CXX = g++

APP_VERSION = 0.2.0
SCHEMA_VERSION = 2

CXXFLAGS = -std=c++20 -O3 -Wall \
           -DAPP_VERSION=\"$(APP_VERSION)\" \
           -DSCHEMA_VERSION=$(SCHEMA_VERSION)
HEADER = mitu.hpp

all: mitu

atlas: atlas.cpp $(HEADER)
	$(CXX) $(CXXFLAGS) atlas.cpp -o atlas

mitu.db: atlas resources/geocoding/en/34.txt
	./atlas

mitu: main.cpp mitu.db $(HEADER)
	$(CXX) $(CXXFLAGS) main.cpp -o mitu

clean:
	rm -f atlas mitu mitu.db