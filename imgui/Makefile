CXXFLAGS+=-std=c++11
CXXFLAGS+=-O0 -g

PKGS=sdl2 gl
CXXFLAGS+=$(shell pkg-config --cflags ${PKGS})
LDLIBS+=$(shell pkg-config --libs ${PKGS})

IMGUI_OBJS=imgui.o imgui_widgets.o imgui_tables.o imgui_draw.o imgui_impl_sdl2.o imgui_impl_opengl4.o

all: libimgui.a

libimgui.a: ${IMGUI_OBJS} gl3w.o
	ar rc $@ $^


clean:
	rm -f *.o
