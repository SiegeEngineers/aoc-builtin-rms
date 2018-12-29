# aoc-builtin-rms

A little mod for adding new "builtin"-like RMS scripts to Age of Empires 2: The Conquerors, with UserPatch.

This mod works together with UserPatch 1.5.

## Usage

Load this .dll into your AoC process somehow. You can use an exe mod or hijack your `language_x1_p1.dll` for this (put your strings and a `LoadLibrary()` call in it).

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
    <map id="-1" name="Cenotes" drsId="54250" />
  </random-maps>
</configuration>
```

Each map has an `id`, a `name`, a `drsId`.

The `id` will be used internally by the game to refer to the map. It should never change. Some IDs (0-50 more or less) are already in use by the game for the standard builtin maps. I recommend using small negative numbers to reduce the risk of clashes.

The `name` determines how the map name is displayed and what the RMS and AI constants are named. For example, `name="Cenotes"` allows using the `(map-type cenotes)` fact and doing `#load-if-defined CENOTES-MAP` in AIs.

The `drsId` is the `bina` resource ID that contains the map script.

Optionally, `description` can refer to a string ID that will be shown when hovering the map name in the game setup screen. This string must be present in a `language_x1_p1.dll` file with your mod.
```xml
<map id="-10" name="Valley" drsId="54260" description="30155" />
```
