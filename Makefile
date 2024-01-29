CXXFLAGS+=-std=c++11
#CXXFLAGS+=-Wno-narrowing
LDLIBS+=-lm -pthread
CXXFLAGS+=-O0 -g

PKGS=sdl2 gl
CXXFLAGS+=$(shell pkg-config --cflags ${PKGS})
LDLIBS+=$(shell pkg-config --libs ${PKGS})

all: iced_sdl2_opengl4

IMGUI_OBJS=imgui.o imgui_widgets.o imgui_tables.o imgui_draw.o imgui_impl_sdl2.o imgui_impl_opengl4.o

iced_sdl2_opengl4: iced_main_sdl2_opengl4.o iced.o iced_nodedef.o iced_parser.o gl3w.o gb_math.o stb_ds.o $(IMGUI_OBJS)
	$(CXX) $^ $(LDLIBS) -o $@

clean:
	rm -f *.o iced_sdl2_opengl4
