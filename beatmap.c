#include <u.h>
#include <libc.h>
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
static int hsgeneral(beatmap *bmp, int fd);
static int hseditor(beatmap *bmp, int fd);
static int hsmetadata(beatmap *bmp, int fd);
static int hsdifficulty(beatmap *bmp, int fd);
static int hsevents(beatmap *bmp, int fd);
static int hstimingpoints(beatmap *bmp, int fd);
static int hscolours(beatmap *bmp, int fd);
static int hsobjects(beatmap *bmp, int fd);

typedef struct handler {
	char *section;				/* section name, e.g. "[General]" */
	int (*func)(beatmap *, int);	/* handler for this section, e.g. hsgeneral */
} handler;

static handler handlers[] = {
	{.section = "[General]", .func = hsgeneral},
	{.section = "[Editor]", .func = hseditor},
	{.section = "[Metadata]", .func = hsmetadata},
	{.section = "[Difficulty]", .func = hsdifficulty},
	{.section = "[Events]", .func = hsevents},
	{.section = "[TimingPoints]", .func = hstimingpoints},
	{.section = "[Colours]", .func = hscolours},
	{.section = "[HitObjects]", .func = hsobjects},
};

typedef struct entry {
	char *key;		/* configuration directive key */
	char *value;	/* configuration directive value */
} entry;

