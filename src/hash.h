/* hash table data types & associated manipulation functions */
enum types {
	TRUNE=0,
	TSTRING,
	TINT,
	TLONG,
	TFLOAT,
	TDOUBLE,
} types;

typedef struct entry entry;
typedef struct entry {
	entry *next;	/* next in hash chain */

	char *key;		/* configuration key */

	int type;		/* one of (:0/enum types/) */
	union {
		Rune *S;
		char *s;
		int i;
		long l;
		float f;
		double d;
	};
} entry;

typedef struct table table;
typedef struct table {
	entry **entries;		/* list of entries */
	int maxentry;		/* number of entry pointers in entries[] */
	int nentry;			/* total number of entries inserted into table */
} table;

table *mktable(int n);
void nuketable(table *tp);
entry *mkentry(char *key, char *value, int type);
void nukeentry(entry *ep);
entry *lookupentry(table *tp, char *key);
entry *nextentry(table *tp, entry *ep);
entry *addentry(table *tp, entry *ep);
entry *rmentry(table *tp, entry *ep);
