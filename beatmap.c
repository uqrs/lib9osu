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
static int rgeneral(beatmap *bmp, Biobuf *bp);
static int reditor(beatmap *bmp, Biobuf *bp);
static int rmetadata(beatmap *bmp, Biobuf *bp);
static int rdifficulty(beatmap *bmp, Biobuf *bp);
static int revents(beatmap *bmp, Biobuf *bp);
static int rtimingpoints(beatmap *bmp, Biobuf *bp);
static int rcolours(beatmap *bmp, Biobuf *bp);
static int robjects(beatmap *bmp, Biobuf *bp);

static int wgeneral(beatmap *bmp, Biobuf *bp);
static int weditor(beatmap *bmp, Biobuf *bp);
static int wmetadata(beatmap *bmp, Biobuf *bp);
static int wdifficulty(beatmap *bmp, Biobuf *bp);
static int wevents(beatmap *bmp, Biobuf *bp);
static int wtimingpoints(beatmap *bmp, Biobuf *bp);
static int wcolours(beatmap *bmp, Biobuf *bp);
static int wobjects(beatmap *bmp, Biobuf *bp);

typedef struct handler {
	char *section;					/* section name, e.g. "[General]" */
	int (*read)(beatmap *, Biobuf *);	/* read handler for this section, e.g. rgeneral */
	int (*write)(beatmap *, Biobuf *);	/* write handler for this section, e.g. wgeneral */
} handler;

static handler handlers[] = {
	{.section = "[General]", .read = rgeneral, .write = wgeneral},
	{.section = "[Editor]", .read = reditor, .write = weditor},
	{.section = "[Metadata]", .read = rmetadata, .write = wmetadata},
	{.section = "[Difficulty]", .read = rdifficulty, .write = wdifficulty},
	{.section = "[Events]", .read = revents, .write = wevents},
	{.section = "[TimingPoints]", .read = rtimingpoints, .write = wtimingpoints},
	{.section = "[Colours]", .read = rcolours, .write = wcolours},
	{.section = "[HitObjects]", .read = robjects, .write = wobjects},
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
  *
  * returns a pointer to a splitline struct dynamically allocated with malloc().
  */
static
splitline *
csvsplit(char *line, char *sepchar)
{
	int maxfield;
	splitline *new;
	char *sepp;
	char sepc;
	char *p;

	if (line[0] == '\0' || sepchar == nil)
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

		sepp++;
		p = sepp + strspn(sepp, sepchar);
	} while (sepc == sepchar[0]);

	return new;
}

/* split a copy of line[] into two distinct fields, delimited by the very first ':'. This routine also trims whitespace
  * around the delimiter.
  *
  * returns a pointer to a splitline struct dynamically allocated with malloc(). */
