#lazy makefile for agb_edit
#I'm doing one-step compilation instead of compiling each C file into an O file and then linking
#it's a small program so this way ends up being both simpler and faster
#Note, this makefile is designed for mingw32/64-gcc and MSYS2, but it will be pretty trivial to adapt it to other compilers

SRC := src/main.c src/gbacia.c src/videolut.c src/console_ui.c
HDR := src/gbacia.h src/videolut.h src/blackbody_color.h src/console_ui.h

.PHONY: all debug clean

all: agb_edit.exe

debug: agb_edit_dbg.exe

clean:
	rm -f agb_edit.exe agb_edit_dbg.exe

agb_edit.exe: $(SRC) $(HDR)
	gcc -Os -o agb_edit.exe $(SRC)

agb_edit_dbg.exe: $(SRC) $(HDR)
	gcc -g -o agb_edit_dbg.exe $(SRC)
