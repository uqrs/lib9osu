#include <u.h>
#include <libc.h>
#include <bio.h>
#include "aux.h"
#include "hitobject.h"
#include "rgbline.h"
#include "hitsound.h"
#include "beatmap.h"

/* References: 
  * https://osu.ppy.sh/wiki/en/osu%21_File_Formats/Osu_%28file_format%29 
  * example/destrier.osu
  * example/crush.osu
  * example/garden.osu
  * example/neoprene.osu
  */

/* beatmap section handler prototypes */
static int hsgeneral(beatmap *bmp, Biobuf *bp);
static int hseditor(beatmap *bmp, Biobuf *bp);
static int hsmetadata(beatmap *bmp, Biobuf *bp);
static int hsdifficulty(beatmap *bmp, Biobuf *bp);
static int hsevents(beatmap *bmp, Biobuf *bp);
static int hstimingpoints(beatmap *bmp, Biobuf *bp);
static int hscolours(beatmap *bmp, Biobuf *bp);
static int hsobjects(beatmap *bmp, Biobuf *bp);

typedef struct handler {
	char *section;					/* section name, e.g. "[General]" */
	int (*read)(beatmap *, Biobuf *);	/* read handler for this section, e.g. hsgeneral */
} handler;

static handler handlers[] = {
	{.section = "[General]", .read = hsgeneral},
	{.section = "[Editor]", .read = hseditor},
	{.section = "[Metadata]", .read = hsmetadata},
	{.section = "[Difficulty]", .read = hsdifficulty},
	{.section = "[Events]", .read = hsevents},
	{.section = "[TimingPoints]", .read = hstimingpoints},
	{.section = "[Colours]", .read = hscolours},
	{.section = "[HitObjects]", .read = hsobjects},
};

/* Field labels for key:value pairs */
enum {
	KEY=0,
	VALUE,
};

/* CSV field labels for hitobject entries 
  * See: https://osu.ppy.sh/wiki/en/osu%21_File_Formats/Osu_%28file_format%29#hit-objects
  */
enum {
	OBJHITSAMP=-1,	/* custom hitsound sampleset (final field) */
	OBJX=0,			/* x position of object */
	OBJY,			/* y position of object */
	OBJTIME,			/* timestamp in milliseconds */
	OBJTYPE,			/* 8-bit integer describing object type & colours; see enum typebits */
	OBJADDITIONS,	/* hitsound additions for object */
	OBJENDTIME=5,	/* spinner end timestamp */
	OBJCURVES=5,		/* slider curve data */
	OBJSLIDES,		/* amount of reverses + 1 */
	OBJLENGTH,		/* ""visual length in osu! pixels"" */
	OBJEDGESOUNDS,	/* slider head/tail hitsounds */
	OBJEDGESETS,		/* slider head/tail sample sets */
};

/* C(olon)SV and P(ipe)SV fields for OBJCURVES and OBJSAMP */
enum {
	OBJCURVETYPE=0,
	HITSAMPNORMAL=0,
	HITSAMPADDITIONS,
	HITSAMPINDEX,
	HITSAMPVOLUME,
	HITSAMPFILE,
};

/* bitmasks for the OBJTYPE field; apply with & */
enum {
	TBTYPE = 0xB,			/* circle, spinner, or slider type */
	TBNEWCOMBO = 0x4,	/* new combo */
	TBCOLOR = 0x70,		/* amount of colours to skip */
	TBCOLORSHIFT = 4,		/* shift right amount after having applied & TCOLOR  */
	TBHOLD = 0x80, 		/* unsupported; nobody cares about osu!mania */
};

/* CSV fields for red/greenline entries 
  * see: https://osu.ppy.sh/wiki/en/osu%21_File_Formats/Osu_%28file_format%29#timing-points */
enum {
	LNTIME=0,		/* timestamp in miliseconds */
	LNDURATION=1,	/* duration of beat in miliseconds (redline only) */
	LNVELOCITY=1,	/* negative inverse slider velocity multiplier (greenline only) */
	LNBEATS,			/* number of beats in measure */
	LNSAMPSET,		/* default hitobject sampleset */
	LNSAMPINDEX,		/* custom sample index */
	LNVOLUME,		/* volume percentage */
	LNREDLINE,		/* if 1, this is a redline */
	LNEFFECTS,		/* extra effects */
};

