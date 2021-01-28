#include <u.h>
#include <libc.h>
#include "aux.h"
#include "hitsound.h"
#include "hitobject.h"

/* creates a new object */
hitobject *
mkobj(uchar type, double t, int x, int y)
{
	hitobject *new;

	new = ecalloc(1, sizeof(hitobject));

	new->type = type;
	new->t = t;
	new->x = x;
	new->y = y;
	new->next = nil;

	return new;
}

/* free an object along with its anchors; MUST call rmobj first if the object is in a list */
void
nukeobj(hitobject *op)
{
	anchor *ap, *next;
	for (ap = op->anchors; ap != nil; ap = next) {
		next = ap->next;
		free(ap);
	}

	free(op->sladditions);
	free(op->slnormalsets);
	free(op->sladditionsets);
	nukehitsample(op->samp);

	free(op);
}

/* inserts an object into the list based on its time value.
  * returns a pointer to the list's head */
hitobject *
addobjt(hitobject *listp, hitobject *op)
{
	hitobject *np;

	if (listp == nil)
		return op;

	if (op->t <= listp->t) {
		op->next = listp;
		return op;
	}

	for (np = listp; np != nil; np = np->next) {
		if (op->t == np->t) {
			for (; np->next != nil && np->next->t == op->t; np = np->next)
				;
			op->next = np->next;
			np->next = op;
			return listp;
		} else if (np->next == nil) {
			np->next = op;
			return listp;
		} else if (op->t > np->t && op->t < np->next->t) {
			op->next = np->next;
			np->next = op;
			return listp;
		}
	}

	return nil; /* unreachable */
}

/* changes hitobject op's time to t, and adjust its position in listp */
hitobject *
moveobjt(hitobject *listp, hitobject *op, double t)
{
	listp = rmobj(listp, op);
	op->t = t;
	return addobjt(listp, op);
}

/* removes the hitobject pointed to by op from listp */
hitobject *
rmobj(hitobject *listp, hitobject *op)
{
	hitobject *np, *newlistp;

	if (listp == nil) {
		return nil;
	} else if (listp == op) {
		newlistp = listp->next;
		listp->next = nil;
		return newlistp;
	}

	for (np = listp; np->next != nil; np = np->next) {
		if (np->next == op) {
			np->next = op->next;
			op->next = nil;
			return listp;
		}
	}

	return nil; /* unreachable */ 
}

/* returns a pointer to an object with time t in listp.
  * If no object in listp  has time t, lookupobjt returns the
  * latest hitobject  whose timestamp comes before t in listp. */
hitobject *
lookupobjt(hitobject *listp, double t)
{
	hitobject *np;
	if (listp == nil)
		return nil;

	for (np = listp; np->next != nil; np = np->next) {
		if (np->next->t > t || np->t == t)
			return np;
	}

	return np;
}

/* returns a pointer to the n-th object in listp.
  * returns nil if there is no n-th object.
  * returns the last object if n is 0 */
hitobject *
lookupobjn(hitobject *listp, uint n)
{
	hitobject *np;
	int i = 0;

	if (n == 0) {
		for (np = listp; np->next != nil; np = np->next)
			;

		return np;
	}

	for (np = listp; np != nil; np = np->next) {
		if (++i == n)
			return np;
	}

	return nil;
}

/* creates a new anchor */
anchor *
mkanch(int x, int y)
{
	anchor *new = (anchor *) ecalloc(1, sizeof(anchor));

	new->x = x;
	new->y = y;
	new->next = nil;

	return new;
}

/* adds an anchor to alistp in position n.
  * if n is 0, addanchn appends anchor to the end of the list.
   * returns a pointer to the list's head */
anchor *
addanchn(anchor *alistp, anchor *ap, uint n)
{
	anchor *np;
	int i;

	if (alistp == nil)
		return ap;

	if (n == 1) {
		ap->next = alistp;
		return ap;
	}

	if (n == 0) {
		for (np = alistp; np->next != nil; np = np->next)
			;
	} else {
		np = alistp;
		for (i = 1; i < n-1; i++) {
			if (np->next == nil)
				break;

			np = np->next;
		}
	}

	ap->next = np->next;
	np->next = ap;

	return alistp;
}
