#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9p.h>
#include "rgbline.h"
#include "hitobject.h"
#include "beatmap.h"

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

	beatmap *bmp = loadmap(fd);

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

				print(")\n");
			} else if (np->type == TSPINNER) {
				print("(Duration: %dms)\n", np->spinnerlength);
			} else {
				print("\n");
			}
		}

		print("\n\n");
		n = 0;

		for (gline *glp = bmp->glines; glp; glp = glp->next)
			print("Green #%d	t = %d (Volume: %d%%) (Velocity: %d, Kiai: %d)\n", ++n, glp->t, glp->volume, glp->velocity, glp->kiai);

		print("\n");
		n = 0;
		
		for (rline *rlp = bmp->rlines; rlp; rlp = rlp->next)
			print("Red #%d	t = %d (Volume: %d%%) (Duration: %d, Meter: %d/4, Kiai: %d)\n", ++n, rlp->t, rlp->volume, rlp->duration, rlp->beats, rlp->kiai);

	}

	print("\n");
	for (int q = 0; q < bmp->ncolours; q++)
		print("Combo%d = #%X\n", q+1, bmp->colours[q]);

	exits(0);
}
