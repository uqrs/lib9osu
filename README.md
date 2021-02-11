# lib9osu
Built with p9p. Has not been tested on an actual plan9 machine.
A small sample of test cases has been provided in example/
```
mk
./osu9 example/destrier.osu
diff outp.osu example/destrier.osu
```
'test' is fickle and hacky. Use at your own risk.
```
./test example/
```
Test cases can be found at: https://data.ppy.sh/
This revision has been tested against:  
- 2021_01_01_osu_files.tar.bz2 (only osu!std maps; i.e. any map where "Mode" is "0")

verify.awk verifies the semantic contents of beatmaps. It does not catch:
- red/green lines with identical timestamps being written back in a different order
- 2B being written back in a different order
- shitposters hiding easter eggs in their mapsets
These maps will require manual intervention, at least until verify.awk has been modified to account for these.

## What has been done?
- Hitobject & timingpoint data structures
- osu! beatmap file parsing (readmap() in beatmap.c)
- Beatmap serialisation (writemap() in beatmap.c)
  
## What has yet to be done?
- Additional functions for traversing the lists & manipulating object/timing point data
- Functions for calculating visual slider length

## Quirks
- osu!mania, osu!taiko, and osu!catch are not supported.
- storyboarding is not supported: the '[Events]' section is simply loaded in as a string.
- for timing point "conflicts", osufs will only guarantee that no greenline will precede a redline with the same timestamp in the list.
