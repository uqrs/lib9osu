#include <u.h>
#include <libc.h>
#include "hitsound.h"
#include "rgbline.h"
#include "hitobject.h"
/* all timeline-related functions round down to a whole integer
  * to stay consistent with the official osu! editor */

/* calculate the duration in ms of n number of 1/divisor beats, using
  * duration as the length of a beat in ms.
  * e.g. t + nexttick(... 4, 3) yields a timestamp 3 quarter-beats
  * (sixteenth notes) after t.
  * 'n' may be negative to seek backwards.
  * returns negative values on bad arguments. */
double
ticklen(double duration, int divisor, int n)
{
	if (divisor < 1)
		return -1;

	return duration/divisor * n;
}

/* calculate the duration in ms of a single journey across the body
  * of a slider of visual length length
  * a velocity of -100 translates to 1x speed.
  * returns negative values on bad arguments.
  */
double
sllen(double length, double duration, double velocity, double slmultiplier)
{
	if (length < 0 || duration <= 0 || slmultiplier <= 0)
		return -1;

	return round(length / (slmultiplier * 100) * duration);
}
