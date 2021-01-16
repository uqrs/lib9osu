#include <u.h>
#include <libc.h>
/* calloc. abort on error. */
void *
ecalloc(int n, int size)
{
	void *new;
	new = calloc(n, size);
	if (new == nil)
		sysfatal("out of memory\n");
	return new;
}

/* realloc. abort on error. */
void *
erealloc(void *p, int n)
{
	void *new;
	new = realloc(p, n);
	if (new == nil)
		sysfatal("out of memory\n");
	return new;		
}

/* convert the string pointed to by s into runes, up to the NULL character.
  * returns a pointer to a section of memory containing
  * the converted string.
  *
  * returns nil when out of memory.
  *
  * returns a Rune string containing only Runeerror if the NULL character
  * occurs in the middle of a UTF sequence. This  sets the error string,
  * and the return value must still be free()d afterwards.
  */
Rune *
strrunedup(char *s)
{
	int nrune = 0;
	int maxrune = 16;
	int i, j;
	Rune *out;

	out = ecalloc(maxrune, sizeof(Rune));

	for (i = 0; s[i] != '\0'; i++) {
		if (nrune+1 > maxrune) {
			maxrune *= 2;
			out = erealloc(out, sizeof(Rune) * maxrune);
		}

		if ((uchar)s[i] < Runeself) {
			out[nrune++] = (Rune)s[i];
			continue;
		}
		char buf[UTFmax];
		buf[0] = s[i++];

		for (j = 1;;i++) {
			if (s[i] == '\0') {
				out[0] = Runeerror;
				out[1] = '\0';
				werrstr("unexpected end of string at position %d", i);

				return out;
			}
			buf[j++] = s[i];
			if (fullrune(buf, j)) {
				chartorune(out+nrune, buf);
				nrune++;
				break;
			}
		}
	}
	out[nrune] = (Rune)'\0';

	return out;
}
