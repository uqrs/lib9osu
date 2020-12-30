/* red, green and blue lines & manipulation functions */
typedef struct rline rline;
typedef struct rline {
	ulong t;				/* timestamp in ms */
	ulong duration;			/* beat duration in ms */
	int beats;				/* time signature in beats/4 */
	int volume;
	int kiai;				/* kiai time enabled yes/no */

	rline *next;			/* next node in rline list */
} rline;

typedef struct gline gline;
typedef struct gline {
	ulong t;				/* timestamp in ms */
	int velocity;			/* ? */
	int volume;			/* volume percentage */
	int kiai;				/* kiai time enabled yes/no */

	gline *next;			/* next node in gline list */
} gline;

gline *mkgline(ulong t, double velocity, int volume, int kiai);
void nukegline(gline *gline);
gline *addglinet(gline *glistp, gline *glp);
gline *moveglinet(gline *glistp, gline *glp, ulong t);
gline *rmgline(gline *glistp, gline *glp);
rline *mkrline(ulong t, ulong duration, int beats, int volume, int kiai);
void nukerline(rline *rline);
rline *addrlinet(rline *rlistp, rline *rlp);
rline *moverlinet(rline *rlistp, rline *rlt, ulong t);
rline *rmrline(rline *rlistp, rline *glp);
