#include <u.h>
#include <libc.h>
#include "aux.h"
#include "hitsound.h"

hitsample *
mkhitsample(int normal, int addition, int index, int volume, Rune *file)
{
	hitsample *new;

	if (normal < SAMPDEFAULT || normal > SAMPDRUM || addition < SAMPDEFAULT || addition > SAMPDRUM)
		return nil;

	new = ecalloc(1, sizeof(hitsample));

	new->normal = normal;
	new->addition = addition;
	new->index = index;
	new->volume = volume;
	new->file = file;

	return new;
}

void
nukehitsample(hitsample *hsp)
{
	if (hsp == nil)
		return;

	free(hsp->file);
	free(hsp);
}
