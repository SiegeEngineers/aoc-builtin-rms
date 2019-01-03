# aoc-builtin-rms

A little mod for adding new "builtin"-like RMS scripts to Age of Empires 2: The Conquerors, with UserPatch.

This mod works together with UserPatch 1.5.

|||
|-|-|
| ![Custom Standard maps](./screenshot-standard.png) | ![Custom Real World maps](./screenshot-realworld.png) |

## Usage

Grab `aoc-builtin-rms.dll` from the [releases](https://github.com/SiegeEngineers/aoc-builtin-rms/releases) page. Load this .dll into your AoC process somehow. You can use an exe mod or hijack your `language_x1_p1.dll` for this (put your strings and a `LoadLibrary()` call in it).

Put your mod's random maps in a .drs archive as `bina` files. The IDs don't really matter so long as they don't clash. Using the 54240 ~ 54299 range is pretty safe. Note that random maps in `.drs` files do **not** automatically load `random_map.def`. Include this at the top of your scripts:

```
#include_drs random_map.def 54000
```

This dll looks for a `<random-maps>` section in your mod's XML file, the same that specifies civilizations and your mod directory.

```xml
<configuration ...>
  <name>...</name>
  <path>...</path>
  <civilizations>
    ...
  </civilizations>
  <random-maps>
    <map id="-1" name="cenotes" string="10916" drsId="54250" />
  </random-maps>
</configuration>
```

Each map must have an `id`, a `name`, `string`, and a `drsId`.

The `id` will be used internally by the game to refer to the map. It should never change. Some IDs (0-50 more or less) are already in use by the game for the standard builtin maps. I recommend using small negative numbers to reduce the risk of clashes.

The `name` determines what the RMS and AI constants for this map are named. For example, `name="cenotes"` allows using the `(map-type cenotes)` fact and doing `#load-if-defined CENOTES-MAP` in AIs. Do **not** use spaces here, instead separate words with dashes, eg `name="city-of-lakes"`.

The `string` is the string ID containing the localised map name, as shown in the random map selection dropdown menu. This string must be present in a `language_x1_p1.dll` file with your mod.

The `drsId` is the `bina` resource ID that contains the map script.

Optionally, `scxDrsId` can refer to a `bina` resource ID containing a scenario file for Real World maps. Specifying `scxDrsId` will cause the map to show in the Real World section instead of the RMS section.

Optionally, `description` can refer to a string ID that will be shown when hovering the map name in the game setup screen. This string must be present in a `language_x1_p1.dll` file with your mod.
```xml
<map id="-10" name="valley" drsId="54260" string="10923" description="30155" />
```

Optionally, `terrainOverrides` can be used to override terrain SLPs, similarly to how ZR@ maps allow you to do for custom random maps in UserPatch 1.5. `terrainOverrides` is a comma-separated strings of SLP id replacements:
```xml
<map terrainOverrides="15018=15033,15014=15012" />
```
The left-hand side is the original SLP ID, where the right-hand side is the new SLP ID. In this case, the road SLP (15018) is replaced with a custom SLP 15033, and the shallows SLP (15014) is replaced with a custom SLP 15012. This is similar to placing the graphic for the 15033 slp in your ZR@ map as `15018.slp`.
These replacement SLPs must be present in the `slp ` resource table of a DRS file with your mod.

## Build

This project can only be built with MinGW GCC compilers at this time.

To create a debug build (with some logging):

```
make
```

To create a release build (smaller and no logging):

```
make clean
env RELEASE=1 make
```

## License

[LGPL-3.0](./LICENSE.md)
