/* hitobject data-types & manipulation functions */
/* for significance to these numbers, see: beatmap.c:0/BTYPE/ */
enum {
	TCIRCLE = 1,
	TSLIDER = 2,
	TSPINNER = 8,
} objtypes;

enum {
	CRVNONE= '\0',
	CRVLINEAR = 'L',
	CRVCATMULL = 'C',
	CRVBEZIER = 'B',
	CRVPERFECT = 'P',
} curvetypes;

typedef struct anchor anchor;
typedef struct anchor {
	int x,y;			/* x and y positions in 'osu pixels' */
	anchor *next;		/* next anchor in the list. If nil, this anchor is a slider tail. */
} anchor;

/* A hitobject may refer to a circle, slider, or spinner. */
typedef struct hitobject hitobject;
typedef struct hitobject {
	ulong t;			/* timestamp in ms */
	int x,y;			/* x and y positions in 'osu pixels' */
	uchar type;		/* one of enum objtypes */
	double length;		/* "visual length in osu! pixels." may be truncated with "no consequences". */

	int additions;		/* bit-flagged integer for additions. see hitsound.h:/additionbits/ */
	int normalset;		/* sample set for the 'normal' sound */
	int additionset;		/* sample set for whistle, finish and clap sounds */
	int *sladditions;	/* additions for each slider edge hit */
	int *slnormalsets;	/* sample sets for each slider edge hit */
	int *sladditionsets;	/* sample sets for additions for each slider edge hit */
	int nsladdition;		/* number of elements in sladditions(ets). Practically always slides+1 */

	int sampindex;		/* custom sample index */
	int volume;		/* sample volume percentage */
	Rune *filename;	/* filename for custom addition sound */

	int newcombo;		/* start new combo on this object */
	int comboskip;		/* 'how many combo colours to skip' */

	hitobject *next;	/* next node in object list */ 

	/* sliders */
	int slides;			/* the amount of times this slider reverses +1 */
	char curve;		/* one of enum curvetypes */
	anchor *alistp;		/* head of anchor list */

	/* spinners */
	ulong spinnerlength;  /* spinner duration in ms */
} hitobject;

hitobject *mkobj(uchar type, ulong t, int x, int y);
void nukeobj(hitobject *obj);
hitobject *addobjt(hitobject *listp, hitobject *op);
hitobject *moveobjt(hitobject *listp, hitobject *op, ulong t);
hitobject *rmobj(hitobject *listp, hitobject *op);
hitobject *lookupobjt(hitobject *listp, ulong t);
hitobject *lookupobjn(hitobject *listp, uint n);
anchor *mkanch(int x, int y);
anchor *addanchn(anchor *alistp, anchor *ap, uint n);
