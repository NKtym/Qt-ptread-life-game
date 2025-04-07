CXX = clang++
CXXFLAGS = -Wall -std=c++11 -Wextra 

SFML_CFLAGS = $(shell pkg-config --cflags sfml-network sfml-system)
SFML_LIBS   = $(shell pkg-config --libs sfml-network sfml-system)

QT_CFLAGS = $(shell pkg-config --cflags Qt5Widgets)
QT_LIBS   = $(shell pkg-config --libs Qt5Widgets)

all: server client

server: server.cpp
	$(CXX) $(CXXFLAGS) server.cpp -o server $(SFML_CFLAGS) $(SFML_LIBS) -lpthread

client.moc: client.cpp
	moc client.cpp -o client.moc

client: client.moc client.cpp
	$(CXX) $(CXXFLAGS) client.cpp -o client $(SFML_CFLAGS) $(SFML_LIBS) $(QT_CFLAGS) $(QT_LIBS)

clean:
	rm -f server client client.moc
