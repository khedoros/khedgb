KHEDGB_REQ_LIST:=main.o cpu.o cpu.h memmap.o memmap.h lcd.o lcd.h apu.o apu.h rom.o rom.h decode.o util.o util.h printer.o printer.h
KHEDGB_OBJ_LIST:=main.o cpu.o memmap.o lcd.o apu.o rom.o decode.o util.o printer.o
LDFLAGS:= $(shell sdl2-config --libs --cflags) -lpng -lm
ifeq "$(BUILD)" "debug"
    CXXFLAGS:=-g -std=c++11 -Wall -DDEBUG
else ifeq "$(BUILD)" "profile"
    CXXFLAGS:=-g -pg -std=c++11
else
    CXXFLAGS:=-std=c++11 -O3 -march=native -flto
endif

ifeq "$(CAMERA)" "yes"
    CXXFLAGS+=-DCAMERA
    KHEDGB_REQ_LIST+=camera.o camera.h
    KHEDGB_OBJ_LIST+=camera.o
    LDFLAGS+= -lopencv_core -lopencv_highgui -lopencv_imgproc -lopencv_videoio
endif


PLATFORM:=$(shell uname -s|cut -d '-' -f 1)
ifneq "$(PLATFORM)" "CYGWIN_NT"
    LDFLAGS+= -lminizip
endif

khedgb: $(KHEDGB_REQ_LIST)
	$(CXX) $(CXXFLAGS) $(KHEDGB_OBJ_LIST) -o khedgb $(LDFLAGS)

graphics_test: utils/graphics_test.cpp lcd.cpp util.cpp
	$(CXX) -std=c++11 -DPPU_ONLY utils/graphics_test.cpp lcd.cpp util.cpp -o graphics_test $(LDFLAGS)

%.o: %.cpp %.h
	$(CXX) -c $(CXXFLAGS) $<

main.o: main.cpp
	$(CXX) -c $(CXXFLAGS) $<

decode.o: decode.cpp
	$(CXX) -c $(CXXFLAGS) $<

clean:
	-rm khedgb *.o
