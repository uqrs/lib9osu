/* hitobject data-types & manipulation functions */
/* for significance to these numbers, see: beatmap.c:0/BTYPE/ */
enum {
	TCIRCLE = 1,
	TSLIDER = 2,
	TSPINNER = 8,
} objtypes;

enum {
	CRVNONE= '\0',	/* filler */
	CRVLINEAR = 'L',	/* linear */
	CRVCATMULL = 'C',	/* catmull-rom curve */
	CRVBEZIER = 'B',	/* bezier curve */
	CRVPERFECT = 'P',	/* perfect circular curve */
} curvetypes;

typedef struct anchor anchor;
typedef struct anchor {
	int x, y;			/* x and y positions in 'osu pixels' */
	anchor *next;		/* next anchor in the list. If nil, this anchor is a slider tail. */
} anchor;

/* A hitobject may refer to a circle, slider, or spinner. */
typedef struct hitobject hitobject;
typedef struct hitobject {
	hitobject *next;	/* next node in object list */

	long t;			/* timestamp in ms */
	int x,y;			/* x and y positions in 'osu pixels' */
	uchar type;		/* one of enum objtypes */
	double length;		/* "visual length in osu! pixels." may be truncated with "no consequences". */

	/* hitsampling */
	int additions;		/* bit-flagged integer for additions. see hitsound.h:/additionbits/ */
	int *sladditions;	/* additions for each slider edge hit */
	int *slnormalsets;	/* sample sets for each slider edge hit */
	int *sladditionsets;	/* sample sets for additions for each slider edge hit */
	int nsladdition;		/* number of elements in sladditions(ets). Practically always slides+1 */
	hitsample *samp;	/* custom hitsample for object */

	/* colours */
	int newcombo;		/* start new combo on this object */
	int comboskip;		/* 'how many combo colours to skip' */


	/* sliders */
	int slides;			/* the amount of times this slider reverses +1 */
	char curve;		/* one of enum curvetypes */
	anchor *anchors;	/* head of anchor list */

	/* spinners */
	long spinnerlength;  /* spinner duration in ms */
} hitobject;

hitobject *mkobj(uchar type, long t, int x, int y);
void nukeobj(hitobject *obj);
hitobject *addobjt(hitobject *listp, hitobject *op);
hitobject *moveobjt(hitobject *listp, hitobject *op, long t);
hitobject *rmobj(hitobject *listp, hitobject *op);
hitobject *lookupobjt(hitobject *listp, long t);
hitobject *lookupobjn(hitobject *listp, uint n);
anchor *mkanch(int x, int y);
anchor *addanchn(anchor *alistp, anchor *ap, uint n);
