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

typedef struct kvdef {
	char *key;		/* string key of entry */
	char *fmt;		/* format string for output */
	int type;		/* the data type of the associated value. see hash.h:2,9 */
} kvdef;

static kvdef kvgeneral[] = {
	/* [General] */
	{.key = "AudioFilename", .fmt = "%s: %S", .type = TRUNE},
	{.key = "AudioLeadIn", .fmt = "%s: %ld", .type = TLONG},
	{.key = "PreviewTime", .fmt = "%s: %ld", .type = TLONG},
	{.key = "Countdown", .fmt = "%s: %d", .type = TINT},
	{.key = "CountdownOffset", .fmt = "%s: %d", .type = TINT},
	{.key = "SampleSet", .fmt = "%s: %s", .type = TSTRING},
	{.key = "StackLeniency", .fmt = "%s: %G", .type = TFLOAT},
	{.key = "Mode", .fmt = "%s: %d", .type = TINT},
	{.key = "LetterboxInBreaks", .fmt = "%s: %d", .type = TINT},
	{.key = "EditorBookmarks", .fmt = "%s: %s", .type = TSTRING},			/* deprecated; replaced by :/"Bookmarks"/ */
	{.key = "EditorDistanceSpacing", .fmt = "%s: %.16G", .type = TDOUBLE},	/* deprecated; replaced by :/"DistanceSpacing"/ */
	{.key = "StoryFireInFront", .fmt = "%s: %d", .type = TINT},
	{.key = "UseSkinSprites", .fmt = "%s: %d", .type = TINT},
	{.key = "OverlayPosition", .fmt = "%s: %s", .type = TSTRING},
	{.key = "SkinPreference", .fmt = "%s:%s", .type = TSTRING},
	{.key = "EpilepsyWarning", .fmt = "%s: %d", .type = TINT},
	{.key = "SpecialStyle", .fmt = "%s: %d", .type = TINT},
	{.key = "WidescreenStoryboard", .fmt = "%s: %d", .type = TINT},
	{.key = "SamplesMatchPlaybackRate", .fmt = "%s: %d", .type = TINT},
};

static kvdef kveditor[] = {
	/* [Editor] */
	{.key = "Bookmarks", .fmt = "%s: %s", .type = TSTRING},
	{.key = "DistanceSpacing", .fmt = "%s: %.16G", .type = TDOUBLE},
	{.key = "BeatDivisor", .fmt = "%s: %G", .type = TFLOAT},
	{.key = "GridSize", .fmt = "%s: %d", .type = TINT},
	{.key = "TimelineZoom", .fmt = "%s: %.7G", .type = TFLOAT},
	{.key = "CurrentTime", .fmt = "%s: %.16G", .type = TDOUBLE},
};

static kvdef kvmetadata[] = {
	/* [Metadata] */
	{.key = "Title", .fmt = "%s:%s", .type = TSTRING},
	{.key = "TitleUnicode", .fmt = "%s:%S", .type = TRUNE},
	{.key = "Artist", .fmt = "%s:%s", .type = TSTRING},
	{.key = "ArtistUnicode", .fmt = "%s:%S", .type = TRUNE},
	{.key = "Creator", .fmt = "%s:%s", .type = TSTRING},
	{.key = "Version", .fmt = "%s:%s", .type = TSTRING},
	{.key = "Source", .fmt = "%s:%s", .type = TSTRING},
	{.key = "Tags", .fmt = "%s:%s", .type = TSTRING},
	{.key = "BeatmapID", .fmt = "%s:%d", .type = TINT},
	{.key = "BeatmapSetID", .fmt = "%s:%d", .type = TINT},
};

static kvdef kvdifficulty[] = {
	/* [Difficulty] */
	{.key = "HPDrainRate", .fmt = "%s:%.3G", .type = TFLOAT},
	{.key = "CircleSize", .fmt = "%s:%.3G", .type = TFLOAT},
	{.key = "OverallDifficulty", .fmt = "%s:%.3G", .type = TFLOAT},
	{.key = "ApproachRate", .fmt = "%s:%.3G", .type = TFLOAT},
	{.key = "SliderMultiplier", .fmt = "%s:%.15G", .type = TDOUBLE},
	{.key = "SliderTickRate", .fmt = "%s:%.16G", .type = TDOUBLE},
};