static
splitline *
kvsplit(char *line)
{
	char *k, *v;
	splitline *new;

	if (line == nil || line[0] == '\0')
		return nil;

	new = ecalloc(1, sizeof(splitline));
	new->nfield = 2;
	new->fields = ecalloc(2, sizeof(char *));

	new->line = k = estrdup(line);
	v = k + strcspn(k, ":");
	v[0] = '\0';
	v++;

	v += strspn(v, " 	");
	k[strcspn(k, " 	")] = '\0';

	new->fields[KEY] = k;
	new->fields[VALUE] = v;

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
rgeneral(beatmap *bmp, Biobuf *bp)
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
		sp = kvsplit(line);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->audiof = estrrunedup(getfield(searchfield(slines, nsline, 0, "AudioFilename"), VALUE));

	bmp->leadin = (ulong) atol(getfield(searchfield(slines, nsline, 0, "AudioLeadIn"), VALUE));
	bmp->previewt = (ulong) atol(getfield(searchfield(slines, nsline, 0, "PreviewTime"), VALUE));
	bmp->countdown = atoi(getfield(searchfield(slines, nsline, 0, "Countdown"), VALUE));
	bmp->sampset = estrdup(getfield(searchfield(slines, nsline, 0, "SampleSet"), VALUE));
	bmp->stackleniency = atof(getfield(searchfield(slines, nsline, 0, "StackLeniency"), VALUE));
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

static
int
wgeneral(beatmap *bmp, Biobuf *bp)
{

	Bprint(bp, "AudioFilename: %S\r\n", bmp->audiof);
	Bprint(bp, "AudioLeadIn: %uld\r\n", bmp->leadin);
	Bprint(bp, "PreviewTime: %uld\r\n", bmp->previewt);
	Bprint(bp, "Countdown: %d\r\n", bmp->countdown);
	Bprint(bp, "SampleSet: %s\r\n", bmp->sampset);
	Bprint(bp, "StackLeniency: %.3g\r\n", bmp->stackleniency);
	Bprint(bp, "Mode: %d\r\n", bmp->mode);
	Bprint(bp, "LetterboxInBreaks: %d\r\n", bmp->letterbox);
	Bprint(bp, "WidescreenStoryboard: %d\r\n", bmp->widescreensb);

	return 0;
}

/* deserialise all [Editor] entries into the appropriate beatmap struct fields.
  * returns 0 on success. */
static
int
reditor(beatmap *bmp, Biobuf *bp)
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
		sp = kvsplit(line);
		slines[nsline++] = sp;
		free(line);
	}

	bookmarks = searchfield(slines, nsline, 0, "Bookmarks");
	sp = csvsplit(getfield(bookmarks, VALUE), ",");
	if (sp->nfield != 0) {
		bmp->bookmarks = ecalloc(sp->nfield, sizeof(ulong));

		for (i = 0; i < sp->nfield; i++)
			bmp->bookmarks[i] = (ulong) atol(sp->fields[i]);

		bmp->nbookmark = sp->nfield;
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

static
int
weditor(beatmap *bmp, Biobuf *bp)
{
	int i;

	Bprint(bp, "Bookmarks: %uld", bmp->bookmarks[0]);
	for (i = 1; i < bmp->nbookmark; i++)
		Bprint(bp, ",%uld", bmp->bookmarks[i]);
	Bprint(bp, "\r\n");

	Bprint(bp, "DistanceSpacing: %.6g\r\n", bmp->distancesnap);
	Bprint(bp, "BeatDivisor: %.8g\r\n", bmp->beatdivisor);
	Bprint(bp, "GridSize: %d\r\n", bmp->gridsize);
	Bprint(bp, "TimelineZoom: %.7g\r\n", bmp->timelinezoom);

	return 0;
}

/* deserialise all [Metadata] entries into the appropriate beatmap struct fields.
  * returns 0 on success. */
static
int
rmetadata(beatmap *bmp, Biobuf *bp)
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
		sp = kvsplit(line);
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

static
int
wmetadata(beatmap *bmp, Biobuf *bp)
{
	Bprint(bp, "Title:%s\r\n", bmp->title);
	Bprint(bp, "TitleUnicode:%S\r\n", bmp->utf8title);
	Bprint(bp, "Artist:%s\r\n", bmp->artist);
	Bprint(bp, "ArtistUnicode:%S\r\n", bmp->utf8artist);
	Bprint(bp, "Creator:%s\r\n", bmp->author);
	Bprint(bp, "Version:%s\r\n", bmp->diffname);
	Bprint(bp, "Source:%s\r\n", bmp->source);
	Bprint(bp, "Tags:%s\r\n", bmp->tags);
	Bprint(bp, "BeatmapID:%d\r\n", bmp->id);
	Bprint(bp, "BeatmapSetID:%d\r\n", bmp->setid);

	return 0;
}


/* deserialise all [Difficulty] entries into the appropriate beatmap struct fields.
  * returns 0 on success.*/
static
int
rdifficulty(beatmap *bmp, Biobuf *bp)
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
		sp = kvsplit(line);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->hp = (float) atof(getfield(searchfield(slines, nsline, 0, "HPDrainRate"), VALUE));
	bmp->cs = (float) atof(getfield(searchfield(slines, nsline, 0, "CircleSize"), VALUE));
	bmp->od = (float) atof(getfield(searchfield(slines, nsline, 0, "OverallDifficulty"), VALUE));
	bmp->ar = (float) atof(getfield(searchfield(slines, nsline, 0, "ApproachRate"), VALUE));
	bmp->slmultiplier = strtod(getfield(searchfield(slines, nsline, 0, "SliderMultiplier"), VALUE), nil);
	bmp->sltickrate = atoi(getfield(searchfield(slines, nsline, 0, "SliderTickRate"), VALUE));

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

static
int
wdifficulty(beatmap *bmp, Biobuf *bp)
{
	Bprint(bp, "HPDrainRate:%.3g\r\n", bmp->hp);
	Bprint(bp, "CircleSize:%.3g\r\n", bmp->cs);
	Bprint(bp, "OverallDifficulty:%.3g\r\n", bmp->od);
	Bprint(bp, "ApproachRate:%.3g\r\n", bmp->ar);
	Bprint(bp, "SliderMultiplier:%.15g\r\n", bmp->slmultiplier);
	Bprint(bp, "SliderTickRate:%d\r\n", bmp->sltickrate);

	return 0;
}

/* the author doesn't care about storyboarding. As such, hsevents copies the
  * raw [Events] contents into a character string, with carriage returns restored.
  *
  * returns 0 on success */
static
int
revents(beatmap *bmp, Biobuf *bp)
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

static
int
wevents(beatmap *bmp, Biobuf *bp)
{
	Bprint(bp, "%s", bmp->events);

	return 0;
}

/* deserialise all [TimingPoints] entries into rline and gline objects, and adds
  * them to bmp's list.
  * returns 0 on success, or BADLINE on illegal line type.
  * This routine sets the errstr. */
static
int
rtimingpoints(beatmap *bmp, Biobuf *bp)
{
	char *line;
	splitline *sp;

	int isredline;
	int t, effects, volume, beats;
	int sampset, sampindex;
	double velocity, duration;
	gline *glp;
	rline *rlp;

	while ((line = nextentry(bp)) != nil) {
		sp = csvsplit(line, ",");

		t = atol(getfield(sp, LNTIME));
		effects = atoi(getfield(sp, LNEFFECTS));
		isredline = atoi(getfield(sp, LNREDLINE));

		volume = atoi(getfield(sp, LNVOLUME));
		sampset = atoi(getfield(sp, LNSAMPSET));
		sampindex = atoi(getfield(sp, LNSAMPINDEX));

		switch (isredline) {
		case 1:
			duration = strtod(getfield(sp, LNDURATION), nil);
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
			beats = atol(getfield(sp, LNBEATS));

			glp = mkgline(t, velocity);

			glp->volume = volume;
			glp->sampset = sampset;
			glp->sampindex = sampindex;

			glp->beats = beats;
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

static
int
wtimingpoints(beatmap *bmp, Biobuf *bp)
{
	rline *rlp;
	gline *glp;

	rlp = bmp->rlines;
	glp = bmp->glines;

	do {
		for (; rlp != nil && (glp == nil || (rlp->t <= glp->t)); rlp = rlp->next)
			Bprint(bp, "%uld,%.16g,%d,%d,%d,%d,1,%d\r\n", rlp->t, rlp->duration, rlp->beats, rlp->sampset, rlp->sampindex, rlp->volume, rlp->kiai);

		for (; glp != nil && (rlp == nil || (glp->t < rlp->t)); glp = glp->next)
			Bprint(bp, "%uld,%.16g,%d,%d,%d,%d,0,%d\r\n", glp->t, glp->velocity, glp->beats, glp->sampset, glp->sampindex, glp->volume, glp->kiai);
	} while (glp != nil || rlp != nil);

	/* *two* cr-lfs follow [TimingPoints]. Why? Fuck you. */
	Bprint(bp, "\r\n");

	return 0;
}

/* deserialise all [Colours] entries into hexadecimal colour codes, and
  * adds them to bmp->colours.
  * returns 0 on success, or NOMEM when out of memory. */
static
int
rcolours(beatmap *bmp, Biobuf *bp)
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
		sp = kvsplit(line);
		slines[nsline++] = sp;
		free(line);
	}

	bmp->colours = ecalloc(nsline, sizeof(long));
	bmp->ncolour = nsline;

	key = ecalloc(sizeof("Combo#"), sizeof(char));

	for (i = 0; i < nsline; i++) {
		sprint(key, "Combo%d", i + 1);
		sp = searchfield(slines, nsline, 0, key);
		csp = csvsplit(sp->fields[1], ",");

		r = atoi(csp->fields[RED]);
		g = atoi(csp->fields[GREEN]);
		b = atoi(csp->fields[BLUE]);

		bmp->colours[i] = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF) << 0);
		
		nukesplitline(csp);
	}

	free(key);

	for (i = 0; i < nsline; i++)
		nukesplitline(slines[i]);

	return 0;
}

