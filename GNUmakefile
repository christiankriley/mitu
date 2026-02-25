# WARNING: This GNUmakefile is specifically for Linux and macOS (Unix-like systems).
# For Windows, use Makefile.

CXX = g++

APP_VERSION = 0.2.0
SCHEMA_VERSION = 2

CXXFLAGS = -std=c++20 -O3 -Wall \
           -DAPP_VERSION=\"$(APP_VERSION)\" \
           -DSCHEMA_VERSION=$(SCHEMA_VERSION)

UNAME_S := $(shell uname -s)

# 2/24/2026 - Clang does not fully support our C++20 libraries, so we will use the latest GCC from Homebrew on macOS if available.
ifeq ($(UNAME_S),Darwin)
BREW_GCCS := $(wildcard /opt/homebrew/bin/g++-*)
CXX := $(lastword $(sort $(BREW_GCCS)))
ifeq ($(CXX),)
CXX = g++
CXXFLAGS += -D_LIBCPP_ENABLE_EXPERIMENTAL
endif
endif

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
