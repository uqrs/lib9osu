/* osu! beatmap types & parsing/manipulation functions */
typedef struct beatmap {
	char *title;		/* song title */
	Rune *utf8title;		/* song title in UTF-8 */
	char *artist;		/* artist */
	Rune *utf8artist;	/* artist in UTF-8 */
	char *author;		/* beatmap author (username) */
	char *diffname;	/* difficulty name */
	char *source;		/* original media the song was produced for */
	char *tags;		/* search terms */
	uint id, setid;		/* beatmap & beatmapset ID */

	Rune *audiof;		/* relative path to mp3 file */
	ulong leadin;		/* milliseconds before audio begins playing */
	ulong previewt;	/* time in milliseconds when audio preview should start */
	uchar countdown;	/* countdown timer speed */
	uchar stackleniency;/* stack leniency */
	uchar mode;		/* force gameplay mode */

	int letterbox;		/* letterbox screen in breaks */
	int widescreensb;	/* widescreen storyboard */
	
	rline *rlines;		/* head of redline list */
	gline *glines;		/* head of greenline list */
	hitobject *objects;	/* head of object list */
} beatmap;

enum {
	NOMEM=-1,
	BADKEY=-2,
	BADCURVE=-3,
	BADOBJECT=-4,
	BADLINE=-5,
};

beatmap *loadmap(int fd);
