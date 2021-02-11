/* osu! beatmap types & parsing/manipulation functions */
typedef struct beatmap {
	char *version;		/* version header */

	table *general;		/* [General] */
	table *editor;		/* [Editor] */
	long *bookmarks;	/* bookmark list */
	int nbookmark;		/* number of bookmarks in *bookmarks */
	table *metadata;	/* [Metadata] */
	table *difficulty;	/* [Difficulty] */
	char *events;		/* [Events] */
	rgline *rglines;		/* [TimingPoints] */
	table *colours;		/* [Colours] */

	/* [HitObjects] */
	hitobject *objects;	/* head of object list */
} beatmap;

/* key-value pair definition */
typedef struct kvdef {
	char *key;		/* string key of entry */
	char *fmt;		/* format string for output */
	int type;		/* the data type of the associated value. see hash.h:2,9 */
} kvdef;

extern kvdef kvgeneral[];
extern kvdef kveditor[];
extern kvdef kvmetadata[];
extern kvdef kvdifficulty[];
extern kvdef kvcolours[];

extern int nkvgeneral;
extern int nkveditor;
extern int nkvmetadata;
extern int nkvdifficulty;
extern int nkvcolours;

int strtoentry(char *s, entry **epp, kvdef *kvlist, int nkvlist, int wstrip);
int strtoline(char *s, rgline **lpp);
int strtoanchlist(char *s, anchor **alistpp);
int strtosladds(char *s, int **sladdsp);
int strtoslsets(char *s, int **slnormsetsp, int **sladdsetsp);
int strtohitsamp(char *s, hitsamp **hspp);
int strtoobj(char *s, hitobject **opp);

beatmap *mkbeatmap();
void nukebeatmap(beatmap *bmp);
int readmap(Biobuf *bp, beatmap *bmp);
int writemap(Biobuf *bp, beatmap *bmp);

enum {
	BADARGS=-1,
	BADKEY=-2,
	BADCURVE=-3,
	BADOBJECT=-4,
	BADLINE=-5,
	BADTYPE=-6,
	BADSAMPLE=-7,
	BADEDGESETS=-8,
	BADANCHOR=-9,
	BADSECTION=-10,
	BADENTRY=-11,
};