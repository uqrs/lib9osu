/* hitsound-related data structures & manipulation functions */
enum sampsets {
	SAMPDEFAULT=0,
	SAMPNORMAL,
	SAMPSOFT,
	SAMPDRUM
} sampsets;

enum additionbits {
	ADBNORMAL= 0x1,
	ADBWHISTLE = 0x2,
	ADBFINISH = 0x4,
	ADBCLAP = 0x8,
} additionbits;

typedef struct hitsample hitsample;
typedef struct hitsample {
	int normal;		/* sample set for the 'normal' sound */
	int addition;		/* sample set for whistle, finish and clap sounds */
	int index;			/* custom sample index */
	int volume;		/* sample volume percentage */
	Rune *file;			/* filename for custom addition sound; may be nil */
} hitsample;

hitsample *mkhitsample(int normal, int addition, int index, int volume, Rune *file);
void nukehitsample(hitsample *hsp);
