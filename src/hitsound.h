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

typedef struct hitsamp hitsamp;
typedef struct hitsamp {
	int normal;		/* sample set for the 'normal' sound */
	int addition;		/* sample set for whistle, finish and clap sounds */
	int index;			/* custom sample index; negative values indicate that no index was selected */
	int volume;		/* sample volume percentage; negative values indicate that no volume was set */
	Rune *file;			/* filename for custom addition sound; nil value indicates that hitsample definition had no 'file' field. */
} hitsamp;

hitsamp *mkhitsamp(int normal, int addition, int index, int volume, Rune *file);
void nukehitsamp(hitsamp *hsp);
