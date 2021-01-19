/* osu! beatmap types & parsing/manipulation functions */
typedef struct beatmap {
	/* [General] */
	Rune *audiof;		/* relative path to mp3 file */
	ulong leadin;		/* milliseconds before audio begins playing */
	ulong previewt;	/* time in milliseconds when audio preview should start */
	int countdown;		/* countdown timer speed */
	int sampset;		/* default beatmap sample set.  see hitsound.h:/sampsets/ */
	int stackleniency;	/* stack leniency */
	int mode;			/* force gameplay mode */
	int letterbox;		/* letterbox screen in breaks */
	int widescreensb;	/* widescreen storyboard */

	/* [Editor] */
	ulong *bookmarks;	/* bookmark list */
	int nbookmarks;	/* number of bookmarks in *bookmarks */
	float distancesnap;	/* distance snap multiplier */
	float beatdivisor;	/* beat snap divisor */
	int gridsize;		/* grid size */
	float timelinezoom;	/* editor timeline zoom */

	/* [Metadata] */
	char *title;		/* song title */
	Rune *utf8title;		/* song title in UTF-8 */
	char *artist;		/* artist */
	Rune *utf8artist;	/* artist in UTF-8 */
	char *author;		/* beatmap author (username) */
	char *diffname;	/* difficulty name */
	char *source;		/* original media the song was produced for */
	char *tags;		/* search terms */
	uint id, setid;		/* beatmap & beatmapset ID */

	/* [Difficulty] */
	float hp;			/* HP drain rate */
	float cs;			/* circle size */
	float ar;			/* approach rate */
	float od;			/* overall difficulty */
	float slmultiplier;	/* slider velocity multiplier */
	int sltickrate;		/* slider tick rate */

	/* [Events] */
	char *events;		/* raw contents of the [Events] section. */

	/* [TimingPoints] */
	rline *rlines;		/* head of redline list */
	gline *glines;		/* head of greenline list */

	/* [Colours] */
	long *colours;		/* combo colour hex codes. nil for default colours */
	int ncolours;		/* number of colours in *colours */

	/* [HitObjects] */
	hitobject *objects;	/* head of object list */
} beatmap;

enum {
	NOMEM=-1,
	BADKEY=-2,
	BADCURVE=-3,
	BADOBJECT=-4,
	BADLINE=-5,
};

beatmap *mkbeatmap();
void nukebeatmap(beatmap *bmp);
int loadmap(beatmap *bmp, Biobuf *bp);
