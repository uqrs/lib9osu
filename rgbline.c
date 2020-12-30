#include <u.h>
#include <libc.h>
#include "rgbline.h"

/* create a new greenline */
gline *
mkgline(ulong t, double velocity, int volume, int kiai)
{
	gline *new = malloc(sizeof(gline));

	if(new == nil)
		return nil;

	new->t = t;
	new->velocity = velocity;
	new->volume = volume;
	new->kiai = kiai;
	new->next = nil;

	return new;
}

/* free a greenline; MUST call rmgline first if the greenline is in a list */
void
nukegline(gline *gline)
{
	free(gline);
}

/* insert a greenline into the list based on its time value.
  * returns a pointer to the list's head */
gline *
addglinet(gline *glistp, gline *glp)
{
	gline *np;

	if (glistp == nil)
		return glp;

	if (glp->t <= glistp->t) {
		glp->next = glistp;
		return glp;
	}

	for (np = glistp; np->next != nil; np = np->next) {
		if (glp->t > np->t && glp->t <= np->next->t) {
			glp->next = np->next;
			np->next = glp;
			return glistp;
		}
	}

	np->next = glp;
	return glistp;
}

/* set greenline's time to 't' and adjust its position in the list */
gline *
movegline(gline *glistp, gline *glp, ulong t)
{
	glistp = rmgline(glistp, glp);
	glp->t = t;
	return addglinet(glistp, glp);
}

/* remove greenline 'glp' from 'glistp' */
gline *
rmgline(gline *glistp, gline *glp)
{
	gline *np, *newlistp;

	if (glistp == nil) {
		return nil;
	} else if (glistp == glp) {
		newlistp = glistp->next;
		glistp->next = nil;
		return newlistp;
	}

	for (np = glistp; np->next != nil; np = np->next) {
		if (np->next == glp) {
			np->next = glp->next;
			glp->next = nil;
			return glistp;
		}
	}

	return nil; /* unreachable */
}

/* create a new redline */
rline *
mkrline(ulong t, ulong duration, int beats, int volume, int kiai)
{
	rline *new = malloc(sizeof(rline));

	if (new == nil)
		return nil;

	new->t = t;
	new->duration = duration;
	new->beats = beats;
	new->volume = volume;
	new->kiai = kiai;
	new->next = nil;

	return new;
}

/* free a redline; MUST call rmrline first if the redline is in a list */
void
nukerline(rline *rlp)
{
	free(rlp);
}

/* insert a redline into the list based on its time value.
  * returns a pointer to the list's head */
rline *
addrlinet(rline *rlistp, rline *rlp)
{
	rline *np;

	if (rlistp == nil)
		return rlp;

	if (rlp->t <= rlistp->t) {
		rlp->next = rlistp;
		return rlp;
	}

	for (np = rlistp; np->next != nil; np = np->next) {
		if (rlp->t > np->t && rlp->t <= np->next->t) {
			rlp->next = np->next;
			np->next = rlp;
			return rlistp;
		}
	}

	np->next = rlp;
	return rlistp;
}

/* set redline's time to 't' and adjust its position in the list */
rline *
moverline(rline *rlistp, rline *rlp, ulong t)
{
	rlistp = rmrline(rlistp, rlp);
	rlp->t = t;
	return addrlinet(rlistp, rlp);
}

/* remove redline 'rlp' from 'rlistp' */
rline *
rmrline(rline *rlistp, rline *rlp)
{
	rline *np, *newlistp;

	if (rlistp == nil) {
		return nil;
	} else if (rlistp == rlp) {
		newlistp = rlistp->next;
		rlistp->next = nil;
		return newlistp;
	}

	for (np = rlistp; np->next != nil; np = np->next) {
		if (np->next == rlp) {
			np->next = rlp->next;
			rlp->next = nil;
			return rlistp;
		}
	}

	return nil; /* unreachable */
}

