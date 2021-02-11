#include <u.h>
#include <libc.h>
#include "aux.h"
#include "rgbline.h"

/* create a new line */
rgline *
mkrgline(double t, double vord, int beats, int type)
{
	rgline *new;

	if (type < GLINE || type > RLINE)
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

/* inserts a line into listp based on its time value.
  * in the case of timestamp "conflicts", addrgbline
  * ensures green lines will not precede redlines in
  * the list's order,
  *
  * returns a pointer to the list's head
  * returns nil if lp is nil, or lp->type is invalid */
rgline *
addrglinet(rgline *listp, rgline *lp)
{
	rgline *np;

	if (lp == nil || lp->type < GLINE || lp->type > RLINE)
		return nil;

	if (listp == nil)
		return lp;

	if (lp->t < listp->t || (lp->type == RLINE && lp->t <= listp->t)) {
		lp->next = listp;
		return lp;
	}

	for (np = listp; np != nil; np = np->next) {
		if (lp->t == np->t) {
			if (lp->type == GLINE)
				for (; np->next != nil && np->next->t == lp->t; np = np->next)
					;
			else if (lp->type == RLINE)
				for (; np->next != nil && np->next->t == lp->t && np->next->type == RLINE; np = np->next)
					;
			lp->next = np->next;
			np->next = lp;
			return listp;
		} else if (np->next == nil) {
			np->next = lp;
			return listp;
		} else if (lp->t > np->t && lp->t < np->next->t) {
			lp->next = np->next;
			np->next = lp;
			return listp;
		}
	}

	return nil; /* unreachable */
}

/* change line lp's time to t and adjust its position in listp */
rgline *
movergline(rgline *listp, rgline *lp, double t)
{
	listp = rmrgline(listp, lp);
	lp->t = t;
	return addrglinet(listp, lp);
}

/* remove the line pointed to by lp from listp */
rgline *
rmrgline(rgline *listp, rgline *lp)
{
	rgline *np, *newlistp;

	if (listp == lp) {
		newlistp = listp->next;
		listp->next = nil;
		return newlistp;
	}

	for (np = listp; np->next != nil; np = np->next) {
		if (np->next == lp) {
			np->next = lp->next;
			lp->next = nil;
			return listp;
		}
	}

	return nil; /* unreachable */
}
