CXX = g++
CXXFLAGS = -std=c++20 -O3 -Wall
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