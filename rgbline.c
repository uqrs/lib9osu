#include <u.h>
#include <libc.h>
#include "aux.h"
#include "rgbline.h"

/* create a new line */
rgline *
mkrgline(long t, double vord, int beats, int type)
{
	rgline *new;

	if (type < GLINE || type > RLINE || beats < 0)
		return nil;

	new = ecalloc(1, sizeof(rgline));
	new->t = t;
	new->beats = beats;

	new->type = type;
	if (new->type == RLINE)
		new->duration = vord;
	else if (new->type == GLINE)
		new->velocity = vord;

	new->next = nil;

	return new;
}

/* free a line; MUST call rmline first if the line is in a list */
void
nukergline(rgline *lp)
{
	free(lp);
}

/* inserts a line into llistp based on its time value.
  * in the case of timestamp "conflicts", addrgbline
  * ensures green lines will not precede redlines in
  * the list's order,
  *
  * returns a pointer to the list's head
  * returns nil if lp is nil, or lp->type is invalid */
rgline *
addrglinet(rgline *llistp, rgline *lp)
{
	rgline *np;

	if (lp == nil || lp->type < GLINE || lp->type > RLINE)
		return nil;
	if (llistp == nil)
		return lp;

	if (lp->type == GLINE) {
		if (lp->t < llistp->t) {
			lp->next = llistp;
			return lp;
		}

		for (np = llistp; np != nil; np = np->next) {
			if (lp->t == np->t) {
				for (; np->next != nil && np->next->t == lp->t; np = np->next)
					;
				lp->next = np->next;
				np->next = lp;
				return llistp;
			} else if (np->next == nil) {
				np->next = lp;
				return llistp;
			} else if (lp->t > np->t && lp->t < np->next->t) {
				lp->next = np->next;
				np->next = lp;
				return llistp;
			}
		}
	} else if (lp->type == RLINE) {
		if (lp->t <= llistp->t) {
			lp->next = llistp;
			return lp;
		}

		for (np = llistp; np != nil; np = np->next) {
			if (np->next == nil) {
				np->next = lp;
				return llistp;
			} else if (lp->t > np->t && lp->t <= np->next->t) {
				lp->next = np->next;
				np->next = lp;
				return llistp;
			}
		}
	}

	return nil; /* unreachable */
}

/* change line lp's time to t and adjust its position in llistp */
rgline *
movergline(rgline *llistp, rgline *lp, long t)
{
	llistp = rmrgline(llistp, lp);
	lp->t = t;
	return addrglinet(llistp, lp);
}

/* remove the line pointed to by lp from llistp */
rgline *
rmrgline(rgline *llistp, rgline *lp)
{
	rgline *np, *newlistp;

	if (llistp == nil) {
		return nil;
	} else if (llistp == lp) {
		newlistp = llistp->next;
		llistp->next = nil;
		return newlistp;
	}

	for (np = llistp; np->next != nil; np = np->next) {
		if (np->next == lp) {
			np->next = lp->next;
			lp->next = nil;
			return llistp;
		}
	}

	return nil; /* unreachable */
}
