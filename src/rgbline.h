/* red, green and blue lines & manipulation functions */
enum linetype {
	GLINE=0,
	RLINE=1,
} linetype;

typedef struct rgline rgline;
typedef struct rgline {
	rgline *next;	/* next rgline in list */

	double t;		/* timestamp in ms */

	int type;		/* one of enum linetype */
	union {
		double duration;	/* note duration in milliseconds (redline) */
		double velocity;	/* bogus slider velocity value (greenline) */
	};

	int beats;		/* time signature in beats/4 */
	int kiai;		/* kiai time enabled yes/no */
	int omitbl;		/* omit first barline in osu!mania and taiko yes/no */
	int effectbits;	/* remaining effect bits (older osu! maps) */
				/* effectbits' kiai and omit barline bits are overridden
				  * by kiai and omitbl on write */

	int sampset;	/* sample set */
	int sampindex;	/* custom sample index; 0 for default */
	int volume;	/* volume percentage */

} line;

line *mkrgline(double t, double vord, int beats, int type);
void nukergline(rgline *lp);
line *addrglinet(rgline *listp, rgline *lp);
line *moverglinet(rgline *listp, rgline *lp, double t);
line *rmrgline(rgline *listp, rgline *lp);
