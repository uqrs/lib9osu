BIN=$PLAN9/bin
LIBDIR=$PLAN9/lib

CC=9c
LD=9l

osu9:Q:	osu9.c hitobject.c rgbline.c beatmap.c aux.c hash.c hitsound.c hitobject.h rgbline.h beatmap.h aux.h hash.h hitsound.c
	9c -c osu9.c hitobject.c rgbline.c beatmap.c aux.c hash.c hitsound.c
	9l -o osu9 osu9.o hitobject.o rgbline.o beatmap.o aux.o hash.o hitsound.o
nuke:
	rm *.o osu9