/* bitmasks for the LNEFFECTS field; apply with & */
enum {
	EBKIAI = 0x1,		/* kiai time enabled */
	EBOMIT = 0x8,		/* unused; nobody cares about osu!taiko or osu!mania */
};

/* colour code csv fields */
enum {
	RED=0,
	GREEN,
	BLUE,
};

/* Possible values for LNSAMPSET field. Indices correspond to hitsound.h:/sampsets/ */
char *samplesets2[] = {
	"Default",		/* unused */
	"Normal",
	"Soft",
	"Drum"
};

typedef struct splitline {
	char *line;		/* line string, with delimiters replaced by '\0' */
	char **fields;	/* field pointers */
	int nfield;		/* elements in **fields */
} splitline;

/* read a line from bp, and strip out the trailing carriage return. */
static
char *
nextline(Biobuf *bp)
{
	char *line;

	if ((line = Brdstr(bp, '\n', 1)) != nil) {
		line[strlen(line) - 1] = '\0';
		return line;
	}

	return nil;
}

/* read lines from bp up until the next section header (e.g. "[General]"), and return the header.
  * returns nil on end-of-file. */
static
char *
nextsection(Biobuf *bp)
{
	char *line;

	while ((line = nextline(bp)) != nil) {
		if (line[0] == '[' && line[strlen(line) - 1] == ']')
			return line;
	}

	return nil;
}

/* read the next line from bp, and return it. If the line is empty, return nil (end of section).
  * returns nil on end-of-file. */
static
char *
nextentry(Biobuf *bp)
{
	char *line;

	if ((line = nextline(bp)) != nil) {
		if (strlen(line) == 0)
			return nil;
		else
			return line;
	}

	return nil;
}

/* return a pointer to the end of a quoted field starting at p.
  * squashes sequences of two double quotes into one single instance. */
static
char *
advquoted(char *p, char *sepchar)
{
	int i,j;
	for (i = j = 0; p[j] != '\0'; i++, j++) {
		if (p[j] == '"' && p[++j] != '"') {
			int k = strcspn(p+j, sepchar);
			memmove(p+i, p+j, k);
			i += k;
			j += k;
			break;
		}
		p[i] = p[j];
	}
	p[i] = '\0';
	return p + j;
}

/* split a copy of line[] into separate fields delimited by any of the characters in sepchar.
  * the maximum number of splits performed by split is determined by maxsplit. 0 indicates
  * no maximum.
  * if squashdelim is set to a nonzero value, then continuous sequences of separator characters
  * are treated as a single delimiter. This implies truncation of unquoted empty fields.
  *
  * returns a pointed to a splitline struct dynamically allocated with malloc().
  */
static
splitline *
split(char *line, char *sepchar, int maxsplit, int squashdelim)
{
	int maxfield;
	splitline *new;
	char *sepp;
	char sepc;
	char *p;

	if (line[0] == '\0' || sepchar == nil || maxsplit < 0)
		return nil;

	sepp = nil;
	maxfield = 9;

	new = ecalloc(1, sizeof(splitline));
	new->fields = ecalloc(maxfield, sizeof(char *));
	new->line = p = strdup(line);
	new->nfield = 0;

	do {
		if (new->nfield+1 == maxfield) {
			maxfield *= 2;
			new->fields = erealloc(new->fields, sizeof(char *) * maxfield);
		}

		if (p[0] == '"')
			sepp = advquoted(++p, sepchar);
		else
			sepp = p + strcspn(p, sepchar);

		sepc = sepp[0];
		sepp[0] = '\0';
		new->fields[new->nfield++] = p;

		if (squashdelim == 0) {
			p = sepp + 1;
		} else {
			sepp++;
			p = sepp + strspn(sepp, sepchar);
		}

		if (new->nfield == maxsplit) {
			new->fields[new->nfield++] = p;
			break;
		}
	} while (sepc == sepchar[0]);

	return new;
}

