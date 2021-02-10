#include <u.h>
#include <libc.h>
#include <bio.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "aux.h"
#include "hash.h"
#include "rgbline.h"
#include "hitsound.h"
#include "hitobject.h"
#include "beatmap.h"

void
main(int argc, char *argv[])
{
	beatmap *bmp;
	Biobuf *bfile, *boutfile;

	if (argc < 2) {
		fprint(2, "usage: %s file.osu\n", argv[0]);
		exits("usage");
	}

	bfile = Bopen(argv[1], OREAD);
	if (bfile == nil) {
		fprint(2, "%r\n");
		exits("Bopen");
	}
	
	bmp = mkbeatmap();
	if (readmap(bfile, bmp) < 0) {
		Bterm(bfile);
		fprint(2, "%r\n");
		exits("readmap");
	}
	Bterm(bfile);

	boutfile = ecalloc(1, sizeof(Biobuf));
	Binit(boutfile, 1, OWRITE);
	writemap(boutfile, bmp);

	Bterm(boutfile);
	nukebeatmap(bmp);

	exits(0);
}

