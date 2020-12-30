#include <u.h>
#include <libc.h>
#include "hitobject.h"

/* create a new object */
hitobject *
mkobj(uchar type, ulong t, int x, int y)
{
	hitobject *new = malloc(sizeof(hitobject));

	if (new == nil)
		return nil;

	new->type = type;
	new->t = t;
	new->x = x;
	new->y = y;

	new->next = nil;
	new->alistp = nil;

	return new;
}

/* free an object along with its anchors; MUST call rmobj first if the object is in a list */
void
nukeobj(hitobject *op)
{
	anchor *ap, *next;
	for (ap = op->alistp; ap != nil; ap = next) {
		next = ap->next;
		free(ap);
	}
	free(op);
}

/* insert object into the list based on its time value.
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

	for (np = listp; np->next != nil; np = np->next) {
		if (op->t > np->t && op->t <= np->next->t) {
			op->next = np->next;
			np->next = op;
			return listp;
		}
	}

	np->next = op;
	return listp;
}

/* set hitobject's time to 't', and adjust its position in the list */
hitobject *
moveobjt(hitobject *listp, hitobject *op, ulong t)
{
	listp = rmobj(listp, op);
	op->t = t;
	return addobjt(listp, op);
}

/* remove hitobject 'op' from 'listp' */
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

/* return a pointer to the first hitobject with time 't'.
  * if no object has time 't', then return the last object where np->t < t */
hitobject *
lookupobjt(hitobject *listp, ulong t)
{
	hitobject *np;
	if (listp == nil)
		print("listp nil\n");

	for (np = listp; np->next != nil; np = np->next) {
		if (np->next->t > t || np->t == t)
			return np;
	}

	return np;
}

/* return a pointer to the 'n'th object.
  * return nil if there is no 'n'th object.
  * return the last object if 'n' is 0 */
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

/* create a new anchor */
anchor *
mkanch(int x, int y)
{
	anchor *new;
	new = (anchor *) malloc(sizeof(anchor));

	if (new == nil)
		return nil;

	new->x = x;
	new->y = y;
	new->next = nil;

	return new;
}

/* add anchor to anchor list in position 'n'.
  * if i is 0, append anchor to the end of the list.
   * returns a pointer to the anchor list's head */
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