/* return a pointer to the n-th field in sp->fields[], starting from 0.
  * n may be negative ('-1' = last field) */
static
char *
getfield(splitline *sp, int n)
{
	if (sp == nil || n >= sp->nfield || abs(n) > sp->nfield)
		return nil;

	if (n < 0)
		return sp->fields[sp->nfield + n];
	else
		return sp->fields[n];
}

/* return a pointer to the first splitline object in array slines of size size where q equals the contents of the nth field.
  * return nil if no matches were found */
static
splitline *
searchfield(splitline **slines, int size, int n, char *q)
{
	int i;

	if (slines == nil || size <= 0 || n < 0 || q == nil)
		return nil;

	for (i = 0; i < size; i++) {
		if (strcmp(slines[i]->fields[n], q) == 0)
			return slines[i];
	}

	return nil;
}

/* destroy a splitline entry. */
static
void
nukesplitline(splitline *sp)
{
	if (sp == nil)
		return;

	free(sp->line);
	free(sp->fields);
	free(sp);

	return;
}

/* deserialise all [General] entries into the appropriate beatmap struct fields.
  * returns 0 on success. */
static
int
hsgeneral(beatmap *bmp, Biobuf *bp)
{
	int i;
	char *line;
	splitline **slines, *sp;
	int nsline;
	int maxsline;

	nsline = 0;
	maxsline = 9;
	slines = ecalloc(maxsline, sizeof(splitline *));
	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ": ", 1, 1);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->audiof = estrrunedup(getfield(searchfield(slines, nsline, 0, "AudioFilename"), VALUE));

	bmp->leadin = (ulong) atol(getfield(searchfield(slines, nsline, 0, "AudioLeadIn"), VALUE));
	bmp->previewt = (ulong) atol(getfield(searchfield(slines, nsline, 0, "PreviewTime"), VALUE));
	bmp->countdown = atoi(getfield(searchfield(slines, nsline, 0, "Countdown"), VALUE));
	bmp->stackleniency = atoi(getfield(searchfield(slines, nsline, 0, "StackLeniency"), VALUE));
	bmp->mode = atoi(getfield(searchfield(slines, nsline, 0, "Mode"), VALUE));
	bmp->letterbox = atoi(getfield(searchfield(slines, nsline, 0, "LetterboxInBreaks"), VALUE));
	bmp->widescreensb = atoi(getfield(searchfield(slines, nsline, 0, "WidescreenStoryboard"), VALUE));
	/* p9p's atof(3) routines don't handle the error string properly, and
	  * some of these values may genuinely be zero. Take a leap of
	  * faith here... */

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

/* deserialise all [Editor] entries into the appropriate beatmap struct fields.
  * returns 0 on success. */
static
int
hseditor(beatmap *bmp, Biobuf *bp)
{
	int i;
	char *line;
	splitline **slines, *sp, *bookmarks;
	int nsline;
	int maxsline;

	nsline = 0;
	maxsline = 5;
	slines = ecalloc(maxsline, sizeof(splitline *));
	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ": ", 1, 1);
		slines[nsline++] = sp;
		free(line);
	}

	bookmarks = searchfield(slines, nsline, 0, "Bookmarks");
	sp = split(getfield(bookmarks, VALUE), ",", 0, 0);
	if (sp->nfield != 0) {
		bmp->bookmarks = ecalloc(sp->nfield, sizeof(ulong));

		for (i = 0; i < sp->nfield; i++)
			bmp->bookmarks[i] = (ulong) atol(sp->fields[i]);

		bmp->nbookmarks = sp->nfield;
	}
	nukesplitline(sp);

	bmp->distancesnap = atof(getfield(searchfield(slines, nsline, 0, "DistanceSpacing"), VALUE));
	bmp->beatdivisor = atof(getfield(searchfield(slines, nsline, 0, "BeatDivisor"), VALUE));
	bmp->gridsize = atoi(getfield(searchfield(slines, nsline, 0, "GridSize"), VALUE));
	bmp->timelinezoom = atof(getfield(searchfield(slines, nsline, 0, "TimelineZoom"), VALUE));

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

/* deserialise all [Metadata] entries into the appropriate beatmap struct fields.
  * returns 0 on success. */
