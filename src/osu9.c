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

/* old. use as reference only
char *types[] = {
	"Circle",
	"Slider",
	"",
	"",
	"",
	"",
	"",
	"Spinner",
};

char *sounds[] = {
	"Normal, ",
	"Whistle, ",
	"Finish, ",
	"Clap, ",
};

char *sets[] = {
	"None",
	"Normal",
	"Soft",
	"Drum",
};

static
char *
gethsnames(int sbits)
{
	char *out;
	out = malloc(39 * sizeof(char));
	out[0] = '\0';
	if (sbits == 0)
		strcat(out, "None, ");
	if (sbits & ADBNORMAL)
		strcat(out, sounds[0]);
	if (sbits & ADBWHISTLE)
		strcat(out, sounds[1]);
	if (sbits & ADBFINISH)
		strcat(out, sounds[2]);
	if (sbits & ADBCLAP)
		strcat(out, sounds[3]);

	int l = strlen(out);
	out[l-2] = '\0';

	return out;
}

void
main(int argc, char *argv[])
{
	Biobuf *bfile, *bout;
	int pfm;

	pfm = 0;

	if (argc != 2) {
		print("Usage: %s file\n", argv[0]);
		exits("Too few arguments.");
	}

	bfile = Bopen(argv[1], OREAD);
	if (bfile == nil) {
		print("You fucked up: %r\n");
		exits("Bopen");
	}

	beatmap *bmp = mkbeatmap();
	loadmap(bmp, bfile);
	Bterm(bfile);

	if (bmp == nil)
		print("%r\n");
	else if (pfm == 1) {
		print("%s\n", bmp->version);
		print("%s - %s [%s] by %s\n", bmp->artist, bmp->title, bmp->diffname, bmp->author);
		print("%S - %S [%s] by %s\n", bmp->utf8artist, bmp->utf8title, bmp->diffname, bmp->author);
		print("CS %f AR %f HP %f OD %f sv %f tick: %d\n", bmp->cs, bmp->ar, bmp->hp, bmp->od, bmp->slmultiplier, bmp->sltickrate);

		print("\n");
		for (int b = 0; b < bmp->nbookmark; b++)
			print("Bookmark #%d %d\n", b+1, bmp->bookmarks[b]);

		print("\n");
		print("%s", bmp->events);

		print("\n");
		for (int q = 0; q < bmp->ncolour; q++)
			print("Combo%d = #%X\n", q+1, bmp->colours[q]);

		int n = 0;
		for (gline *glp = bmp->glines; glp; glp = glp->next)
			print("Green #%d	t = %d (Volume: %d%%) (Velocity: %.16g, Kiai: %d) (%s:%d)\n", ++n, glp->t, glp->volume, glp->velocity, glp->kiai, sets[glp->sampset], glp->sampindex);

		print("\n");
		n = 0;

		for (rline *rlp = bmp->rlines; rlp; rlp = rlp->next)
			print("Red #%d	t = %d (Volume: %d%%) (Duration: %.16g, Meter: %d/4, Kiai: %d) (%s:%d)\n", ++n, rlp->t, rlp->volume, rlp->duration, rlp->beats, rlp->kiai, sets[rlp->sampset], rlp->sampindex);

		for (hitobject *np = bmp->objects; np; np = np->next) {
			print("%s", types[np->type-1]);

			if (np->newcombo == 1)
				print("! %d", np->comboskip);

			print("	#%d	t = %d (%d,%d)	",++n,np->t,np->x,np->y);

			if (np->type == TSLIDER) {
				print("(Type: '%c' Points:", np->curve);
				for (anchor *ap = np->anchors; ap != nil; ap = ap->next)
					print(" %d,%d", ap->x, ap->y);

				print(")");

				if (np->nsladdition > 0) {
					print(" (");
					print("%s", gethsnames(np->sladditions[0]));
					for (int y = 1; y < np->nsladdition; y++)
						print(" // %s", gethsnames(np->sladditions[y]));
	
					print(") (");
	
					print("%s:%s", sets[np->slnormalsets[0]], sets[np->sladditionsets[0]]);
					for (int y = 1; y < np->nsladdition; y++)
						print(" // %s:%s", sets[np->slnormalsets[y]], sets[np->sladditionsets[y]]);
	
					print(")");
				}

				print("\n");
			} else if (np->type == TSPINNER) {
				print("(Duration: %dms)\n", np->spinnerlength);
			} else {
				print("(%s) ", gethsnames(np->additions));
				print("(%s:%s)\n", sets[np->normalset], sets[np->additionset]);
			}
		}
	}

	bout = Bopen("/home/mvk/osufs/outp.osu", OWRITE);
	writemap(bmp, bout);
	Bterm(bout);

	nukebeatmap(bmp);

	exits(0);
}
*/
