ifeq "$(BUILD)" "debug"
    CXXFLAGS:=-g -std=c++11 -Wall -DDEBUG
else
    CXXFLAGS:=-std=c++11 -O3
endif

LDFLAGS:= $(shell sdl2-config --libs --cflags)

PLATFORM:=$(shell uname -s|cut -d '-' -f 1)
ifneq "$(PLATFORM)" "CYGWIN_NT"
    LDFLAGS+= -lminizip
endif

khedgb: main.o cpu.o cpu.h memmap.o memmap.h lcd.o lcd.h apu.o apu.h rom.o rom.h decode.o util.o util.h
	$(CXX) $(CXXFLAGS) main.o cpu.o memmap.o lcd.o apu.o rom.o decode.o util.o -o khedgb $(LDFLAGS)

graphics_test: graphics_test.cpp lcd.cpp util.cpp
	$(CXX) -std=c++11 -DPPU_ONLY graphics_test.cpp lcd.cpp util.cpp -o graphics_test $(LDFLAGS)

%.o: %.cpp %.h
	$(CXX) -c $(CXXFLAGS) $<

main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $<

decode.o: decode.cpp
	$(CXX) -c $(CXXFLAGS) $<

clean:
	rm khedgb *.o
