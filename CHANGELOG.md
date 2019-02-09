# aoc-builtin-rms

All notable changes to this project will be documented in this file.

This project adheres to [Semantic Versioning](http://semver.org/).

## 0.5.0
* Refactor to use hooking library `hook.c`.
* Fix an issue where aoc-builtin-rms could look for a config file in the wrong mod directory.

## 0.4.0
* Add API-only module, without the automatic XML loading.
* Move config to an `aoc-builtin-rms.xml` config file to avoid polluting the UserPatch file with non-UP config.

## 0.3.0
* Reduce logging in the release DLL.
* Use `%[^"]` instead of `%s` for reading strings from XML attributes, behaves less strangely when whitespace is used.
* Support terrain overrides, like ZR@ maps.

## 0.2.0
* Relicense as LGPL-3.0 to make it easier to adopt in existing mods.

## 0.1.1
* Fix map ID byte sizes, for transfer in the multiplayer game lobby.

## 0.1.0
* Initial release.
