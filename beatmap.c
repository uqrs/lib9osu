#include <u.h>
#include <libc.h>
#include <bio.h>
#include "aux.h"
#include "hash.h"
#include "hitsound.h"
#include "hitobject.h"
#include "rgbline.h"
#include "beatmap.h"

/* References: 
  * https://osu.ppy.sh/wiki/en/osu%21_File_Formats/Osu_%28file_format%29 
  * example/destrier.osu
  * example/crush.osu
  * example/garden.osu
  * example/neoprene.osu */

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

typedef struct typespec {
	char *key;		/* string key of entry */
	char *fmt;		/* format string for output */
	int type;		/* the data type of the associated value. see hash.h:2,9 */
} typespec;

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

static typespec fgeneral[] = {
	/* [General] */
	{.key = "AudioFilename", .fmt = "%S", .type = TRUNE},
	{.key = "AudioLeadIn", .fmt = "%ld", .type = TLONG},
	{.key = "PreviewTime", .fmt = "%ld", .type = TLONG},
	{.key = "Countdown", .fmt = "%d", .type = TINT},
	{.key = "SampleSet", .fmt = "%s", .type = TSTRING},
	{.key = "StackLeniency", .fmt = "%g", .type = TFLOAT},
	{.key = "Mode", .fmt = "%d", .type = TINT},
	{.key = "LetterboxInBreaks", .fmt = "%d", .type = TINT},
	{.key = "WidescreenStoryboard", .fmt = "%d", .type = TINT},
};

static typespec feditor[] = {
	/* [Editor] */
	{.key = "Bookmarks", .fmt = "%s", .type = TSTRING},
	{.key = "DistanceSpacing", .fmt = "%g", .type = TFLOAT},
	{.key = "BeatDivisor", .fmt = "%g", .type = TFLOAT},
	{.key = "GridSize", .fmt = "%d", .type = TINT},
	{.key = "TimelineZoom", .fmt = "%.7g", .type = TFLOAT},
};

static typespec fmetadata[] = {
	/* [Metadata] */
	{.key = "Title", .fmt = "%s", .type = TSTRING},
	{.key = "TitleUnicode", .fmt = "%S", .type = TRUNE},
	{.key = "Artist", .fmt = "%s", .type = TSTRING},
	{.key = "ArtistUnicode", .fmt = "%S", .type = TRUNE},
	{.key = "Creator", .fmt = "%s", .type = TSTRING},
	{.key = "Version", .fmt = "%s", .type = TSTRING},
	{.key = "Source", .fmt = "%s", .type = TSTRING},
	{.key = "Tags", .fmt = "%s", .type = TSTRING},
	{.key = "BeatmapID", .fmt = "%d", .type = TINT},
	{.key = "BeatmapSetID", .fmt = "%d", .type = TINT},
};

static typespec fdifficulty[] = {
	/* [Difficulty] */
	{.key = "HPDrainRate", .fmt = "%.3g", .type = TFLOAT},
	{.key = "CircleSize", .fmt = "%.3g", .type = TFLOAT},
	{.key = "OverallDifficulty", .fmt = "%.3g", .type = TFLOAT},
	{.key = "ApproachRate", .fmt = "%.3g", .type = TFLOAT},
	{.key = "SliderMultiplier", .fmt = "%.15g", .type = TDOUBLE},
	{.key = "SliderTickRate", .fmt = "%.3g", .type = TFLOAT},
};

