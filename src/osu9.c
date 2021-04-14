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
#include "timeline.h"

void
rotate(int *x, int *y, int ox, int oy, float angle)
{
	float s, c;
	int nx, ny;

	if (x == nil || y == nil)
		return;

	s = sin(angle);
	c = cos(angle);

	*x -= ox;
	*y -= oy;

	nx = *x * c - *y * s;
	ny = *x * s + *y * c;

	*x = nx + ox;
	*y = ny + oy;

	return;
}

int
docircle(beatmap *bmp, hitobject *op)
{
	int t, n;
	float u;
	rgline *rlp;
	hitobject *nop;

	t = op->t;
	n = 0;
	rlp = lookuprglinet(bmp->rglines, op->t, RLINE);
	for (u = 0; u <= 6; u += 0.2) {
		nop = mkobj(TCIRCLE, t, 256 + cos(u)*128, 192 + sin(u)*128);
		bmp->objects = addobjt(bmp->objects, nop);
		t += ticklen(rlp->duration, 8, 1);
		if (n++ % 4 == 0)
			nop->newcombo = 1;
	}
	bmp->objects = rmobj(bmp->objects, op);

	return 0;
}

int
doncspam(beatmap *bmp, char *s)
{
	hitobject *op;
	int selected, n;

	selected = 0;
	op = lookupobjstr(bmp->objects, &selected, s);
	for (n = 0; n < selected; n++) {
		op->newcombo = 1;
		op = op->next;
	}

	return 0;
}

int
dospiral(beatmap *bmp)
{
	hitobject *np;
	rgline *rlp;
	double t;
	int i;
	float angle;

	i = 0;
	angle = 0;
	rlp = lookuprglinet(bmp->rglines, bmp->objects->t, RLINE);
	bmp->objects = np = mkobj(TCIRCLE, -1, 256, 192);
	for (t = rlp->t; t <= 100000; t += ticklen(rlp->duration, 16, 1)) {
		np = addobjt(np, mkobj(TCIRCLE, t++, 128, 96))->next;
		np = addobjt(np, mkobj(TCIRCLE, t++, 384, 96))->next;
		np = addobjt(np, mkobj(TCIRCLE, t++, 384, 320))->next;
		np = addobjt(np, mkobj(TCIRCLE, t++, 128, 320))->next;
	}
	np = bmp->objects;
	bmp->objects = rmobj(bmp->objects, bmp->objects);
	nukeobj(np);

	i = 0;
	for (np = bmp->objects; np != nil; np = np->next) {
		if (i++ % 4 == 0)
			angle += 0.1;
		if (i % 16 == 0)
			np->newcombo = 1;
		rotate(&np->anchors->x, &np->anchors->y, 256, 192, angle);
	}

	return 0;
}

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

	int nanchors;
	anchor *ap;
	hitobject *op;

	for (op = bmp->objects; op != nil; op = op->next) {
		nanchors = 0;
		for (ap = op->anchors; ap != nil; ap = ap->next)
			nanchors++;

		print("%G\n", bezierlen(op->anchors, nanchors));
	}
	exits(0);

	boutfile = ecalloc(1, sizeof(Biobuf));
	Binit(boutfile, 1, OWRITE);
	writemap(boutfile, bmp);

	Bterm(boutfile);
	nukebeatmap(bmp);

	exits(0);
}
