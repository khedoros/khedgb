khedgb: main.o cpu.o cpu.h memmap.o memmap.h lcd.o lcd.h rom.o rom.h
	g++ -std=c++11 main.o cpu.o memmap.o lcd.o -o khedgb

%.o: %.cpp
	g++ -std=c++11 -c $<
