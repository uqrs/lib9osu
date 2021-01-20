# osufs
osu! beatmap 9p server

Built with p9p. Has not been tested on an actual plan9 machine.
```
mk
./osu9 example/destrier.osu
diff outp.osu example/destrier.osu
```

Inexperienced C programmer; caveat emptor.

## What has been done?
- Hitobject & timingpoint data structures
- osu! beatmap file parsing infrastructure
  - Hitobject parsing
  - Red/greenline parsing
  - Beatmap metadata parsing
  - Song metadata parsing (including proper UTF-8 handling via plan9's Rune facilities)
- Beatmap serialisation
  
## What has yet to be done?
- Additional functions for traversing the lists & manipulating object/timing point data
- Functions for calculating visual slider length
- The actual 9p server itself.