static
int
hsmetadata(beatmap *bmp, Biobuf *bp)
{
	int i;
	char *line;
	splitline **slines, *sp;
	int nsline;
	int maxsline;

	nsline = 0;
	maxsline = 10;
	slines = ecalloc(maxsline, sizeof(splitline *));
	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ":", 1, 1);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->title = estrdup(getfield(searchfield(slines, nsline, 0, "Title"), VALUE));
	bmp->utf8title = strrunedup(getfield(searchfield(slines, nsline, 0, "TitleUnicode"), VALUE));
	bmp->artist = estrdup(getfield(searchfield(slines, nsline, 0, "Artist"), VALUE));
	bmp->utf8artist = strrunedup(getfield(searchfield(slines, nsline, 0, "ArtistUnicode"), VALUE));
	bmp->author = estrdup(getfield(searchfield(slines, nsline, 0, "Creator"), VALUE));
	bmp->diffname = estrdup(getfield(searchfield(slines, nsline, 0, "Version"), VALUE));
	bmp->source = estrdup(getfield(searchfield(slines, nsline, 0, "Source"), VALUE));
	bmp->tags = estrdup(getfield(searchfield(slines, nsline, 0, "Tags"), VALUE));
	bmp->id = atoi(getfield(searchfield(slines, nsline, 0, "BeatmapID"), VALUE));
	bmp->setid = atoi(getfield(searchfield(slines, nsline, 0, "BeatmapSetID"), VALUE));

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

/* deserialise all [Difficulty] entries into the appropriate beatmap struct fields.
  * returns 0 on success.*/
static
int
hsdifficulty(beatmap *bmp, Biobuf *bp)
{
	int i;
	char *line;
	splitline **slines, *sp;
	int nsline;
	int maxsline;

	nsline = 0;
	maxsline = 6;
	slines = ecalloc(maxsline, sizeof(splitline *));
	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ":", 1, 1);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->hp = (float) atof(getfield(searchfield(slines, nsline, 0, "HPDrainRate"), VALUE));
	bmp->cs = (float) atof(getfield(searchfield(slines, nsline, 0, "CircleSize"), VALUE));
	bmp->od = (float) atof(getfield(searchfield(slines, nsline, 0, "OverallDifficulty"), VALUE));
	bmp->ar = (float) atof(getfield(searchfield(slines, nsline, 0, "ApproachRate"), VALUE));
	bmp->slmultiplier = (float) atof(getfield(searchfield(slines, nsline, 0, "SliderMultiplier"), VALUE));
	bmp->sltickrate = atoi(getfield(searchfield(slines, nsline, 0, "SliderTickRate"), VALUE));

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

/* the author doesn't care about storyboarding. As such, hsevents copies the
  * raw [Events] contents into a character string, with carriage returns restored.
  *
  * returns 0 on success */
static
int
hsevents(beatmap *bmp, Biobuf *bp)
{
	int nchar;
	int maxchar;
	int len;
	char *line;

	nchar = 0;
	maxchar = 256;
	bmp->events = ecalloc(maxchar, sizeof(char));

	while ((line = nextentry(bp)) != nil) {
		len = strlen(line);

		/* + 2 for carriage return and newline */
		if (nchar + len + 2 > maxchar) {
			do {
				maxchar *= 2;
			} while (nchar + len + 2 > maxchar);

			bmp->events = erealloc(bmp->events, sizeof(char) * maxchar);
		}

		strcat(bmp->events, line);
		strcat(bmp->events, "\r\n");

		nchar += len + 2;
		free(line);
	}

	return 0;
}

/* deserialise all [TimingPoints] entries into rline and gline objects, and adds
  * them to bmp's list.
  * returns 0 on success, or BADLINE on illegal line type.
  * This routine sets the errstr. */
