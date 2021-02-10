#include <u.h>
#include <libc.h>
#include "aux.h"
#include "hash.h"

/* create a new table with a hash size of n */
table *
mktable(int n)
{
	table *new;

	if (n < 1)
		return nil;

	new = ecalloc(1, sizeof(table));

	new->entries = ecalloc(n, sizeof(entry *));
	new->maxentry = n;
	new->nentry = 0;

	return new;
}

/* obliterate table tp */
void
nuketable(table *tp)
{
	int i;
	entry *np, *next;

	if (tp == nil)
		return;

	for (i = 0; i < tp->maxentry; i++) {
		for (np = tp->entries[i]; np != nil; np = next) {
			next = np->next;
			nukeentry(np);
		}
	}

	free(tp->entries);
	free(tp);
}

/* create an entry object with the key field, and deserialise
  * value depending on type */
entry *
mkentry(char *key, char *value, int type)
{
	entry *new;

	if (key == nil || value == nil || type < TRUNE || type > TDOUBLE)
		return nil;

	new = ecalloc(1, sizeof(entry));
	new->key = strdup(key);
	new->type = type;
	new->next = nil;

	switch (new->type) {
	case TRUNE:
		new->S = estrrunedup(value);
		break;
	case TSTRING:
		new->s = estrdup(value);
		break;
	case TINT:
		new->i = atoi(value);
		break;
	case TLONG:
		new->l = atol(value);
		break;
	case TFLOAT:
		new->f = atof(value);
		break;
	case TDOUBLE:
		new->d = strtod(value, nil);
		break;
	}

	return new;
}

/* let entry ep buy the farm. must call rmentry first if entry is in a table */
void
nukeentry(entry *ep)
{
	if (ep == nil)
		return;

	free(ep->key);
	if (ep->type == TRUNE)
		free(ep->S);
	else if (ep->type == TSTRING)
		free(ep->s);
	free(ep);
}

/* generate a hash value across range n from s */
static
uint
hash(char *s, int n)
{
	static int mult = 37;
	uint h;
	uchar *p;

	h = 0;
	for (p = (uchar *) s; *p != '\0'; p++)
		h = mult * h + *p;

	return h % n;
}

/* find the entry in tp whose key field matches key
  * returns the found entry, or nil if no matches.
  * lookup searches case-insensitively */
entry *
lookupentry(table *tp, char *key)
{
	uint h;
	entry *np;

	if (tp == nil || key == nil)
		return nil;

	h = hash(key, tp->maxentry);
	for (np = tp->entries[h]; np != nil; np = np->next)
		if (cistrcmp(key, np->key) == 0)
			return np;

	return nil;
}

/* return the next entry in ep's hash chain, or the first
  * entry of the next hash chain if ep is the final object in
  * its chain. if ep is nil, nextentry returns the first entry
  * in the next hash chain.
  * returns nil when the entire table has been exhausted */
entry *
nextentry(table *tp, entry *ep)
{
	uint h;
	entry *np;

	if (tp == nil)
		return nil;

	if (ep == nil) {
		h = 0;
		for (h = 0; h != tp->maxentry; h++)
			if (tp->entries[h] != nil)
				return tp->entries[h];

		return nil;
	}

	h = hash(ep->key, tp->maxentry);
	for (np = tp->entries[h]; np != nil; np = np->next) {
		if (np->next == nil) {
			for (h++; h != tp->maxentry; h++)
				if (tp->entries[h] != nil)
					return tp->entries[h];

			return nil;
		} else if (np == ep) {
			return np->next;
		}
	}

	/* entry not in table */
	return nil;
}

/* prepend ep to the appropriate hash chain in tp */
entry *
addentry(table *tp, entry *ep)
{
	uint h;

	if (tp == nil || ep == nil)
		return nil;

	h = hash(ep->key, tp->maxentry);
	ep->next = tp->entries[h];
	tp->entries[h] = ep;
	tp->nentry++;

	return ep;
}

/* remove entry ep from table tp
  * returns a pointer to ep, ready to be fed into nukeentry if so desired */
entry *
rmentry(table *tp, entry *ep)
{
	uint h;
	entry *np;

	if (tp == nil || ep == nil)
		return nil;

	h = hash(ep->key, tp->maxentry);
	if (tp->entries[h] == nil) {
		return ep;
	} else if (tp->entries[h] == ep) {
		tp->entries[h] = ep->next;
		ep->next = nil;
		tp->nentry--;
		return ep;
	}

	for (np = tp->entries[h]; np->next != nil; np = np->next) {
		if (np->next == ep) {
			np->next = ep->next;
			ep->next = nil;
			tp->nentry--;
			return ep;
		}
	}

	return nil; /* unreachable */
}
