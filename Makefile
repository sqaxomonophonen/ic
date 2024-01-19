CXXFLAGS+=-std=c++11
#CXXFLAGS+=-Wno-narrowing
LDLIBS+=-lm -pthread
CXXFLAGS+=-O0 -g

PKGS=sdl2 gl
CXXFLAGS+=$(shell pkg-config --cflags ${PKGS})
LDLIBS+=$(shell pkg-config --libs ${PKGS})

all: ed_sdl2_opengl4

IMGUI_OBJS=imgui.o imgui_widgets.o imgui_tables.o imgui_draw.o imgui_impl_sdl2.o imgui_impl_opengl4.o

ed_sdl2_opengl4: ed_main_sdl2_opengl4.o gl3w.o $(IMGUI_OBJS)
	$(CXX) $^ $(LDLIBS) -o $@

clean:
	rm -f *.o ed_sdl2_opengl4
