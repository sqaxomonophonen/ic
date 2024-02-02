CXXFLAGS+=-std=c++11
#CXXFLAGS+=-Wno-narrowing
LDLIBS+=-lm -pthread
CXXFLAGS+=-O0 -g


PKGS=sdl2 gl
CXXFLAGS+=$(shell pkg-config --cflags ${PKGS})
LDLIBS+=$(shell pkg-config --libs ${PKGS})

CXXFLAGS+=-I./lua
LDLIBS+=-L./lua -llua

CXXFLAGS+=-I./imgui
LDLIBS+=-L./imgui -limgui

all: iced_sdl2_opengl4


iced_sdl2_opengl4: iced_main_sdl2_opengl4.o iced.o iced_nodedef.o iced_codegen.o gb_math.o stb_ds.o
	$(CXX) $^ $(LDLIBS) -o $@

clean:
	rm -f *.o iced_sdl2_opengl4
