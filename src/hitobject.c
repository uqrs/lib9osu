#include <u.h>
#include <libc.h>
#include <stdio.h>
#include "aux.h"
#include "hitsound.h"
#include "hitobject.h"

/* creates a new object */
hitobject *
mkobj(uchar type, double t, int x, int y)
{
	hitobject *new;
	anchor *ap;

	new = ecalloc(1, sizeof(hitobject));
	ap = mkanch(x, y);

	new->type = type;
	new->t = t;
	new->anchors = ap;
	new->next = nil;

	return new;
}

/* free an object along with its anchors; MUST call rmobj first if the object is in a list */
void
nukeobj(hitobject *op)
{
	anchor *ap, *next;

	if (op == nil)
		return;

	for (ap = op->anchors; ap != nil; ap = next) {
		next = ap->next;
		free(ap);
	}

	free(op->sladditions);
	free(op->slnormalsets);
	free(op->sladditionsets);
	nukehitsamp(op->hitsamp);

	free(op);
}

/* inserts an object into the list based on its time value.
  * returns a pointer to the list's head */
hitobject *
addobjt(hitobject *listp, hitobject *op)
{
	hitobject *np;

	if (op == nil)
		return nil;

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
	if (listp == nil || op == nil)
		return nil;

	listp = rmobj(listp, op);
	op->t = t;

	return addobjt(listp, op);
}

/* removes the hitobject pointed to by op from listp */
hitobject *
rmobj(hitobject *listp, hitobject *op)
{
	hitobject *np, *newlistp;

	if (listp == nil)
		return nil;

	if (listp == op) {
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
	int i;

	if (listp == nil || n < 0)
		return nil;

	if (n == 0) {
		for (np = listp; np->next != nil; np = np->next)
			;

		return np;
	}

	for (np = listp, i = 0; np != nil; np = np->next)
		if (++i == n)
			return np;

	return nil;
}

/* returns a pointer to the object in listp
  * corresponding to the timestamp in s.
  * writes the number of selected objects to
  * selected, if it is not nil.
  * sample input:
  * 00:00:880 (1,1,2,3,1,2,3,4,1)
  */
hitobject *
lookupobjstr(hitobject *listp, int *selected, char *s)
{
	hitobject *op;
	char *objstr, *p;
	int nmins, nsec, nms, t;

	if (listp == nil || s == nil)
		return nil;

	objstr = ecalloc(256, sizeof(char));
	nmins = nsec = nms = 0;

	if (sscanf(s, "%d:%d:%d %s", &nmins, &nsec, &nms, objstr) < 4)
		return nil;

	if (selected != nil)
		for (p = objstr; *p != '\0'; p++)
			if (*p == ',' || *p == ')')
				*selected += 1;

	t = nms + 1000*(nmins*60 + nsec);
	op = lookupobjt(listp, t);

	return (op->t >= t) ? op : op->next;
}

/* creates a new anchor */
anchor *
mkanch(int x, int y)
{
	anchor *new;

	new = ecalloc(1, sizeof(anchor));
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

	if (n < 0)
		return nil;

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

/* calculates the distance between points (x1,y1) and (x2,y2)
  * using Pythagoras' theorem */
float
hypotenuselen(float x1, float y1, float x2, float y2)
{
	return sqrt(pow(x2 - x1, 2) + pow(y2 - y1, 2));
}


/* interpolates a point on a n-order bezier curve denoted by alistp,
  * where 0 <= t <= 1.
  * writes the coordinates of this point to *x and *y.
  * returns 0 on success, negative values for failures.
  */
int
bezierpoint(anchor *alistp, int n, float *x, float *y, float t)
{
	float nx, ny;
	int k;
	anchor *ap;

	if (alistp == nil || x == nil || y == nil || t < 0 || t > 1)
		return -1;

	nx = ny = 0;

	ap = alistp;
	for (k = 0; k <= n; k++) {
		nx += ap->x * (fact(n) / (fact(k) * fact(n - k))) * pow(1 - t, n - k) * pow(t, k);
		ny += ap->y * (fact(n) / (fact(k) * fact(n - k))) * pow(1 - t, n - k) * pow(t, k);
		ap = ap->next;
	}

	*x = nx;
	*y = ny;

	return 0;
}

/* returns the length of the curve denoted by the first
  * nanchors control points in alistp
  *
  * returns the length of the curve in osu! pixels on success,
  * negative values on failure.
  */
float
bezierlen(anchor *alistp, int nanchors)
{
	static float step = 0.0125;
	float t, len, x1, y1, x2, y2;

	if (alistp == nil || nanchors < 1)
		return -1;

	len = 0;
	if (bezierpoint(alistp, nanchors - 1, &x1, &y1, 0) < 0)
		return -1;

	for (t = step; t < 1; t += step) {
		if (bezierpoint(alistp, nanchors - 1, &x2, &y2, t) < 0)
			return -1;

		len += hypotenuselen(x1, y1, x2, y2);

		x1 = x2;
		y1 = y2;
	}

	return len;
}