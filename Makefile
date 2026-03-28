##
## EPITECH PROJECT, 2026
## Pharmalgo
## File description:
## Makefile
##

TARGET = challenge
CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17
INCLUDES = -IPharmalgo/Lib_Croix -Ilibcroix/WiringPi/wiringPi
LDFLAGS = -Llibcroix/WiringPi/wiringPi -Wl,-rpath,'$$ORIGIN/libcroix/WiringPi/wiringPi'
LDLIBS = -lwiringPi

SRC = \
	main.cpp \
	Pharmalgo/Lib_Croix/CroixPharma.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SRC) $(LDFLAGS) $(LDLIBS) -o $(TARGET)

clean:
	rm -f $(TARGET)

fclean: clean

re: fclean all