static kvdef kvcolours[] = {
	/* [Colours] */
	{.key = "Combo1", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo2", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo3", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo4", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo5", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo6", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo7", .fmt = "%s : %s", .type = TSTRING},
	{.key = "Combo8", .fmt = "%s : %s", .type = TSTRING},
	{.key = "SliderBody", .fmt = "%s : %s", .type = TSTRING},
	{.key = "SliderTrackOverride", .fmt = "%s : %s", .type = TSTRING},
	{.key = "SliderBorder", .fmt = "%s : %s", .type = TSTRING},
	{.key = "SliderBorderColor", .fmt = "%s : %s", .type = TSTRING},
	{.key = "StarBreakAdditive", .fmt = "%s : %s", .type = TSTRING},
	{.key = "SpinnerApproachCircle", .fmt = "%s : %s", .type = TSTRING},
};

static int nkvgeneral = sizeof(kvgeneral) / sizeof(kvdef);
static int nkveditor = sizeof(kveditor) / sizeof(kvdef);
static int nkvmetadata = sizeof(kvmetadata) / sizeof(kvdef);
static int nkvdifficulty = sizeof(kvdifficulty) / sizeof(kvdef);
static int nkvcolours =  sizeof(kvcolours) / sizeof(kvdef);

/* Field labels for key:value pairs */
enum {
	KEY=0,
	VALUE,
};
int maxkvfields = VALUE + 1;

/* CSV field labels for hitobject entries 
  * See: https://osu.ppy.sh/wiki/en/osu%21_File_Formats/Osu_%28file_format%29#hit-objects
  */
enum {
	/* ! = optional field */
	OBJX=0,				/* x position of object */
	OBJY,				/* y position of object */
	OBJTIME,				/* timestamp in milliseconds */
	OBJTYPE,				/* 8-bit integer describing object type & colours; see enum typebits */
	OBJADDITIONS,		/* hitsound additions for object */
	OBJENDTIME=5,		/* spinner end timestamp */
	OBJCURVES=5,			/* slider curve data */
	OBJCIRCLEHITSAMP=5,	/* ! custom hitsound sampleset (circles) */
	OBJSPINNERHITSAMP=6,	/* ! custom hitsound sampleset (spinners) */
	OBJSLIDES=6,			/* amount of reverses + 1 */
	OBJLENGTH,			/* ""visual length in osu! pixels"" */
	OBJEDGESOUNDS,		/* ! slider head/tail hitsounds */
	OBJEDGESETS,			/* ! slider head/tail sample sets */
	OBJSLIDERHITSAMP		/* ! custom hitsound sampleset (sliders) */
};
int maxobjfields = OBJSLIDERHITSAMP + 1;

/* C(olon)SV and P(ipe)SV fields for OBJCURVES and OBJSAMP */
enum {
	OBJCURVETYPE=0,
	HITSAMPNORMAL=0,
	HITSAMPADDITIONS,
	HITSAMPINDEX,
	HITSAMPVOLUME,
	HITSAMPFILE,
};
int maxhitsampfields = HITSAMPFILE + 1;

/* bitmasks for the OBJTYPE field; apply with & */
enum {
	TBTYPE = 0xB,			/* circle, spinner, or slider type */
	TBNEWCOMBO = 0x4,	/* new combo */
	TBCOLOR = 0x70,		/* amount of colours to skip */
	TBCOLORSHIFT = 4,		/* shift right amount after having applied & TCOLOR  */
	TBHOLD = 0x80, 		/* nobody cares about osu!mania */
};

/* CSV fields for red/greenline entries 
  * see: https://osu.ppy.sh/wiki/en/osu%21_File_Formats/Osu_%28file_format%29#timing-points */
enum {
	/* ! = optional field */
	LNTIME=0,		/* timestamp in miliseconds */
	LNVORD=1,		/* duration of beat in miliseconds (redline), or negative inverse slider velocity multiplier (greenline) */
	LNBEATS,			/* number of beats in measure */
	LNSAMPSET,		/* default hitobject sampleset */
	LNSAMPINDEX,		/* custom sample index */
	LNVOLUME,		/* volume percentage */
	LNTYPE,			/* ! one of rgbline.h:0/linetype/ */
	LNEFFECTS,		/* ! extra effects */
};
int maxrglinefields = LNEFFECTS + 1;