static
int
hstimingpoints(beatmap *bmp, Biobuf *bp)
{
	char *line;
	splitline *sp;

	int isredline;
	int t, effects, volume, beats;
	int sampset, sampindex;
	double velocity;
	ulong duration;
	gline *glp;
	rline *rlp;

	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ",", 0, 0);

		t = atol(getfield(sp, LNTIME));
		effects = atoi(getfield(sp, LNEFFECTS));
		isredline = atoi(getfield(sp, LNREDLINE));

		volume = atoi(getfield(sp, LNVOLUME));
		sampset = atoi(getfield(sp, LNSAMPSET));
		sampindex = atoi(getfield(sp, LNSAMPINDEX));

		switch (isredline) {
		case 1:
			duration = atol(getfield(sp, LNDURATION));
			beats = atol(getfield(sp, LNBEATS));

			rlp = mkrline(t, duration, beats);

			rlp->volume = volume;
			rlp->sampset = sampset;
			rlp->sampindex = sampindex;

			rlp->kiai = effects & EBKIAI;

			bmp->rlines = addrlinet(bmp->rlines, rlp);

			break;
		case 0:
			velocity = strtod(getfield(sp, LNVELOCITY), nil);

			glp = mkgline(t, velocity);

			glp->volume = volume;
			glp->sampset = sampset;
			glp->sampindex = sampindex;

			glp->kiai = effects & EBKIAI;

			bmp->glines = addglinet(bmp->glines, glp);

			break;
		default:
			/* can't happen */
			werrstr("unknown line type t=%d type=%d", t, isredline);
			nukesplitline(sp);
			free(line);
			return BADLINE;
		}

		nukesplitline(sp);
		free(line);
	}

	return 0;
}

/* convert red, green and blue values to a hexadecimal colour code */
static
long
rgbtohex(int r, int g, int b)
{
	return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF) << 0);
}

/* deserialise all [Colours] entries into hexadecimal colour codes, and
  * adds them to bmp->colours.
  * returns 0 on success, or NOMEM when out of memory. */
static
int
hscolours(beatmap *bmp, Biobuf *bp)
{
	int r, g, b;
	int i;
	char *line, *key;
	splitline **slines, *sp, *csp;
	int nsline;
	int maxsline;

	nsline = 0;
	maxsline = 8;
	slines = ecalloc(maxsline, sizeof(splitline *));
	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ": ", 1, 1);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->colours = ecalloc(nsline, sizeof(long));
	bmp->ncolours = nsline;

	key = ecalloc(sizeof("Combo#"), sizeof(char));

	for (i = 0; i < nsline; i++) {
		sprint(key, "Combo%d", i + 1);
		sp = searchfield(slines, nsline, 0, key);
		csp = split(sp->fields[1], ",", 0, 0);

		r = atoi(csp->fields[RED]);
		g = atoi(csp->fields[GREEN]);
		b = atoi(csp->fields[BLUE]);

		bmp->colours[i] = rgbtohex(r, g, b);
		nukesplitline(csp);
	}

	free(key);

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

/* deserialise all [HitObjects] entries into hitobject objects, and adds
  * them to bmp's list.
  * returns 0 on success, or BADLINE on illegal line type.
  * This routine sets the errstr. */
