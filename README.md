# osufs
osu! beatmap 9p server

Built with p9p. Has not been tested on an actual plan9 machine.
```
mk
./osu9 example/destrier.osu
```

Inexperienced C programmer; caveat emptor.

## What has been done?
- Hitobject & timingpoint data structures
- osu! beatmap file parsing infrastructure
  - Hitobject parsing (except hitsounds)
  - Red/greenline parsing (except hitsounds)
  - Beatmap metadata parsing
  - Song metadata parsing (including proper UTF-8 handling via plan9's Rune facilities)
  
## What has yet to be done?
- Hitsounding data structures
- Hitsounding parsing
- Parsing of remaining sections (\[Events\] & \[Difficulty\])
- Serialisation of data back into file
- Additional functions for traversing the lists & manipulating object/timing point data
- The actual 9p server itself.
