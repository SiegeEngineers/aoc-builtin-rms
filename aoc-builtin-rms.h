#ifndef _AOC_BUILTIN_RMS_H
#define _AOC_BUILTIN_RMS_H

typedef struct terrain_overrides {
  int terrains[50];
} terrain_overrides_t;

typedef struct custom_map {
  /* Internal ID of the map--stored as an int but many places in game assume it's a single byte
   * so this should not exceed 255 */
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
  char type;
  /* DRS ID of the scenario if a real world map (not yet supported) */
  int scx_drs_id;
  /* Custom terrain texture IDs */
  terrain_overrides_t terrains;
} custom_map_t;

void aoc_builtin_rms_init(custom_map_t*, size_t);
void aoc_builtin_rms_deinit();

#endif