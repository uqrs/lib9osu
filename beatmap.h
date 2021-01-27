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

enum {
	NOMEM=-1,
	BADKEY=-2,
	BADCURVE=-3,
	BADOBJECT=-4,
	BADLINE=-5,
	BADTYPE=-6,
	BADSAMPLE=-7,
	BADEDGESETS=-8,
	BADANCHOR=-9,
};

beatmap *mkbeatmap();
void nukebeatmap(beatmap *bmp);
int readmap(beatmap *bmp, Biobuf *bp);
int writemap(beatmap *bmp, Biobuf *bp);