static
int
wcolours(beatmap *bmp, Biobuf *bp)
{
	int i;
	long c;

	for (i = 0; i < bmp->ncolour; i++) {
		c = bmp->colours[i];
		Bprint(bp, "Combo%d : %d,%d,%d\r\n", i + 1, c>>16, c>>8 & 0xFF, c & 0xFF);
	}

	return 0;
}

/* deserialise all [HitObjects] entries into hitobject objects, and adds
  * them to bmp's list.
  * returns 0 on success, or BADLINE on illegal line type.
  * This routine sets the errstr. */
static
int
robjects(beatmap *bmp, Biobuf *bp)
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
		sp = csvsplit(line, ",");

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

			curvesp = csvsplit(getfield(sp, OBJCURVES), "|");
			curvetype = getfield(curvesp, OBJCURVETYPE);
			op->curve = curvetype[0];
			for (i = 1; i < curvesp->nfield; i++) {
				xs = getfield(curvesp, i);
				ys = xs + strcspn(xs, ":");
				ys[0] = '\0';
				ys++;
				op->anchors = addanchn(op->anchors, mkanch(atoi(xs), atoi(ys)), 0);
			}
			nukesplitline(curvesp);

			/* the OBJEDGESOUNDS and OBJEDGESETS fields are OPTIONAL, and will be
			  * MISSING if the slider has NO HITSOUNDS!!!
			  *
			  * I will pay $100 to the first person to slam a pie into peppy's face. */
			if (sp->nfield > OBJEDGESOUNDS) {
				op->nsladdition = op->slides + 1;
				op->sladditions = ecalloc(op->nsladdition, sizeof(int));
				esampsp = csvsplit(getfield(sp, OBJEDGESOUNDS), "|");
				for (i = 0; i < op->nsladdition; i++)
					op->sladditions[i] = atoi(getfield(esampsp, i));
				nukesplitline(esampsp);

				op->slnormalsets = ecalloc(op->nsladdition, sizeof(int));
				op->sladditionsets = ecalloc(op->nsladdition, sizeof(int));
				esetssp = csvsplit(getfield(sp, OBJEDGESETS), "|");
				for (i = 0; i < op->nsladdition; i++) {
					nsp = getfield(esetssp, i);
					asp = nsp + strcspn(nsp, ":");
					asp[0] = '\0';
					asp++;
	
					op->slnormalsets[i] = atoi(nsp);
					op->sladditionsets[i] = atoi(asp);
				}
				nukesplitline(esetssp);
			} else {
				op->nsladdition = 0;
			}

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

		if (op->type != TSLIDER || sp->nfield > OBJEDGESOUNDS) {
			hsampsp = csvsplit(getfield(sp, OBJHITSAMP), ":");
			op->normalset = atoi(getfield(hsampsp, HITSAMPNORMAL));
			op->additionset = atoi(getfield(hsampsp, HITSAMPADDITIONS));
			op->sampindex = atoi(getfield(hsampsp, HITSAMPINDEX));
			op->volume = atoi(getfield(hsampsp, HITSAMPVOLUME));
			op->filename = estrrunedup(getfield(hsampsp, HITSAMPFILE));
			op->additions = atoi(getfield(sp, OBJADDITIONS));
			nukesplitline(hsampsp);
		}

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

