# Third-party notices and source boundary

The root [MIT license](LICENSE) covers original code and documentation in this
repository. Existing third-party notices retain their own scope.

## Dependencies

These libraries are installed separately; their source and binaries are not
vendored in this source distribution:

| Library | License | Upstream |
|---|---|---|
| raylib | zlib/libpng | [raylib license](https://github.com/raysan5/raylib/blob/master/LICENSE) |
| JSON for Modern C++ | MIT | [nlohmann/json license](https://github.com/nlohmann/json/blob/develop/LICENSE.MIT) |

If you distribute a binary with these dependencies, retain their applicable
notices as required by their licenses.

## Implementation references

`src/entities/boss.cpp` credits the Vile laugh-tail reference to
[UnityMegamanX](https://github.com/jfalcos/UnityMegamanX). Its MIT notice,
Copyright (c) 2020 Javier Falcon, is preserved verbatim in
[licenses/UnityMegamanX-MIT.txt](licenses/UnityMegamanX-MIT.txt).
No Unity project or its assets are bundled here.

## Materials outside this repository

Mega Man, Mega Man X, related characters and trademarks belong to their
respective owners, including Capcom. This independent project is not endorsed
by Capcom. The code license does not grant rights to those names, original
dialogue, ROMs, sprites, music or other game assets.

No game content pack, extracted artwork/audio, ROM, emulator state, captured
rendering table or research archive is distributed here. Original art/audio
contributions need a stated author and compatible license before inclusion.
See [reference-data.md](docs/reference-data.md) for the build boundary.
