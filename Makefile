CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
TARGET = driver

$(TARGET): driver.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) driver.cpp

clean:
	rm -f $(TARGET)