static
int
wobjects(beatmap *bmp, Biobuf *bp)
{
	int i;
	hitobject *op;
	anchor *ap;
	int typebits;
	char *hitsamp;

	for (op = bmp->objects; op != nil; op = op->next) {
		typebits = op->type;
		if (op->newcombo == 1)
			typebits |= TBNEWCOMBO | (op->comboskip << TBCOLORSHIFT);

		Bprint(bp, "%d,%d,%uld,%d,%d", op->x, op->y, op->t, typebits, op->additions);

		switch(op->type) {
		case TCIRCLE:
			break;
		case TSLIDER:
			Bprint(bp, ",%c", op->curve);
			for (ap = op->anchors; ap != nil; ap = ap->next)
				Bprint(bp, "|%d:%d", ap->x, ap->y);

			Bprint(bp, ",%d,%.16g", op->slides, op->length);

			if (op->nsladdition > 0) {
				Bprint(bp, ",%d", op->sladditions[0]);
				for (i = 1; i < op->nsladdition; i++)
					Bprint(bp, "|%d", op->sladditions[i]);
	
				Bprint(bp, ",%d:%d", op->slnormalsets[0], op->sladditionsets[0]);
				for (i = 1; i < op->nsladdition; i++)
					Bprint(bp, "|%d:%d", op->slnormalsets[i], op->sladditionsets[i]);
			}

			break;
		case TSPINNER:
			Bprint(bp, ",%uld", op->t + op->spinnerlength);

			break;
		}

		if (op->type != TSLIDER || op->nsladdition > 0) {
			hitsamp = smprint(",%d:%d:%d:%d:%S", op->normalset, op->additionset, op->sampindex, op->volume, op->filename);
			Bprint(bp, "%s", hitsamp);
			free(hitsamp);
		}

		Bprint(bp, "\r\n");
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

int
writemap(beatmap *bmp, Biobuf *bp)
{
	int i;
	int exit;
	int nhandler = sizeof(handlers) / sizeof(handler);

	Bprint(bp, "%s\r\n", "osu file format v14");

	for (i = 0; i < nhandler; i++) {
		Bprint(bp, "\r\n%s\r\n", handlers[i].section);
		if ((exit = handlers[i].write(bmp, bp)) < 0)
			return exit;


	}

	return 0;
}