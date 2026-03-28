##
## EPITECH PROJECT, 2026
## Pharmalgo
## File description:
## Makefile
##

TARGET = challenge
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = -L.
LDLIBS = -lsim

SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

fclean: clean

re: fclean all