/* bitmasks for the rgline.effects field; apply with & */
enum {
	EBKIAI = 0x1,		/* kiai time enabled */
	EBOMIT = 0x8,		/* omit first barline */
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

/* return 0 if line contains any characters besides spaces, tabs or carriage returns */
static
int
isempty(char *ln)
{
	char *p;

	if (ln == nil)
		return -1;

	for (p = ln; *p != '\0'; p++)
		if (*p != '\r' && *p != '\t' && *p != ' ')
			return 0;

	return 1;
}

/* return 1 if line contains a section header, 0 otherwise */
static
int
isheader(char *ln)
{
	if (ln == nil)
		return -1;

	if (ln[0] == '[' && ln[strlen(ln)-1] == ']')
		return 1;
	else
		return 0;
}


/* read a line from bp, and strip out the trailing carriage return. */
static
char *
nextline(Biobuf *bp)
{
	char *ln;

	if ((ln = Brdstr(bp, '\n', 1)) != nil) {
		if (ln[strlen(ln) - 1] == '\r')
			ln[strlen(ln) - 1] = '\0';
		return ln;
	}

	return nil;
}

/* read lines from bp up until the next section header (e.g. "[General]"), and return the header.
  * returns nil on end-of-file. */
static
char *
nextsection(Biobuf *bp)
{
	char *ln;

	while ((ln = nextline(bp)) != nil) {
		if (isheader(ln) == 1)
			return ln;
		free(ln);
	}

	/* end-of-file */
	return nil;
}

/* read the next configuration directive from bp, and
  * return it, skipping empty lines.
  * returns nil at the next section header, or end-of-file. */
static
char *
nextdirective(Biobuf *bp)
{
	char *ln;

	while ((ln = nextline(bp)) != nil) {
		if (isheader(ln) == 1) {
			Bseek(bp, -(Blinelen(bp)+1), 1);
			return nil;
		} else if (isempty(ln) == 0) {
			return ln;
		}

		free(ln);
	}

	/* end-of-file */
	return nil;
}

/* read the raw contents of the current section into a malloc'd string
  * returns a pointer to the string on success, nil on failure */
static
char *
readsection(Biobuf *bp)
{
	int nchar;
	int maxchar;
	int len;
	char *ln, *section;

	nchar = 0;
	maxchar = 256;
	section = ecalloc(maxchar, sizeof(char));

	while ((ln = nextdirective(bp)) != nil) {
		len = strlen(ln);

		/* + 2 for carriage return and newline */
		if (nchar + len + 2 > maxchar) {
			do {
				maxchar *= 2;
			} while (nchar + len + 2 > maxchar);

			section = erealloc(section, sizeof(char) * maxchar);
		}

		strcat(section, ln);
		strcat(section, "\r\n");

		nchar += len + 2;
		free(ln);
	}

	return section;
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

/* count the amount of CSV fields in ln
  * returns the amount of fields on success, or -1 on failure */
static
int
csvcountf(char *ln, char *sep)
{
	char *p;
	int n;

	if (ln == nil || sep == nil)
		return -1;

	n = 0;
	for (p = ln; *p != '\0'; p++) {
		if (*p == *sep)
			n++;
		else if (*p == '"')
			p = advquoted(++p, sep);
	}

	return n+1;
}

/* replace each sep in line[] with a null character, and store pointers to the
  * start of each field in fields[]. returns the amount of split fields. */
static
int
csvsplit(char *ln, char **fields, int nfields, char *sep)
{
	char *sepp;
	char sepc;
	char *p;
	int i;

	if (ln == nil || fields == nil || nfields < 0 || sep == nil)
		return -1;

	sepp = nil;
	p = ln;
	i = 0;

	if (*p == '\0')
		return 0;

	do {
		if (*p == '"')
			sepp = advquoted(++p, sep);
		else
			sepp = p + strcspn(p, sep);

		sepc = *sepp;
		*sepp = '\0';
		fields[i++] = p;

		sepp++;
		p = sepp + strspn(sepp, sep);
	} while (sepc == sep[0] && i < nfields);

	return i;
}

/* split line[] into two distinct fields, delimited by the
  * first instance of any characters in sep. If wstrip is larger than 0, then
  * kvsplit strips whitespace around the delimiter.
  * returns the number of split fields (realistically, always '2') */
static
int
kvsplit(char *ln, char **fields, int nfields, char *sep, int wstrip)
{
	char *k, *v;

	if (ln == nil || ln[0] == '\0' || sep == nil)
		return -1;

	k = ln;
	v = k + strcspn(k, sep);
	v[0] = '\0';
	v++;

	if (wstrip > 0) {
		v += strspn(v, " 	");
		k[strcspn(k, " 	")] = '\0';
	}

	fields[KEY] = k;
	fields[VALUE] = v;

	return 2;
}

/* search through an nkvlist-sized kvlist[], and return a pointer
  * to the first kvdef in kvlist[] whose key value matches k.
  * returns nil if no matches were found.
  * lookupkvdef searches case insensitively */
static
kvdef *
lookupkvdef(kvdef *kvlist, int nkvlist, char *k)
{
	int i;

	if (k == nil || nkvlist <= 0)
		return nil;

	for (i = 0; i < nkvlist; i++)
		if (cistrcmp(kvlist[i].key, k) == 0)
			return &kvlist[i];

	return nil;
}

/* create a new entry object from s with the respective .type enum
  * from kvlist. If the key does not appear in kvlist, then the entry's
  * type defaults to TSTRING.
  * if wstrip is non-zero, strip whitespace around the delimiter
  * returns 0 on success, -1 on failure.
  * this routine sets errstr 
  * sample input: 'AudioFilename: audio.mp3' */
static
int
strtoentry(char *s, entry **epp, kvdef *kvlist, int nkvlist, int wstrip)
{
	char **fields;
	entry *ep;
	kvdef *kvdefp;
	int type;

	if (s == nil || epp == nil || kvlist == nil || nkvlist <= 0 || wstrip < 0)
		return -1;

	fields = ecalloc(maxkvfields, sizeof(char *));
	kvsplit(s, fields, maxkvfields, ":", wstrip);

	kvdefp = lookupkvdef(kvlist, nkvlist, fields[KEY]);
	type = (kvdefp != nil) ? kvdefp->type : TSTRING;

	if ((ep = mkentry(fields[KEY], fields[VALUE], type)) == nil) {
		werrstr("malformed keyval definition");
		free(fields);
		return -1;
	}

	free(fields);
	*epp = ep;

	return 0;
}

/* create a new rgline object from the line definition in s, and assigns it to *lpp.
  * returns 0 on success, -1 on failure.
  * this routine sets errstr */
static
int
strtoline(char *s, rgline **lpp)
{
	char **fields;
	int nfields;
	int effects, type, beats;
	rgline *lp;
	double t, vord;

	if (s == nil || lpp == nil)
		return -1;

	fields = ecalloc(maxrglinefields, sizeof(char *));
	nfields = csvsplit(s, fields, maxrglinefields, ",");
	if (nfields <= LNVOLUME || nfields > LNEFFECTS+1)
		goto badline;

	t = sfatod(fields[LNTIME]);
	vord = sfatod(fields[LNVORD]);
	beats = sfatod(fields[LNBEATS]);
	type = (nfields > LNTYPE) ? sfatoi(fields[LNTYPE]) : RLINE;

	if ((lp = mkrgline(t, vord, beats, type)) == nil)
		goto badline;

	lp->volume = sfatoi(fields[LNVOLUME]);
	lp->sampset = sfatoi(fields[LNSAMPSET]);
	lp->sampindex = sfatoi(fields[LNSAMPINDEX]);
	if (nfields > LNEFFECTS) {
		effects = sfatoi(fields[LNEFFECTS]);
		lp->kiai = (effects & EBKIAI) > 0;
		lp->omitbl = (effects & EBOMIT) > 0;
		lp->effectbits = effects & ~(EBKIAI | EBOMIT);
	}

	free(fields);

	*lpp = lp;
	return 0;

badline:
	werrstr("malformed line definition");
	free(fields);
	return -1;
}

/* create new anchors from the list in s, and add them to *alistpp
  * returns 0 on success, -1 on failure.
  * this routine sets errstr */
static
int
strtoanchlist(char *s, anchor **alistpp)
{
	char **fields;
	int nfields;
	char **afields;
	int anfields;
	anchor *ap;
	int i;
	int x,y;

	if (s == nil || alistpp == nil)
		return -1;

	nfields = csvcountf(s, "|");
	fields = ecalloc(nfields, sizeof(char *));
	csvsplit(s, fields, nfields, "|");

	/* fields[0] contains curve type */
	for (i = 1; i < nfields; i++) {
		if ((anfields = csvcountf(fields[i], ":")) != 2)
			goto badanchor;

		afields = ecalloc(anfields, sizeof(char *));
		csvsplit(fields[i], afields, anfields, ":");

		x = sfatoi(afields[X]);
		y = sfatoi(afields[Y]);
		free(afields);

		if ((ap = mkanch(x, y)) == nil)
			goto badanchor;
		*alistpp = addanchn(*alistpp, ap, 0);
	}
	free(fields);
	return i+1;

badanchor:
	werrstr("bad anchor definition %s", fields[i]);
	free(fields);
	return -1;
}

/* create new slider additions from the list in s, and add them to *sladdsp
  * returns 0 on success, -1 on failure. */
static
int
strtosladds(char *s, int **sladdsp)
{
	char **fields;
	int *sladds;
	int nfields;
	int i;

	if (s == nil || sladdsp == nil)
		return -1;

	nfields = csvcountf(s, "|");

	sladds = ecalloc(nfields, sizeof(int *));
	fields = ecalloc(nfields, sizeof(char *));
	csvsplit(s, fields, nfields, "|");

	for (i = 0; i < nfields; i++)
		sladds[i] = sfatoi(fields[i]);
	free(fields);

	*sladdsp = sladds;
	return nfields;
}

/* create new slider edge samplesets from the list in s, and add them to *slnormsetp
  * and *sladdsetsp
  * returns 0 on success, -1 on failure.
  * this routine sets errstr */
static
int
strtoslsets(char *s, int **slnormsetsp, int **sladdsetsp)
{
	char **fields;
	int nfields;
	char **sfields;
	int snfields;
	int *slnormsets, *sladdsets;
	int i;

	if (s == nil || slnormsetsp == nil || sladdsetsp == nil)
		return -1;

	nfields = csvcountf(s, "|");
	fields = ecalloc(nfields, sizeof(char *));
	csvsplit(s, fields, nfields, "|");

	slnormsets = ecalloc(nfields, sizeof(int));
	sladdsets = ecalloc(nfields, sizeof(int));
	for (i = 0; i < nfields; i++) {
		snfields = csvcountf(fields[i], ":");
		if (snfields != SLADDSET+1) {
			werrstr("malformed edgeset definition %s'", fields[i]);
			free(fields);
			return -1;
		}

		sfields = ecalloc(snfields, sizeof(char *));
		csvsplit(fields[i], sfields, snfields, ":");

		slnormsets[i] = (snfields > SLNORMSET) ? sfatoi(sfields[SLNORMSET]) : 0;
		sladdsets[i] = (snfields > SLADDSET) ? sfatoi(sfields[SLADDSET]) : 0;
		free(sfields);
	}
	free(fields);

	*slnormsetsp = slnormsets;
	*sladdsetsp = sladdsets;

	return nfields;
}

/* create a new hitsample from the sample definition in s, and assign it to *hspp
  * returns 0 on success, -1 on failure.
  * this routine sets errstr */
static
int
strtohitsamp(char *s, hitsamp **hspp)
{
	char **fields;
	int nfields;
	hitsamp *hsp;
	int normal, addition, index, volume;
	Rune *file;

	if (s == nil || hspp == nil)
		return -1;

	fields = ecalloc(maxhitsampfields, sizeof(char *));
	nfields = csvsplit(s, fields, maxhitsampfields, ":");
	if (nfields  < HITSAMPINDEX || nfields > HITSAMPFILE+1)
		goto badsamp;

	normal = sfatoi(fields[HITSAMPNORMAL]);
	addition = sfatoi(fields[HITSAMPADDITIONS]);
	index = (nfields > HITSAMPINDEX) ? sfatoi(fields[HITSAMPINDEX]) : 0;
	volume = (nfields > HITSAMPVOLUME) ? sfatoi(fields[HITSAMPVOLUME]) : 0;
	file = (nfields > HITSAMPFILE) ? estrrunedup(fields[HITSAMPFILE]) : estrrunedup("");

	if ((hsp = mkhitsamp(normal, addition, index, volume, file)) == nil)
		goto badsamp;

	free(fields);
	*hspp = hsp;

	return 0;

badsamp:
	werrstr("malformed hitsample definition");
	free(fields);
	return -1;
}

/* create a new hitobject from the object definition in s, and assign it to *opp
  * returns 0 on success, -1 on failure.
  * this routine sets errstr */
static
int
strtoobj(char *s, hitobject **opp)
{
	char **fields;
	int nfields;
	hitobject *op;
	int x, y, typebits, type;
	double t;

	if (s == nil || opp == nil)
		return -1;

	fields = ecalloc(maxobjfields, sizeof(char *));
	nfields = csvsplit(s, fields, maxobjfields, ",");
	if (nfields < OBJADDITIONS)
		return -1;

	x = sfatoi(fields[OBJX]);
	y = sfatoi(fields[OBJY]);
	t = sfatod(fields[OBJTIME]);

	typebits = sfatoi(fields[OBJTYPE]);
	type = typebits & TBTYPE;

	if ((op = mkobj(type, t, x, y)) == nil)
		return -1;

	if ((typebits & TBNEWCOMBO) > 0)
	op->newcombo = (typebits & TBNEWCOMBO) > 0 ? 1 : 0;
	op->comboskip = (typebits & TBCOLOR) >> TBCOLORSHIFT;

	op->typebits = typebits & ~(TBTYPE|TBCOLOR|TBNEWCOMBO|TBHOLD);
	op->additions = sfatoi(fields[OBJADDITIONS]);

	switch (op->type) {
	case TCIRCLE:
		if (nfields > OBJCIRCLEHITSAMP)
			if (strtohitsamp(fields[OBJCIRCLEHITSAMP], &op->hitsamp) < 0)
				goto badstr;

		break;
	case TSLIDER:
		op->slides = sfatoi(fields[OBJSLIDES]);
		op->length = sfatod(fields[OBJLENGTH]);

		op->curve = fields[OBJCURVES][0];
		if (strtoanchlist(fields[OBJCURVES], &op->anchors) < 0)
			goto badstr;
		if (nfields > OBJEDGESOUNDS)
			if ((op->nsladditions = strtosladds(fields[OBJEDGESOUNDS], &op->sladditions)) < 0)
				goto badstr;
		if (nfields > OBJEDGESETS)
			if ((op->nslsets = strtoslsets(fields[OBJEDGESETS], &op->slnormalsets, &op->sladditionsets)) < 0)
				goto badstr;
		if (nfields > OBJSLIDERHITSAMP)
			if (strtohitsamp(fields[OBJSLIDERHITSAMP], &op->hitsamp) < 0)
				goto badstr;

		break;
	case TSPINNER:
		op->spinnerlength = sfatod(fields[OBJENDTIME]) - t;
		if (nfields > OBJSPINNERHITSAMP)
			if (strtohitsamp(fields[OBJSPINNERHITSAMP], &op->hitsamp) < 0)
				goto badstr;

		break;
	default:
		/* can't happen */
		werrstr("bad type for object t=%ld: '%b'", op->t, op->type);
		nukeobj(op);
		return -1;
	}

	*opp = op;

	free(fields);
	return 0;

badstr:
	/* assume errstr was set by strto* method */
	free(fields);
	return -1;
}

/* create a new beatmap object */
beatmap *
mkbeatmap()
{
	beatmap *new = ecalloc(1, sizeof(beatmap));

	new->general = mktable(8);
	new->editor = mktable(8);
	new->metadata = mktable(8);
	new->difficulty = mktable(8);
	new->colours = mktable(8);

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

/* read a .osu file from bp, and deserialise all sections into the relevant
  * bmp structs. this routine calls multiple subroutines that all set the errstr.
  * returns 0 on success, negative values on failure. */
int
readmap(Biobuf *bp, beatmap *bmp)
{
	char *s, *e;
	table *tp;
	entry *ep;
	rgline *lp;
	hitobject *op;
	kvdef *kvlist;
	int nkvlist;
	int wstrip;

	bmp->version = nextline(bp);

	while ((s = nextsection(bp)) != nil) {
		if (strcmp(s, "[TimingPoints]") == 0) {
			while ((e = nextdirective(bp)) != nil) {
				if (strtoline(e, &lp) < 0)
					return -2;
				bmp->rglines = addrglinet(bmp->rglines, lp);
				free(e);
			}

			free(s);
			continue;
		} else if (strcmp(s, "[HitObjects]") == 0) {
			while ((e = nextdirective(bp)) != nil) {
				if (strtoobj(e, &op) < 0)
					return -2;
				bmp->objects = addobjt(bmp->objects, op);
				free(e);
			}

			free(s);
			continue;
		}

		if (strcmp(s, "[Events]") == 0) {
			bmp->events = readsection(bp);
			free(s);
			continue;
		}

		wstrip = 1;
		if (strcmp(s, "[General]") == 0) {
			tp =  bmp->general;
			kvlist = kvgeneral;
			nkvlist = nkvgeneral;
		} else if (strcmp(s, "[Editor]") == 0) {
			tp =  bmp->editor;
			kvlist = kveditor;
			nkvlist = nkveditor;
		} else if (strcmp(s, "[Metadata]") == 0) {
			tp =  bmp->metadata;
			kvlist = kvmetadata;
			nkvlist = nkvmetadata;
			wstrip = 0;
		} else if (strcmp(s, "[Difficulty]") == 0) {
			tp =  bmp->difficulty;
			kvlist = kvdifficulty;
			nkvlist = nkvdifficulty;
		} else if (strcmp(s, "[Colours]") == 0) {
			tp =  bmp->colours;
			kvlist = kvcolours;
			nkvlist = nkvcolours;
		} else {
			werrstr("bad section %s", s);
			free(s);
			return -1;
		}

		while ((e = nextdirective(bp)) != nil) {
			if (strtoentry(e, &ep, kvlist, nkvlist, wstrip) < 0)
				return -3;
			addentry(tp, ep);
			free(e);
		}

		free(s);
	}

	return 0;
}

/* first write all tp entries listed in kvlist to bp, before writing
  * all remaining entries that do not appear in kvlist. writeentries
  * assumes that all entries not in kvlist have the type TSTRING.
  * returns 0 on success, negative values on failure */
static
int
writeentries(Biobuf *bp, table *tp, kvdef *kvlist, int nkvlist)
{
	entry **written;
	entry *ep;
	int i, n, found;

	if (tp == nil || bp == nil || kvlist == nil || nkvlist < 0)
		return -1;

	written = ecalloc(tp->nentry, sizeof(entry *));
	for (i = 0, n = 0; i < nkvlist; i++) {
		ep = lookupentry(tp, kvlist[i].key);
		if (ep != nil) {
			Bprint(bp, "\r\n");
			switch(ep->type) {
			case TRUNE:
				Bprint(bp, kvlist[i].fmt, kvlist[i].key, ep->S);
				break;
			case TSTRING:
				Bprint(bp, kvlist[i].fmt, kvlist[i].key, ep->s);
				break;
			case TINT:
				Bprint(bp, kvlist[i].fmt, kvlist[i].key, ep->i);
				break;
			case TLONG:
				Bprint(bp, kvlist[i].fmt, kvlist[i].key, ep->l);
				break;
			case TFLOAT:
				Bprint(bp, kvlist[i].fmt, kvlist[i].key, ep->f);
				break;
			case TDOUBLE:
				Bprint(bp, kvlist[i].fmt, kvlist[i].key, ep->d);
				break;
			}
			written[n++] = ep;
		}
	}

	if (n == tp->nentry) {
		free(written);
		return 0;
	}

	for (ep = nextentry(tp, nil), found = 0; ep != nil; ep = nextentry(tp, ep), found = 0) {
		for (i = 0; i < n; i++) {
			if (written[i] == ep) {
				found = 1;
				break;
			}
		}

		if (found == 0) {
			if (ep->type != TSTRING) {
				werrstr("unknown key %s does not have type TSTRING");
				free(written);
				return -1;
			}
			Bprint(bp, "\r\n%s: %s", ep->key, ep->s);
		}
	}

	free(written);
	return 0;
}

/* write all rglines from lines to bp
  * returns 0 on success, -1 on failure */
static
int
writerglines(Biobuf *bp, rgline *lines)
{
	rgline *np;
	double vord;
	int effects;

	if (bp == nil || lines == nil)
		return -1;

	for (np = lines; np != nil; np = np->next) {
		vord = (np->type == GLINE) ? np->velocity : np->duration;
		effects = np->effectbits;
		effects = (np->kiai > 0) ? effects | EBKIAI : effects & ~EBKIAI;
		effects = (np->omitbl > 0) ? effects | EBOMIT : effects & ~EBOMIT;

		Bprint(bp, "\r\n%.16G,%.16G,%d,%d,%d,%d,%d,%d", np->t, vord, np->beats, np->sampset, np->sampindex, np->volume, np->type, effects);
	}

	return 0;
}

/* write all hitobjects from objects to bp
  * returns 0 on success, -1 on failure */
static
int
writehitobjects(Biobuf *bp, hitobject *objects)
{
	int i;
	hitobject *np;
	anchor *ap;
	int typebits;

	if (bp == nil || objects == nil)
		return -1;

	for (np = objects; np != nil; np = np->next) {
		Bprint(bp, "\r\n");
		typebits = np->typebits | np->type | (np->comboskip << TBCOLORSHIFT);
		if (np->newcombo > 0)
			typebits |= TBNEWCOMBO;

		Bprint(bp, "%d,%d,%.16G,%d,%d", np->x, np->y, np->t, typebits, np->additions);

		switch(np->type) {
		case TCIRCLE:
			break;
		case TSLIDER:
			Bprint(bp, ",%c", np->curve);
			for (ap = np->anchors; ap != nil; ap = ap->next)
				Bprint(bp, "|%d:%d", ap->x, ap->y);

			Bprint(bp, ",%d,%.16G", np->slides, np->length);

			if (np->sladditions != nil) {
				Bprint(bp, ",%d", np->sladditions[0]);
				for (i = 1; i < np->nsladditions; i++)
					Bprint(bp, "|%d", np->sladditions[i]);
			}

			if (np->slnormalsets != nil && np->sladditionsets != nil) {
				Bprint(bp, ",%d:%d", np->slnormalsets[0], np->sladditionsets[0]);
				for (i = 1; i < np->nslsets; i++)
					Bprint(bp, "|%d:%d", np->slnormalsets[i], np->sladditionsets[i]);
			}

			break;
		case TSPINNER:
			Bprint(bp, ",%.11G", np->t + np->spinnerlength);

			break;
		}

		if (np->hitsamp != nil) {
			Bprint(bp, ",%d:%d:%d", np->hitsamp->normal, np->hitsamp->addition, np->hitsamp->index);
			Bprint(bp, ":%d:%S", np->hitsamp->volume, np->hitsamp->file);
		}
	}

	return 0;
}

/* write all sections to bp */
int
writemap(Biobuf *bp, beatmap *bmp)
{
	Bprint(bp, "%s\r\n", bmp->version);

	if (bmp->general->nentry > 0) {
		Bprint(bp, "\r\n[General]");
		writeentries(bp, bmp->general, kvgeneral, nkvgeneral);
	}
	 if (bmp->editor->nentry > 0) {
		Bprint(bp, "\r\n\r\n[Editor]");
		writeentries(bp, bmp->editor, kveditor, nkveditor);
	}
	if (bmp->metadata->nentry > 0) {
		Bprint(bp, "\r\n\r\n[Metadata]");
		writeentries(bp, bmp->metadata, kvmetadata, nkvmetadata);
	}
	if (bmp->difficulty->nentry > 0) {
		Bprint(bp, "\r\n\r\n[Difficulty]");
		writeentries(bp, bmp->difficulty, kvdifficulty, nkvdifficulty);
	}
	if (bmp->events != nil) {
		Bprint(bp, "\r\n\r\n[Events]");
		Bprint(bp, "\r\n%s", bmp->events);
	}
	if (bmp->rglines != nil) {
		Bprint(bp, "\r\n[TimingPoints]");
		writerglines(bp, bmp->rglines);
	}
	if (bmp->colours->nentry > 0) {
		Bprint(bp, "\r\n\r\n[Colours]");
		writeentries(bp, bmp->colours, kvcolours, nkvcolours);
	}
	if (bmp->objects != nil) {
		Bprint(bp, "\r\n\r\n[HitObjects]");
		writehitobjects(bp, bmp->objects);
	}

	return 0;
}