/* key-value pairs */
static entry entries[] = {
	/* [General] */
	{.key = "AudioFilename", .value = nil},
	{.key = "AudioLeadIn", .value = nil},
	{.key = "PreviewTime", .value = nil},
	{.key = "Countdown", .value = nil},
	{.key = "SampleSet", .value = nil},
	{.key = "StackLeniency", .value = nil},
	{.key = "Mode", .value = nil},
	{.key = "LetterboxInBreaks", .value = nil},
	{.key = "WidescreenStoryboard", .value = nil},

	/* [Editor] */
	{.key = "Bookmarks", .value = nil},
	{.key = "DistanceSpacing", .value = nil},
	{.key = "BeatDivisor", .value = nil},
	{.key = "GridSize", .value = nil},
	{.key = "TimelineZoom", .value = nil},

	/* [Metadata] */
	{.key = "Title", .value = nil},
	{.key = "TitleUnicode", .value = nil},
	{.key = "Artist", .value = nil},
	{.key = "ArtistUnicode", .value = nil},
	{.key = "Creator", .value = nil},
	{.key = "Version", .value = nil},
	{.key = "Source", .value = nil},
	{.key = "Tags", .value = nil},
	{.key = "BeatmapID", .value = nil},
	{.key = "BeatmapSetID", .value = nil},

	/* [Difficulty] */
	{.key = "HPDrainRate", .value = nil},
	{.key = "CircleSize", .value = nil},
	{.key = "OverallDifficulty", .value = nil},
	{.key = "ApproachRate", .value = nil},
	{.key = "SliderMultiplier", .value = nil},
	{.key = "SliderTickRate", .value = nil},

	/* [Colours] */
	{.key = "Combo1", .value = nil},
	{.key = "Combo2", .value = nil},
	{.key = "Combo3", .value = nil},
	{.key = "Combo4", .value = nil},
	{.key = "Combo5", .value = nil},
	{.key = "Combo6", .value = nil},
	{.key = "Combo7", .value = nil},
	{.key = "Combo8", .value = nil},
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
char *samplesets[] = {
	"Default",		/* unused */
	"Normal",
	"Soft",
	"Drum"
};

static int maxchar = 256;	/* size of line[] */
static char *line = nil;	/* line read by getline() */
static char *sline = nil;	/* line copy used by csvsplit() and keyval() */

/* relevant for: [Events], [TimingPoints], and [HitObjects] (CSV-like) */
static int maxfield = 9;	/* size of fields[] */
static int nfield = 0;		/* number of fields processed by csvsplit() */
static char **fields = nil;	/* field pointers */


/* free all getline() & csvsplit()-related static fields
  * & reset their associated variables. it only makes sense
  * to call this function before reading a new line. */
static
void
reset()
{
	free(line);
	free(fields);

	maxchar = maxfield = nfield = 0;
	line = nil;
	fields = nil;
}

/* copy the contents of s into line[] and return a pointer to line[].
  * return nil if s is nil, or when out of memory */
static
char *
setline(char *s)
{
	if (s == nil)
		return nil;

	reset();
	line = strdup(s);

	if (line == nil)
		return nil;

	return line;
}

/* read a line from fd into line[]. returns a pointer to line[], or 'nil'
  * on EOF or when out of memory.
  *
  * line[] is freed on subsequent calls; the caller is responsible
  * for copying the data if so desired. */
static
char *
getline(int fd)
{
	int nchar;
	int nread;
	char c;

	reset();
	maxchar = 256;
	nchar = 0;
	line = ecalloc(maxchar, sizeof(char));

	while ((nread = read(fd, &c, 1)) != 0) {
		if (c == '\n')
			break;
		else if (c == '\r')
			continue;

		if (nchar+1 > maxchar) {
			maxchar *= 2;
			line = erealloc(line, sizeof(char) * maxchar);
		}

		line[nchar++] = c;
	};

	if (nread == 0 && nchar == 0)
		return nil;

	line[nchar] = '\0';
	return line;
}

/* call getline() until the next section header is read.
  * returns a pointer to line[], or nil on EOF. */
static
char *
nextsection(int fd)
{
	char *l;

	while ((l = getline(fd)) != nil) {
		if (l[0] == '[' && l[strlen(line)-1] == ']' )
			return l;
	}

	/* end of file */
	return nil;
}

/* call getline(), and return a pointer to line[], or 'nil' if line[] is empty. */
static
char *
nextentry(int fd)
{
	char *line = getline(fd);

	if (line == nil || line[0] == '\0')
		return nil;
	else
		return line;
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

/* split line[] into distinct fields depending delimited by sepchar
  * Returns the amount of split fields, or -1 if memory has run out.
  *
  * fields are freed on subsequent calls. The caller is responsible
  * for making a copy of the fields that they wish to use.
  */
static
int
csvsplit(char *sepchar)
{
	char *sepp = nil;
	char sepc;
	char *p = nil;

	if (line[0] == '\0' || sepchar == nil)
		return 0;

	p = sline = strdup(line);

	nfield = 0;
	maxfield = 9;

	fields = ecalloc(maxfield, sizeof(char *));

	do {
		if (nfield+1 > maxfield) {
			maxfield *= 2;
			fields = erealloc(fields, sizeof(char *) * maxfield);
		}

		if (p[0] == '"')
			sepp = advquoted(++p, sepchar);
		else
			sepp = p + strcspn(p, sepchar);

		sepc = sepp[0];
		sepp[0] = '\0';
		fields[nfield++] = p;
		p = sepp + 1;
	} while (sepc == sepchar[0]);

	return nfield;
}

/* return a pointer to the n-th field in fields[], starting from 0.
  * n may be negative ('-1' = last field) */
static
char *
csvgetfield(int n)
{
	if (n >= nfield || abs(n) > nfield)
		return nil;
	else if (n < 0)
		return fields[nfield + n];
		
	return fields[n];
}

/* return the entry in entries[] whose key matches q.
  * return nil on no matches.
  */
static
entry *
getentry(char *q)
{
	int n;
	int nentry = sizeof(entries) / sizeof(entry);

	if (q == nil)
		return nil;

	for (n = 0; n < nentry; n++) {
		if (strcmp(q, entries[n].key) == 0)
			return &entries[n];
	}

	return nil;
}

/* free all .value strings in entries[] and assign nil to each one. */
static
void
resetentries()
{
	int nentry = sizeof(entries) / sizeof(entry);
	int n;

	for (n = 0; n < nentry; n++) {
		free(entries[n].value);
		entries[n].value = nil;
	}
}

/* split line into key and value, and copy the value to the entries[]
  * entry with a matching key. returns the relevant entry, or nil if no
  * entry with the given key exists. this routine sets errstr.
  */
static
entry *
kvsplit()
{
	entry *ep = nil;
	char *valuep = nil;
	char *keyp = nil;

	if (line == nil || line[0] == '\0')
		return nil;

	keyp = strdup(line);

	valuep = keyp + strcspn(keyp, ":");
	valuep[0] = '\0';
	valuep++;
	valuep += strspn(valuep, " ");

	keyp[strcspn(keyp, " ")] = '\0';

	ep = getentry(keyp);

	if (ep == nil) {
		werrstr("Unrecognised key '%s'", keyp);
		return nil;
	}

	ep->value = strdup(valuep);
	free(keyp);

	return ep;
}

/* deserialises all [General] entries into the appropriate beatmap struct fields.
  * returns 0 on success, BADKEY on an illegal configuration key, or NOMEM
  * when out of memory. */
static
int
hsgeneral(beatmap *bmp, int fd)
{
	while (nextentry(fd) != nil) {
		if (kvsplit() == nil)
			return BADKEY;
	}

	bmp->audiof = strrunedup(getentry("AudioFilename")->value);

	bmp->leadin = (ulong) atol(getentry("AudioLeadIn")->value);
	bmp->previewt = (ulong) atol(getentry("PreviewTime")->value);
	bmp->countdown = atoi(getentry("Countdown")->value);
	bmp->stackleniency = atoi(getentry("StackLeniency")->value);
	bmp->mode = atoi(getentry("Mode")->value);
	bmp->letterbox = atoi(getentry("LetterboxInBreaks")->value);
	bmp->widescreensb = atoi(getentry("WidescreenStoryboard")->value);
	/* p9p's atof(3) routines don't handle the error string properly, and
	  * some of these values may genuinely be zero. Take a leap of
	  * faith here... */

	return 0;
}

static
int
hseditor(beatmap *bmp, int fd)
{
	int n;
	int i;

	while (nextentry(fd) != nil) {
		if (kvsplit() == nil)
			return BADKEY;
	}

	setline(getentry("Bookmarks")->value);
	if ((n = csvsplit(",")) != 0) {
		bmp->bookmarks = ecalloc(n, sizeof(ulong));

		for (i = 0; i < n; i++)
			bmp->bookmarks[i] = (ulong) atol(csvgetfield(i));

		bmp->nbookmarks = n;
	}

	bmp->distancesnap = atof(getentry("DistanceSpacing")->value);
	bmp->beatdivisor = atof(getentry("BeatDivisor")->value);
	bmp->gridsize = atoi(getentry("GridSize")->value);
	bmp->timelinezoom = atof(getentry("TimelineZoom")->value);

	return 0;
}

/* deserialises all [Metadata] entries into the appropriate beatmap struct fields.
  * returns 0 on success, BADKEY on an illegal configuration key, or NOMEM
  * when out of memory. */
static
int
hsmetadata(beatmap *bmp, int fd)
{
	while (nextentry(fd) != nil) {
		if (kvsplit() == nil)
			return BADKEY;
	}

	if ((bmp->title = strdup(getentry("Title")->value)) == nil) return NOMEM;
	if ((bmp->utf8title = strrunedup(getentry("TitleUnicode")->value)) == nil) return NOMEM;
	if ((bmp->artist = strdup(getentry("Artist")->value)) == nil) return NOMEM;
	if ((bmp->utf8artist = strrunedup(getentry("ArtistUnicode")->value)) == nil) return NOMEM;
	if ((bmp->author = strdup(getentry("Creator")->value)) == nil) return NOMEM;
	if ((bmp->diffname = strdup(getentry("Version")->value)) == nil) return NOMEM;
	if ((bmp->source = strdup(getentry("Source")->value)) == nil) return NOMEM;
	if ((bmp->tags = strdup(getentry("Tags")->value)) == nil) return NOMEM;
	bmp->id = atoi(getentry("BeatmapID")->value);
	bmp->setid = atoi(getentry("BeatmapSetID")->value);

	return 0;
}

/* deserialises all [Difficulty] entries into the appropriate beatmap struct fields.
  * returns 0 on success, BADKEY on an illegal configuration key, or NOMEM
  * when out of memory. */
static
int
hsdifficulty(beatmap *bmp, int fd)
{
	while (nextentry(fd) != nil) {
		if (kvsplit() == nil)
			return BADKEY;
	}

	bmp->hp = (float) atof(getentry("HPDrainRate")->value);
	bmp->cs = (float) atof(getentry("CircleSize")->value);
	bmp->od = (float) atof(getentry("OverallDifficulty")->value);
	bmp->ar = (float) atof(getentry("ApproachRate")->value);
	bmp->slmultiplier = (float) atof(getentry("SliderMultiplier")->value);
	bmp->sltickrate = atoi(getentry("SliderTickRate")->value);

	return 0;
}

/* the author doesn't care about storyboarding. As such, hsevents copies the
  * raw [Events] contents into a character string, with carriage returns restored.
  *
  * returns 0 on success, or NOMEM when out of memory. */
static
int
hsevents(beatmap *bmp, int fd)
{
	int nchar;
	int maxchar;
	int len;
	char *linep;

	nchar = 0;
	maxchar = 256;
	bmp->events = ecalloc(maxchar, sizeof(char));

	while ((linep = nextentry(fd)) != nil) {
		len = strlen(linep);

		/* + 2 for carriage return and newline */
		if (nchar + len + 2 > maxchar) {
			do {
				maxchar *= 2;
			} while (nchar + len + 2 > maxchar);

			bmp->events = erealloc(bmp->events, sizeof(char) * maxchar);
		}

		strcat(bmp->events, linep);
		strcat(bmp->events, "\r\n");

		nchar += len + 2;
	}

	return 0;
}

/* deserialises all [TimingPoints] entries into rline and gline objects, and adds
  * them to bmp's list.
  * returns 0 on success, BADLINE on illegal line type, or NOMEM
  * when out of memory. This routine sets the errstr. */
static
int
hstimingpoints(beatmap *bmp, int fd)
{
	int isredline;
	int t, beats, volume, effects, kiai;
	int sampset, sampindex;
	double velocity;
	ulong duration;
	gline *glp;
	rline *rlp;

	while (nextentry(fd) != nil) {
		csvsplit(",");

		t = atol(csvgetfield(LNTIME));

		volume = atoi(csvgetfield(LNVOLUME));
		sampset = atoi(csvgetfield(LNSAMPSET));
		sampindex = atoi(csvgetfield(LNSAMPINDEX));

		effects = atoi(csvgetfield(LNEFFECTS));
		kiai = effects & EBKIAI;

		isredline = atoi(csvgetfield(LNREDLINE));

		switch (isredline) {
		case 1:
			duration = atol(csvgetfield(LNDURATION));
			beats = atol(csvgetfield(LNBEATS));

			rlp = mkrline(t, duration, beats);

			rlp->volume = volume;
			rlp->sampset = sampset;
			rlp->sampindex = sampindex;

			rlp->kiai = kiai;

			bmp->rlines = addrlinet(bmp->rlines, rlp);

			break;
		case 0:
			velocity = strtod(csvgetfield(LNVELOCITY), nil);

			glp = mkgline(t, velocity);

			glp->volume = volume;
			glp->sampset = sampset;
			glp->sampindex = sampindex;

			glp->kiai = kiai;

			bmp->glines = addglinet(bmp->glines, glp);

			break;

		default:
			/* can't happen */
			werrstr("unknown line type t=%d type=%d", t, isredline);
			return BADLINE;
		}
	}

	return 0;
}

/* converts red, green and blue values to a hexadecimal colour code */
static
long
rgbtohex(int r, int g, int b)
{
	return ((r & 0xFF) << 16) | ((g & 0xFF) << 8) | ((b & 0xFF) << 0);
}

/* deserialises all [Colours] entries into hexadecimal colour codes, and
  * adds them to bmp->colours.
  * returns 0 on success, or NOMEM when out of memory. */
static
int
hscolours(beatmap *bmp, int fd)
{
	int r, g, b;
	int i;
	int n = 0;
	char *key;
	entry *ep;

	while (nextentry(fd) != nil) {
		if (kvsplit() == nil)
			return BADKEY;
		n++;
	}

	bmp->colours = ecalloc(n, sizeof(long));
	bmp->ncolours = n;

	key = ecalloc(sizeof("Combo#"), sizeof(char));

	for (i = 0; i < n; i++) {
		sprint(key, "Combo%d", i + 1);
		ep = getentry(key);

		setline(ep->value);
		csvsplit(",");

		r = atoi(csvgetfield(RED));
		g = atoi(csvgetfield(GREEN));
		b = atoi(csvgetfield(BLUE));

		bmp->colours[i] = rgbtohex(r, g, b);
	}

	free(key);

	return 0;
}

/* deserialises all [HitObjects] entries into hitobject objects, and adds
  * them to bmp's list.
  * returns 0 on success, or BADLINE on illegal line type.
  * This routine sets the errstr. */
static
int
hsobjects(beatmap *bmp, int fd)
{
	hitobject *op;

	int t, x, y, typebits, type;

	char *curves, *curvetype;
	char *xs, *ys;

	int additions;
	char *hitsample;
	char *edgesounds, *edgesets;
	char *nsp, *asp;

	int n, i;

	while (nextentry(fd) != nil) {
		csvsplit(",");

		t = (ulong) atol(csvgetfield(OBJTIME));
		x = atoi(csvgetfield(OBJX));
		y = atoi(csvgetfield(OBJY));
		typebits = atoi(csvgetfield(OBJTYPE));
		type = typebits & TBTYPE;

		additions = atoi(csvgetfield(OBJADDITIONS));
		hitsample = csvgetfield(OBJHITSAMP);

		op = mkobj(type, t, x, y);

		switch (type) {
		case TCIRCLE:
			break;
		case TSLIDER:
			curves = csvgetfield(OBJCURVES);
			edgesounds = csvgetfield(OBJEDGESOUNDS);
			edgesets = csvgetfield(OBJEDGESETS);

			op->slides = atoi(csvgetfield(OBJSLIDES));
			op->length = strtod(csvgetfield(OBJLENGTH), nil);

			setline(curves);
			n = csvsplit("|");
			curvetype = csvgetfield(OBJCURVETYPE);
			op->curve = curvetype[0];
			for (i = 1; i < n; i++) {
				xs = csvgetfield(i);
				ys = xs + strcspn(xs, ":");
				ys[0] = '\0';
				ys++;
				op->alistp = addanchn(op->alistp, mkanch(atoi(xs), atoi(ys)), 0);
			}

			op->nsladdition = op->slides + 1;
			op->sladditions = ecalloc(op->nsladdition, sizeof(int));
			setline(edgesounds);
			csvsplit("|");
			for (i = 0; i < op->nsladdition; i++)
				op->sladditions[i] = atoi(csvgetfield(i));

			op->slnormalsets = ecalloc(op->nsladdition, sizeof(int));
			op->sladditionsets = ecalloc(op->nsladdition, sizeof(int));
			setline(edgesets);
			csvsplit("|");
			for (i = 0; i < op->nsladdition; i++) {
				nsp = csvgetfield(i);
				asp = nsp + strcspn(nsp, ":");
				asp[0] = '\0';
				asp++;

				op->slnormalsets[i] = atoi(nsp);
				op->sladditionsets[i] = atoi(asp);
			}

			break;
		case TSPINNER:
			op->spinnerlength = (ulong) atol(csvgetfield(OBJENDTIME)) - t;

			break;
		default:
			/* can't happen */
			werrstr("object with no type t=%d x=%d y=%d type=%b", t, x, y, type);
			return BADOBJECT;
		}

		op->additions = additions;
		setline(hitsample);
		csvsplit(":");
		op->normalset = atoi(csvgetfield(HITSAMPNORMAL));
		op->additionset = atoi(csvgetfield(HITSAMPADDITIONS));
		op->sampindex = atoi(csvgetfield(HITSAMPINDEX));
		op->volume = atoi(csvgetfield(HITSAMPVOLUME));
		op->filename = strrunedup(csvgetfield(HITSAMPFILE));

		if (typebits & TBNEWCOMBO) {
			op->newcombo = 1;
			op->comboskip = (typebits & TBCOLOR) >> TBCOLORSHIFT;
		}

		bmp->objects = addobjt(bmp->objects, op);
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

/* loads .osu file data from fd, and calls the appropriate hs*() 
  * handler for each section, which in turn fill bmp with the appropriate
  * data.
  * 
  * this routine returns 0 on success, NOMEM when out of memory, or
  * the exit code of a handler function if it is less than 0. Some handlers
  * set the errstr.
  */
int
loadmap(beatmap *bmp, int fd)
{
	char *s;
	int i;
	int exit;
	int nhandler = sizeof(handlers) / sizeof(handler);

	resetentries();

	while ((s = nextsection(fd)) != nil) {
		for (i = 0; i < nhandler; i++) {
			if (strcmp(s, handlers[i].section) == 0)
				if ((exit = handlers[i].func(bmp, fd)) < 0)
					return exit;
		}
	}

	return 0;
}

