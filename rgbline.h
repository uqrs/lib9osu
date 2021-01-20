/* red, green and blue lines & manipulation functions */
typedef struct rline rline;
typedef struct rline {
	ulong t;				/* timestamp in ms */
	double duration;		/* beat duration in ms */
	int beats;				/* time signature in beats/4 */
	int kiai;				/* kiai time enabled yes/no */

	int sampset;			/* sample set */
	int sampindex;			/* custom sample index; 0 for default */
	int volume;			/* volume percentage */

	rline *next;			/* next node in rline list */
} rline;

typedef struct gline gline;
typedef struct gline {
	ulong t;				/* timestamp in ms */
	double velocity;		/* ? */
	int beats;				/* ignored for greenlines */
	int kiai;				/* kiai time enabled yes/no */

	int sampset;			/* sample set */
	int sampindex;			/* custom sample index; 0 for default */
	int volume;			/* volume percentage */

	gline *next;			/* next node in gline list */
} gline;

gline *mkgline(ulong t, double velocity);
void nukegline(gline *gline);
gline *addglinet(gline *glistp, gline *glp);
gline *moveglinet(gline *glistp, gline *glp, ulong t);
gline *rmgline(gline *glistp, gline *glp);
rline *mkrline(ulong t, double duration, int beats);
void nukerline(rline *rline);
rline *addrlinet(rline *rlistp, rline *rlp);
rline *moverlinet(rline *rlistp, rline *rlt, ulong t);
rline *rmrline(rline *rlistp, rline *glp);
