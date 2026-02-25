# WARNING: This Makefile is specifically for Windows.
# For Linux and macOS (Unix-like systems), use GNUmakefile.

APP_VERSION = 0.2.0
SCHEMA_VERSION = 2

CXX = cl
CXXFLAGS = /std:c++20 /O2 /W4 /EHsc /DAPP_VERSION=\"$(APP_VERSION)\" /DSCHEMA_VERSION=$(SCHEMA_VERSION)

HEADER = mitu.hpp

all: mitu.exe

atlas.exe: atlas.cpp $(HEADER)
    $(CXX) $(CXXFLAGS) atlas.cpp /Featlas.exe

mitu.db: atlas.exe resources\geocoding\en\34.txt
    atlas.exe

mitu.exe: main.cpp mitu.db $(HEADER)
    $(CXX) $(CXXFLAGS) main.cpp /Femitu.exe

clean:
    -del /f atlas.exe mitu.exe mitu.db *.obj 2>nul