static typespec fcolours[] = {
	/* [Colours] */
	{.key = "Combo1", .fmt = "%s", .type = TSTRING},
	{.key = "Combo2", .fmt = "%s", .type = TSTRING},
	{.key = "Combo3", .fmt = "%s", .type = TSTRING},
	{.key = "Combo4", .fmt = "%s", .type = TSTRING},
	{.key = "Combo5", .fmt = "%s", .type = TSTRING},
	{.key = "Combo6", .fmt = "%s", .type = TSTRING},
	{.key = "Combo7", .fmt = "%s", .type = TSTRING},
	{.key = "Combo8", .fmt = "%s", .type = TSTRING},
	{.key = "SliderTrackOverride", .fmt = "%s", .type = TSTRING},
	{.key = "SliderBorder", .fmt = "%s", .type = TSTRING},
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
	OBJX=0,				/* x position of object */
	OBJY,				/* y position of object */
	OBJTIME,				/* timestamp in milliseconds */
	OBJTYPE,				/* 8-bit integer describing object type & colours; see enum typebits */
	OBJADDITIONS,		/* hitsound additions for object */
	OBJENDTIME=5,		/* spinner end timestamp */
	OBJCURVES=5,			/* slider curve data */
	OBJCIRCLEHITSAMP=5,	/* custom hitsound sampleset (circles) */
	OBJSPINNERHITSAMP=6,	/* custom hitsound sampleset (spinners) */
	OBJSLIDES=6,			/* amount of reverses + 1 */
	OBJLENGTH,			/* ""visual length in osu! pixels"" */
	OBJEDGESOUNDS,		/* slider head/tail hitsounds */
	OBJEDGESETS,			/* slider head/tail sample sets */
	OBJSLIDERHITSAMP		/* custom hitsound sampleset (sliders) */
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
	LNVORD=1,		/* duration of beat in miliseconds (redline), or negative inverse slider velocity multiplier (greenline) */
	LNBEATS,			/* number of beats in measure */
	LNSAMPSET,		/* default hitobject sampleset */
	LNSAMPINDEX,		/* custom sample index */
	LNVOLUME,		/* volume percentage */
	LNTYPE,			/* one of rgbline.h:0/linetype/ */
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

/* slider curve csv fields */
enum {
	X=0,
	Y,
};

/* slider edgesample csv fields */
enum {
	SLNORMSET=0,
	SLADDSET,
};

/* Possible values for LNSAMPSET field. Indices correspond to hitsound.h:/sampsets/ */
char *samplesets[] = {
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
		if (line[strlen(line) - 1] == '\r')
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
advquoted(char *p, char *sep)
{
	int i,j;
	for (i = j = 0; p[j] != '\0'; i++, j++) {
		if (p[j] == '"' && p[++j] != '"') {
			int k = strcspn(p+j, sep);
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

/* split a copy of line[] into separate fields delimited by any of the characters in sep.
  * returns a pointer to a splitline struct dynamically allocated with malloc(). */
static
splitline *
csvsplit(char *line, char *sep)
{
	int maxfield;
	splitline *new;
	char *sepp;
	char sepc;
	char *p;

	if (line == nil || line[0] == '\0' || sep == nil)
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
			sepp = advquoted(++p, sep);
		else
			sepp = p + strcspn(p, sep);

		sepc = sepp[0];
		sepp[0] = '\0';
		new->fields[new->nfield++] = p;

		sepp++;
		p = sepp + strspn(sepp, sep);
	} while (sepc == sep[0]);

	return new;
}

/* split a copy of line[] into two distinct fields, delimited by the
  * first instance of any characters in sep. This routine also
  * trims whitespace around the delimiter.
  * returs a pointer to a splitline struct dynamically allocated with malloc(). */
static
splitline *
kvsplit(char *line, char *sep)
{
	char *k, *v;
	splitline *new;

	if (line == nil || line[0] == '\0' || sep == nil)
		return nil;

	new = ecalloc(1, sizeof(splitline));
	new->nfield = 2;
	new->fields = ecalloc(2, sizeof(char *));

	new->line = k = estrdup(line);
	v = k + strcspn(k, sep);
	v[0] = '\0';
	v++;

	v += strspn(v, " 	");
	k[strcspn(k, " 	")] = '\0';

	new->fields[KEY] = k;
	new->fields[VALUE] = v;

	return new;
}

/* send a splitline to the shadow realm. */
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

/* search through an nspec-sized speclist[], and return a pointer
  * to the first typespec in speclist[] whose key value matches the final argument
  * returns nil if no matches were found */
static
typespec *
lookupspec(typespec *speclist, int nspec, char *key)
{
	int i;

	if (key == nil || nspec <= 0)
		return nil;

	for (i = 0; i < nspec; i++)
		if (strcmp(speclist[i].key, key) == 0)
			return &speclist[i];

	return nil;
}

/* read beatmap entries from bp up until the next blank line
  * add the entry to tp with the corresponding typespec in speclist's
  * .type value.
  * returns 0 on success, or BADKEY if key is not in speclist.
  * this routine sets errstr. */
static
int
readentries(table *tp, Biobuf *bp, typespec *speclist, int nspec)
{
	char *line;
	splitline *sp;
	typespec *specp;

	if (tp == nil || bp == nil || speclist == nil || nspec <= 0)
		return -1;

	while ((line = nextentry(bp)) != nil) {
		sp = kvsplit(line, ":");

		specp = lookupspec(speclist, nspec, sp->fields[KEY]);
		if (specp == nil) {
			werrstr("Unknown configuration key '%s'\n", sp->fields[KEY]);
			free(line);
			nukesplitline(sp);

			return BADKEY;
		}

		addentry(tp, mkentry(sp->fields[KEY], sp->fields[VALUE], specp->type));

		free(line);
		nukesplitline(sp);
	}

	return 0;
}

/* for each typespec in speclist, look for an entry in tp whose key value matches
  * the  's .fmt field. write each entry's value to bp using the typespec's format
  * argument .fmt, and the given separator sep.
  * returns 0 on success. */
static
int
writeentries(table *tp, Biobuf *bp, typespec *speclist, int nspec, char *sep)
{
	entry *ep;
	int i;

	if (tp == nil || bp == nil || speclist == nil || nspec <= 0 || sep == nil)
		return -1;

	for (i = 0; i < nspec; i++) {
		ep = lookup(tp, speclist[i].key);
		if (ep != nil) {
			Bprint(bp, "%s%s", speclist[i].key, sep);
			switch(ep->type) {
			case TRUNE:
				Bprint(bp, speclist[i].fmt, ep->S);
				break;
			case TSTRING:
				Bprint(bp, speclist[i].fmt, ep->s);
				break;
			case TINT:
				Bprint(bp, speclist[i].fmt, ep->i);
				break;
			case TLONG:
				Bprint(bp, speclist[i].fmt, ep->l);
				break;
			case TFLOAT:
				Bprint(bp, speclist[i].fmt, ep->f);
				break;
			case TDOUBLE:
				Bprint(bp, speclist[i].fmt, ep->d);
				break;
			}
			Bprint(bp, "\r\n");
		}
	}

	return 0;
}

/* read all entries from the [General] section, and add them to
  * table *bmp->general.
  * returns readentries' exit code. */
static
int
rgeneral(beatmap *bmp, Biobuf *bp)
{
	int nspec;

	if (bmp->general == nil)
		bmp->general = mktable(8);
	nspec = sizeof(fgeneral) / sizeof(typespec);
	return readentries(bmp->general, bp, fgeneral, nspec);
}

/* write all entries from table *bmp->general to bp.
  * returns writeentries' exit code. */
static
int
wgeneral(beatmap *bmp, Biobuf *bp)
{
	int nspec;

	if (bmp->general == nil)
		return 0;

	Bprint(bp, "\r\n[General]\r\n");
	nspec = sizeof(fgeneral) / sizeof(typespec);
	return writeentries(bmp->general, bp, fgeneral, nspec, ": ");
}

/* read all entries from the [Editor] section, and add them to
  * table *bmp->editor.
  * deserialise bookmarks' value into a list of bmp->nbookmarks
  * number of longs, now assigned to bmp->bookmarks, and remove
  * the "Bookmarks" entry from bmp->editor
  * returns readentries' exit code on failure, or 0 on success. */
static
int
reditor(beatmap *bmp, Biobuf *bp)
{
	int i, nspec, exit;
	entry *ep;
	splitline *sp;

	if (bmp->editor == nil)
		bmp->editor = mktable(8);

	nspec = sizeof(feditor) / sizeof(typespec);
	exit = readentries(bmp->editor, bp, feditor, nspec);
	if (exit < 0)
		return exit;

	ep = lookup(bmp->editor, "Bookmarks");
	sp = csvsplit(ep->s, ",");
	if (sp->nfield > 0) {
		bmp->bookmarks = ecalloc(sp->nfield, sizeof(long));
		for (i = 0; i < sp->nfield; i++)
			bmp->bookmarks[i] = atol(sp->fields[i]);

		bmp->nbookmark = sp->nfield;
	}
	nukesplitline(sp);
	nukeentry(rmentry(bmp->editor, ep));

	return 0;
}

/* write all entries from table *bmp->general to bp, including
  * a comma-separated string representation of 
  * long *bmp->bookmarks for the "Bookmarks" entry.
  * returns writeentries' exit code. */
static
int
weditor(beatmap *bmp, Biobuf *bp)
{
	int i, nspec;
	char *ulongmax;
	char *bookmarks, *p;

	if (bmp->editor == nil)
		return 0;

	if (bmp->nbookmark > 0) {
		ulongmax = smprint("%uld", (ulong)-1);
		bookmarks = p = ecalloc((strlen(ulongmax)+1)*bmp->nbookmark + 1, sizeof(char));
		free(ulongmax);

		for (i = 0; i < bmp->nbookmark; i++) {
			sprint(p, ",%ld", bmp->bookmarks[i]);
			for (;*p != '\0'; p++)
				;
		}
		p = bookmarks + 1;

		addentry(bmp->editor, mkentry("Bookmarks", p, TSTRING));
		free(bookmarks);
	}

	nspec = sizeof(feditor) / sizeof(typespec);

	Bprint(bp, "\r\n[Editor]\r\n");
	return writeentries(bmp->editor, bp, feditor, nspec, ": ");
}

/* read all entries from the [Metadata] section, and add them tp
  * table *bmp->metadata.
  * returns readentries' exit code. */
static
int
rmetadata(beatmap *bmp, Biobuf *bp)
{
	int nspec;

	if (bmp->metadata == nil)
		bmp->metadata = mktable(8);
	nspec = sizeof(fmetadata) / sizeof(typespec);
	return readentries(bmp->metadata, bp, fmetadata, nspec);
}

/* write all entries from table *bmp->general to bp.
  * returns writeentries' exit code. */
static
int
wmetadata(beatmap *bmp, Biobuf *bp)
{
	int nspec;

	if (bmp->metadata == nil)
		return 0;

	Bprint(bp, "\r\n[Metadata]\r\n");
	nspec = sizeof(fmetadata) / sizeof(typespec);
	return writeentries(bmp->metadata, bp, fmetadata, nspec, ":");
}

/* read all entries from the [Difficulty] section, and add them to
  * table *bmp->difficulty.
  * returns readentries' exit code. */
static
int
rdifficulty(beatmap *bmp, Biobuf *bp)
{
	int nspec;

	if (bmp->difficulty == nil)
		bmp->difficulty = mktable(8);
	nspec = sizeof(fdifficulty) / sizeof(typespec);
	return readentries(bmp->difficulty, bp, fdifficulty, nspec);
}

/* write all entries from table *bmp->difficulty to bp.
  * returns writeentries' exit code. */
static
int
wdifficulty(beatmap *bmp, Biobuf *bp)
{
	int nspec;

	if (bmp->difficulty == nil)
		return 0;

	Bprint(bp, "\r\n[Difficulty]\r\n");
	nspec = sizeof(fdifficulty) / sizeof(typespec);
	return writeentries(bmp->difficulty, bp, fdifficulty, nspec, ":");
}

/* the author doesn't care about storyboarding. As such, revents copies the
  * raw [Events] contents into a char *bmp->events, with carriage returns restored.
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

/* write the raw contents of *bmp->events to bp.
  * returns 0 on success. */
static
int
wevents(beatmap *bmp, Biobuf *bp)
{
	Bprint(bp,"\r\n[Events]\r\n");
	Bprint(bp, "%s", bmp->events);

	return 0;
}
/* deserialise all [TimingPoints] entries into rgline objects, and add
  * them to list rline *bmp->rglines.
  * returns 0 on success, or BADLINE on .osu syntax errors.
  * This routine sets errstr. */
static
int
rtimingpoints(beatmap *bmp, Biobuf *bp)
{
	char *line;
	rgline *lp;
	splitline *sp;
	int t, type, beats;
	double vord;

	while ((line = nextentry(bp)) != nil) {
		sp = csvsplit(line, ",");
		if (sp->nfield != LNEFFECTS+1) {
			print("nfield: %d; lneffects: %d\n", sp->nfield, LNEFFECTS);
			goto badline;
		}

		t = atol(sp->fields[LNTIME]);
		vord = strtod(sp->fields[LNVORD], nil);
		beats = sfatoi(sp->fields[LNBEATS]);
		type = sfatoi(sp->fields[LNTYPE]);

		lp = mkrgline(t, vord, beats, type);
		if ((lp = mkrgline(t, vord, beats, type)) == nil)
			goto badline;

		lp->volume = sfatoi(sp->fields[LNVOLUME]);
		lp->sampset = sfatoi(sp->fields[LNSAMPSET]);
		lp->sampindex = sfatoi(sp->fields[LNSAMPINDEX]);
		lp->kiai = sfatoi(sp->fields[LNEFFECTS]) & EBKIAI;

		bmp->rglines = addrglinet(bmp->rglines, lp);

		nukesplitline(sp);
		free(line);
	}

	return 0;

badline:
	werrstr("malformed line: %s\n", line);
	nukesplitline(sp);
	free(line);
	return BADLINE;
}

/* write all rglines in bmp->rglines to bp in a serialised form. */
int
wtimingpoints(beatmap *bmp, Biobuf *bp)
{
	rgline *np;
	double vord;

	if (bmp->rglines == nil)
		return 0;

	Bprint(bp, "\r\n[TimingPoints]\r\n");
	for (np = bmp->rglines; np != nil; np = np->next) {
		vord = (np->type == GLINE) ? np->velocity : np->duration;
		Bprint(bp, "%ld,%.16g,%d,%d,%d,%d,%d,%d\r\n", np->t, vord, np->beats, np->sampset, np->sampindex, np->volume, np->type, np->kiai);
	}

	/* *two* cr-lfs follow [TimingPoints]. Why? Fuck you. */
	Bprint(bp, "\r\n");

	return 0;
}

/* read all entries from the [Colours] section, and add them tp
  * table *bmp->colours. change each colour entry's type to long, and
  * convert the r,g,b value string into a hex colour code.
  * returns readentries' exit code. */
static
int
rcolours(beatmap *bmp, Biobuf *bp)
{
	int i, nspec, exit;
	int r, g, b;
	long hex;
	entry *ep;
	splitline *sp;

	if (bmp->colours == nil)
		bmp->colours = mktable(4);

	nspec = sizeof(fcolours) / sizeof(typespec);
	exit = readentries(bmp->colours, bp, fcolours, nspec);
	if (exit < 0)
		return exit;

	for (i = 0; i < nspec; i++) {
		if ((ep = lookup(bmp->colours, fcolours[i].key)) == nil)
			continue;

		sp = csvsplit(ep->s, ",");
		r = sfatoi(sp->fields[RED]);
		g = sfatoi(sp->fields[GREEN]);
		b = sfatoi(sp->fields[BLUE]);
		hex = ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF) << 0);
		free(ep->s);
		ep->type = TLONG;
		ep->l = hex;
		nukesplitline(sp);
	}

	return 0;
}

/* convert the hex colour longs in table *bmp->colours' entries
  * colour longs back into r,g,b, and write them to bp.
  * returns writeentries' exit code. */
static
int
wcolours(beatmap *bmp, Biobuf *bp)
{
	int i, nspec;
	char *c;
	entry *ep;

	if (bmp->colours == nil)
		return 0;

	nspec = sizeof(fcolours) / sizeof(typespec);
	for (i = 0; i < nspec; i++) {
		if ((ep = lookup(bmp->colours, fcolours[i].key)) == nil)
			continue;

		c = smprint("%d,%d,%d", ep->l>>16, ep->l>>8 & 0xFF, ep->l & 0xFF);
		ep->type = TSTRING;
		ep->s = c;
	}

	Bprint(bp, "\r\n[Colours]\r\n");
	return writeentries(bmp->colours, bp, fcolours, nspec, " : ");
}

/* deserialise all [HitObjects] entries into rgline objects, and add
  * them to list rline *bmp->rglines.
  * returns 0 on success, or one of BADANCHOR, BADEDGESETS,
  * BADOBJECT, or BADSAMPLE on .osu syntax errors.
  * This routine sets errstr. */
static
int
robjects(beatmap *bmp, Biobuf *bp)
{
	hitobject *op;
	char *line;
	splitline *sp, *curvessp, *curvesp, *esampsp, *esetssp, *esetsp, *hsampsp;
	int normal, addition, index, volume;
	Rune *file;
	int t, x, y, typebits, type;
	char *hitsamp;
	int i;

	while ((line = nextentry(bp)) != nil) {
		sp = csvsplit(line, ",");

		t = atol(sp->fields[OBJTIME]);
		x = atoi(sp->fields[OBJX]);
		y = atoi(sp->fields[OBJY]);
		typebits = atoi(sp->fields[OBJTYPE]);
		type = typebits & TBTYPE;

		op = mkobj(type, t, x, y);

		switch (op->type) {
		case TCIRCLE:
			if (sp->nfield > OBJCIRCLEHITSAMP)
				hitsamp = sp->fields[OBJCIRCLEHITSAMP];

			break;
		case TSLIDER:
			op->slides = atoi(sp->fields[OBJSLIDES]);
			op->length = strtod(sp->fields[OBJLENGTH], nil);

			curvessp = csvsplit(sp->fields[OBJCURVES], "|");
			op->curve = curvessp->fields[0][0];
			for (i = 1; i < curvessp->nfield; i++) {
				curvesp = csvsplit(curvessp->fields[i], ":");
				if (curvesp->nfield != 2) {
					werrstr("bad anchor #%d for object t=%ld: '%s'", i, op->t, curvessp->fields[i]);
					nukesplitline(curvesp);
					nukesplitline(curvessp);
					nukeobj(op);
					return BADANCHOR;
				}

				op->anchors = addanchn(op->anchors, mkanch(atoi(curvesp->fields[X]), atoi(curvesp->fields[Y])), 0);
				nukesplitline(curvesp);
			}
			nukesplitline(curvessp);

			/* the OBJEDGESOUNDS and OBJEDGESETS fields are OPTIONAL, and will be
			  * MISSING if the slider has NO HITSOUNDS!!!
			  *
			  * I will pay $100 to the first person to slam a pie into peppy's face. */
			if (sp->nfield > OBJEDGESOUNDS) {
				op->nsladdition = op->slides + 1;
				op->sladditions = ecalloc(op->nsladdition, sizeof(int));
				esampsp = csvsplit(sp->fields[OBJEDGESOUNDS], "|");
				for (i = 0; i < op->nsladdition; i++)
					op->sladditions[i] = atoi(esampsp->fields[i]);
				nukesplitline(esampsp);
			}

			if (sp->nfield > OBJEDGESETS) {
				op->nsladdition = op->slides + 1;
				op->slnormalsets = ecalloc(op->nsladdition, sizeof(int));
				op->sladditionsets = ecalloc(op->nsladdition, sizeof(int));
				esetssp = csvsplit(sp->fields[OBJEDGESETS], "|");
				for (i = 0; i < op->nsladdition; i++) {
					esetsp = csvsplit(esetssp->fields[i], ":");
					if (esetsp->nfield != SLADDSET+1) {
						werrstr("bad slider edgesets for object t=%ld: '%s'", i, op->t, esetssp->fields[i]);
						nukesplitline(esetssp);
						nukesplitline(esetsp);
						nukeobj(op);
						return BADEDGESETS;
					}
	
					op->slnormalsets[i] = atoi(esetsp->fields[SLNORMSET]);
					op->sladditionsets[i] = atoi(esetsp->fields[SLADDSET]);
					nukesplitline(esetsp);
				}
				nukesplitline(esetssp);
			}

			if (sp->nfield > OBJSLIDERHITSAMP)
				hitsamp = sp->fields[OBJSLIDERHITSAMP];

			break;
		case TSPINNER:
			op->spinnerlength = atol(sp->fields[OBJENDTIME]) - t;

			if (sp->nfield > OBJSPINNERHITSAMP)
				hitsamp = sp->fields[OBJSPINNERHITSAMP];

			break;
		default:
			/* can't happen */
			werrstr("bad type for object t=%ld: '%b'", op->t, op->type);
			nukesplitline(sp);
			free(line);
			nukeobj(op);
			return BADOBJECT;
		}

		op->additions = atoi(sp->fields[OBJADDITIONS]);
		if (hitsamp != nil) {
			hsampsp = csvsplit(hitsamp, ":");
			if (hsampsp->nfield != HITSAMPFILE+1) {
				werrstr("malformed hitsample definition for object t=%ld: '%s'", op->t, hitsamp);
				nukesplitline(hsampsp);
				nukeobj(op);
				return BADSAMPLE;
			}

			normal = atoi(hsampsp->fields[HITSAMPNORMAL]);
			addition = atoi(hsampsp->fields[HITSAMPADDITIONS]);
			index = atoi(hsampsp->fields[HITSAMPINDEX]);
			volume = atoi(hsampsp->fields[HITSAMPVOLUME]);
			file = estrrunedup(hsampsp->fields[HITSAMPFILE]);

			op->samp = mkhitsample(normal, addition, index, volume, file);
			nukesplitline(hsampsp);
		}

		if (typebits & TBNEWCOMBO) {
			op->newcombo = 1;
			op->comboskip = (typebits & TBCOLOR) >> TBCOLORSHIFT;
		}

		bmp->objects = addobjt(bmp->objects, op);
		nukesplitline(sp);
		free(line);
		hitsamp = nil;
	}

	return 0;
}

/* write all objects in hitobject *bmp->objects to bp.
  * returns 0 on success. */
static
int
wobjects(beatmap *bmp, Biobuf *bp)
{
	int i;
	hitobject *op;
	anchor *ap;
	int typebits;

	Bprint(bp, "\r\n[HitObjects]\r\n");

	for (op = bmp->objects; op != nil; op = op->next) {
		typebits = op->type;
		if (op->newcombo == 1)
			typebits |= TBNEWCOMBO | (op->comboskip << TBCOLORSHIFT);

		Bprint(bp, "%d,%d,%ld,%d,%d", op->x, op->y, op->t, typebits, op->additions);

		switch(op->type) {
		case TCIRCLE:
			break;
		case TSLIDER:
			Bprint(bp, ",%c", op->curve);
			for (ap = op->anchors; ap != nil; ap = ap->next)
				Bprint(bp, "|%d:%d", ap->x, ap->y);

			Bprint(bp, ",%d,%.16g", op->slides, op->length);

			if (op->sladditions != nil) {
				Bprint(bp, ",%d", op->sladditions[0]);
				for (i = 1; i < op->nsladdition; i++)
					Bprint(bp, "|%d", op->sladditions[i]);
			}

			if (op->slnormalsets != nil && op->sladditionsets != nil) {
				Bprint(bp, ",%d:%d", op->slnormalsets[0], op->sladditionsets[0]);
				for (i = 1; i < op->nsladdition; i++)
					Bprint(bp, "|%d:%d", op->slnormalsets[i], op->sladditionsets[i]);
			}

			break;
		case TSPINNER:
			Bprint(bp, ",%ld", op->t + op->spinnerlength);

			break;
		}

		if (op->samp != nil) {
			Bprint(bp, ",%d:%d", op->samp->normal, op->samp->addition);
			Bprint(bp, ":%d:%d", op->samp->index, op->samp->volume);
			Bprint(bp, ":%S", op->samp->file);
		}

		Bprint(bp, "\r\n");
	}

	return 0;
}

/* create a new beatmap object
  * returns nil when out of memory. */
beatmap *
mkbeatmap()
{
	beatmap *new = ecalloc(1, sizeof(beatmap));

	return new;
}

/* free a beatmap object, including all hitobjects in bmp->objects,
  * and rglines in bmp->rglines. */
void
nukebeatmap(beatmap *bmp)
{
	rgline *np, *next;
	hitobject *op, *onext;

	nuketable(bmp->general);
	nuketable(bmp->editor);
	nuketable(bmp->metadata);
	nuketable(bmp->difficulty);
	nuketable(bmp->colours);

	free(bmp->bookmarks);
	free(bmp->events);

	for (np = bmp->rglines; np != nil; np = next) {
		next = np->next;
		nukergline(np);
	}

	for (op = bmp->objects; op != nil; op = onext) {
		onext = op->next;
		nukeobj(op);
	}

	free(bmp);
}

/* exhaust all sections in bp. for each section, call the corresponding
  * .read handler in handlers[] with bmp and bp.
  * returns any of the handlers' exit codes if they are nonzero, or zero
  * on success */ 
int
readmap(beatmap *bmp, Biobuf *bp)
{
	char *s;
	int i;
	int exit;
	int nhandler = sizeof(handlers) / sizeof(handler);

	bmp->version = nextline(bp);

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

/* call each .write handler in handlers[] with bmp and bp.
  * returns any of the handlers' exit codes if they are nonzero, or zero
  * on success */
int
writemap(beatmap *bmp, Biobuf *bp)
{
	int i;
	int exit;
	int nhandler = sizeof(handlers) / sizeof(handler);

	Bprint(bp, "%s\r\n", bmp->version);

	for (i = 0; i < nhandler; i++) {
		if ((exit = handlers[i].write(bmp, bp)) < 0)
			return exit;
	}

	return 0;
}
