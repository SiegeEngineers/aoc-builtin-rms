#ifndef _AOC_BUILTIN_RMS_H
#define _AOC_BUILTIN_RMS_H
#include <stddef.h>

#define AOC_BUILTIN_RMS_VERSION "0.6.3"

typedef struct TerrainOverrides {
  int terrains[50];
} TerrainOverrides;

typedef enum CustomMapType {
  Standard = 0,
  RealWorld = 1,
  Custom = 2,
  CustomSection = 4
} CustomMapType;

typedef struct CustomMap {
  /* Internal ID of the map--stored as an int but many places in game assume
   * it's a single byte so this should not exceed 255 */
  int id;
  /* Internal name of the map */
  char* name;
  /* String ID containing the localised name of the map. */
  int string;
  /* Hover description of the map */
  int description;
  /* Constant name for this map for use in AI rules */
  char* ai_const_name;
  /* Symbol name for this map for use in AI preprocessing statements */
  char* ai_symbol_name;
  /* DRS ID of the map */
  int drs_id;
  /* Kind of map (standard/real world) */
  CustomMapType type;
  /* DRS ID of the scenario if a real world map (not yet supported) */
  int scx_drs_id;
  /* Custom terrain texture IDs */
  TerrainOverrides terrains;
} CustomMap;

typedef struct MapSection {
  /* String ID containing the localised name of the map style. */
  int name;
  /* ID of the default map selection for this style. */
  int default_map;
  /* Prefix for AI symbols for this style. For example, "SPECIAL-MAP" defines
   * "SPECIAL-MAP-SNAKE-PIT". */
  char* ai_symbol_prefix;
  /* Prefix for AI constants for this style. For example, "special-map" defines
   * "special-map-snake-pit". */
  char* ai_const_prefix;
} MapSection;

void aoc_builtin_rms_init(MapSection*, size_t, CustomMap*, size_t);
void aoc_builtin_rms_deinit();

#endif
