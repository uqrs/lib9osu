#include <u.h>
#include <libc.h>
#include "aux.h"
#include "hitsound.h"

hitsamp *
mkhitsamp(int normal, int addition, int index, int volume, Rune *file)
{
	hitsamp *new;

	if (normal < SAMPDEFAULT || normal > SAMPDRUM || addition < SAMPDEFAULT || addition > SAMPDRUM)
		return nil;

	new = ecalloc(1, sizeof(hitsamp));

	new->normal = normal;
	new->addition = addition;
	new->index = index;
	new->volume = volume;
	new->file = file;

	return new;
}

void
nukehitsamp(hitsamp *hsp)
{
	if (hsp == nil)
		return;

	free(hsp->file);
	free(hsp);
}
