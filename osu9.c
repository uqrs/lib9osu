#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "rgbline.h"
#include "hitobject.h"
#include "beatmap.h"
#include "hitsound.h"

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

	int fd;
	int n = 0;

	if (argc != 2) {
		print("Usage: %s file\n", argv[0]);
		exits("Too few arguments.");
	}

	fd = open(argv[1], OREAD);

	if (fd == -1)
		print("You fucked up: %r\n");

	beatmap *bmp = mkbeatmap();
	loadmap(bmp, fd);

	if (bmp == nil)
		print("%r\n");
	else {
		print("%s - %s [%s] by %s\n", bmp->artist, bmp->title, bmp->diffname, bmp->author);
		print("%S - %S [%s] by %s\n", bmp->utf8artist, bmp->utf8title, bmp->diffname, bmp->author);
		print("CS %f AR %f HP %f OD %f sv %f tick: %d\n", bmp->cs, bmp->ar, bmp->hp, bmp->od, bmp->slmultiplier, bmp->sltickrate);

		for (hitobject *np = bmp->objects; np; np = np->next) {
			print("%s", types[np->type-1]);

			if (np->newcombo == 1)
				print("! %d", np->comboskip);

			print("	#%d	t = %d (%d,%d)	",++n,np->t,np->x,np->y);

			if (np->type == TSLIDER) {
				print("(Type: '%c' Points:", np->curve);
				for (anchor *ap = np->alistp; ap != nil; ap = ap->next)
					print(" %d,%d", ap->x, ap->y);

				print(") (");

				print("%s", gethsnames(np->sladditions[0]));
				for (int y = 1; y < np->nsladdition; y++)
					print(" // %s", gethsnames(np->sladditions[y]));

				print(") (");

				print("%s:%s", sets[np->slnormalsets[0]], sets[np->sladditionsets[0]]);
				for (int y = 1; y < np->nsladdition; y++)
					print(" // %s:%s", sets[np->slnormalsets[y]], sets[np->sladditionsets[y]]);

				print(")\n");

			} else if (np->type == TSPINNER) {
				print("(Duration: %dms)\n", np->spinnerlength);
			} else {
				print("(%s) ", gethsnames(np->additions));
				print("(%s:%s)\n", sets[np->normalset], sets[np->additionset]);
			}
		}

		print("\n\n");
		n = 0;

		for (gline *glp = bmp->glines; glp; glp = glp->next)
			print("Green #%d	t = %d (Volume: %d%%) (Velocity: %d, Kiai: %d) (%s:%d)\n", ++n, glp->t, glp->volume, glp->velocity, glp->kiai, sets[glp->sampset], glp->sampindex);

		print("\n");
		n = 0;
		
		for (rline *rlp = bmp->rlines; rlp; rlp = rlp->next)
			print("Red #%d	t = %d (Volume: %d%%) (Duration: %d, Meter: %d/4, Kiai: %d) (%s:%d)\n", ++n, rlp->t, rlp->volume, rlp->duration, rlp->beats, rlp->kiai, sets[rlp->sampset], rlp->sampindex);

	}

	print("\n");
	for (int q = 0; q < bmp->ncolours; q++)
		print("Combo%d = #%X\n", q+1, bmp->colours[q]);

	print("\n");
	for (int b = 0; b < bmp->nbookmarks; b++)
		print("Bookmark #%d %d\n", b+1, bmp->bookmarks[b]);

	print("\n");
	print("%f %f %d %f\n", bmp->distancesnap, bmp->beatdivisor, bmp->gridsize, bmp->timelinezoom);

	print("\n");
	print("%d\n", strlen(bmp->events)+1);
	print("%s", bmp->events);

	nukebeatmap(bmp);

	exits(0);
}
