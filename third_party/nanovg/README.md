# nanovg path subset

Source: https://github.com/memononen/nanovg, commit
ce3bf745eb2d2dbc14a50bf2446783f691ac4353 (2026-02-19), file `src/nanovg.c`.

Licence: zlib (LICENSE.txt). Only the path flattening, stroke expansion,
join/cap and fill expansion code was taken; the alterations are listed at the
top of `nanovg_path.c` and `nanovg_path.h`. arisa reaches it only through
`src/bridge/nanovg/`.