static
int
hsobjects(beatmap *bmp, Biobuf *bp)
{
	hitobject *op;
	char *line;
	splitline *sp, *curvesp, *esampsp, *esetssp, *hsampsp;

	int t, x, y, typebits, type;

	char *curvetype;
	char *xs, *ys;

	char *nsp, *asp;

	int i;

	while ((line = nextentry(bp)) != nil) {
		sp = split(line, ",", 0, 0);

		t = atol(getfield(sp, OBJTIME));
		x = atoi(getfield(sp, OBJX));
		y = atoi(getfield(sp, OBJY));
		typebits = atoi(getfield(sp, OBJTYPE));
		type = typebits & TBTYPE;

		op = mkobj(type, t, x, y);

		switch (type) {
		case TCIRCLE:
			break;
		case TSLIDER:
			op->slides = atoi(getfield(sp, OBJSLIDES));
			op->length = strtod(getfield(sp, OBJLENGTH), nil);

			curvesp = split(getfield(sp, OBJCURVES), "|", 0, 0);
			curvetype = getfield(curvesp, OBJCURVETYPE);
			op->curve = curvetype[0];
			for (i = 1; i < curvesp->nfield; i++) {
				xs = getfield(curvesp, i);
				ys = xs + strcspn(xs, ":");
				ys[0] = '\0';
				ys++;
				op->alistp = addanchn(op->alistp, mkanch(atoi(xs), atoi(ys)), 0);
			}
			nukesplitline(curvesp);

			op->nsladdition = op->slides + 1;
			op->sladditions = ecalloc(op->nsladdition, sizeof(int));
			esampsp = split(getfield(sp, OBJEDGESOUNDS), "|", 0, 0);
			for (i = 0; i < op->nsladdition; i++)
				op->sladditions[i] = atoi(getfield(esampsp, i));
			nukesplitline(esampsp);

			op->slnormalsets = ecalloc(op->nsladdition, sizeof(int));
			op->sladditionsets = ecalloc(op->nsladdition, sizeof(int));
			esetssp = split(getfield(sp, OBJEDGESETS), "|", 0, 0);
			for (i = 0; i < op->nsladdition; i++) {
				nsp = getfield(esetssp, i);
				asp = nsp + strcspn(nsp, ":");
				asp[0] = '\0';
				asp++;

				op->slnormalsets[i] = atoi(nsp);
				op->sladditionsets[i] = atoi(asp);
			}
			nukesplitline(esetssp);

			break;
		case TSPINNER:
			op->spinnerlength = (ulong) atol(getfield(sp, OBJENDTIME)) - t;

			break;
		default:
			/* can't happen */
			werrstr("object with no type t=%d x=%d y=%d type=%b", t, x, y, type);
			nukesplitline(sp);
			free(line);
			return BADOBJECT;
		}

		hsampsp = split(getfield(sp, OBJHITSAMP), ":", 0, 0);
		op->normalset = atoi(getfield(hsampsp, HITSAMPNORMAL));
		op->additionset = atoi(getfield(hsampsp, HITSAMPADDITIONS));
		op->sampindex = atoi(getfield(hsampsp, HITSAMPINDEX));
		op->volume = atoi(getfield(hsampsp, HITSAMPVOLUME));
		op->filename = estrrunedup(getfield(hsampsp, HITSAMPFILE));
		op->additions = atoi(getfield(sp, OBJADDITIONS));
		nukesplitline(hsampsp);

		if (typebits & TBNEWCOMBO) {
			op->newcombo = 1;
			op->comboskip = (typebits & TBCOLOR) >> TBCOLORSHIFT;
		}

		bmp->objects = addobjt(bmp->objects, op);

		nukesplitline(sp);
		free(line);
	}

	return 0;
}

/* creates a new beatmap object. returns nil when out of memory. */
beatmap *
mkbeatmap()
{
	beatmap *new = ecalloc(1, sizeof(beatmap));

	return new;
}

/* frees a beatmap object, INCLUDING all objects, red- and greenlines, and strings. */
void
nukebeatmap(beatmap *bmp)
{
	rline *rlp, *rnext;
	gline *glp, *gnext;
	hitobject *op, *onext;

	free(bmp->audiof);

	free(bmp->bookmarks);

	free(bmp->title);
	free(bmp->utf8title);
	free(bmp->artist);
	free(bmp->utf8artist);
	free(bmp->author);
	free(bmp->diffname);
	free(bmp->source);
	free(bmp->tags);

	free(bmp->events);

	for (rlp = bmp->rlines; rlp != nil; rlp = rnext) {
		rnext = rlp->next;
		nukerline(rlp);
	}

	for (glp = bmp->glines; glp != nil; glp = gnext) {
		gnext = glp->next;
		nukegline(glp);
	}

	free(bmp->colours);

	for (op = bmp->objects; op != nil; op = onext) {
		onext = op->next;
		nukeobj(op);
	}
}

int
loadmap(beatmap *bmp, Biobuf *bp)
{
	char *s;
	int i;
	int exit;
	int nhandler = sizeof(handlers) / sizeof(handler);

	while ((s = nextsection(bp)) != nil) {
		for (i = 0; i < nhandler; i++) {
			if (strcmp(s, handlers[i].section) == 0)
				if ((exit = handlers[i].read(bmp, bp)) < 0)
					return exit;
		}
		free(s);
	}

	return 0;
}