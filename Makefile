CXXFLAGS:=-g -std=c++11
LDFLAGS:= $(shell sdl2-config --libs --cflags)

khedgb: main.o cpu.o cpu.h memmap.o memmap.h lcd.o lcd.h rom.o rom.h decode.o util.o util.h
	$(CXX) $(CXXFLAGS) main.o cpu.o memmap.o lcd.o rom.o decode.o util.o -o khedgb $(LDFLAGS)

%.o: %.cpp %.h
	$(CXX) -c $(CXXFLAGS) $<

main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $<

decode.o: decode.cpp
	$(CXX) -c $(CXXFLAGS) $<

clean:
	rm khedgb *.o
