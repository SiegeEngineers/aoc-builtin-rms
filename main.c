#include <windows.h>
#include <stdio.h>
#include "aoc-builtin-rms.h"

#define RMS_STANDARD 0
#define RMS_REALWORLD 1

#define TERRAIN_TEXTURE_BASE 15000
#define TERRAIN_TEXTURE_MAX 15050

#ifdef DEBUG
#  define debug(...) printf(__VA_ARGS__)
#else
#  define debug(...)
#endif

/* location of the short name of the active UP mod,
 * as specified in GAME= command line parameter or hardcoded
 * in the mod exe */
static const size_t offs_game_name = 0x7A5058;

static custom_map_t custom_maps[100] = {
  { 0 }
};

/**
 * Count the number of custom maps.
 */
static size_t count_custom_maps() {
  size_t i = 0;
  for (; custom_maps[i].id; i++) {}
  return i;
}

#define ERR_NO_ID 1
#define ERR_NO_NAME 2
#define ERR_NO_STRING_ID 4
#define ERR_NO_DRS_ID 3
#define ERR_TOO_BIG_ID 5
#define ERR_TOO_BIG_TERRAIN 6

static int parse_map_terrain_overrides(char* source, terrain_overrides_t* out) {
  char* read_ptr = source;
  char* end_ptr = strchr(source, '\0');

  do {
    int orig = 0;
    int replace = 0;
    sscanf(read_ptr, "%d=%d", &orig, &replace);
    if (orig > 0 && replace > 0) {
      if (orig < TERRAIN_TEXTURE_BASE || orig >= TERRAIN_TEXTURE_MAX) {
        return ERR_TOO_BIG_TERRAIN;
      }
      debug("[aoc-builtin-rms] found terrain override: %d -> %d\n", orig, replace);
      out->terrains[orig - TERRAIN_TEXTURE_BASE] = replace;
    }

    read_ptr = strchr(read_ptr, ',');
    if (read_ptr != NULL) read_ptr++;
  } while (read_ptr && read_ptr < end_ptr);
  return 0;
}

/**
 * Parse a <map /> XML element from the string pointed to by `read_ptr_ptr`.
 * The pointer in `read_ptr_ptr` will point past the end of the <map/> element
 * after this function.
 */
static int parse_map(char** read_ptr_ptr) {
  custom_map_t map = {
    .id = 0,
    .name = NULL,
    .string = -1,
    .description = -1,
    .ai_const_name = NULL,
    .ai_symbol_name = NULL,
    .type = RMS_STANDARD,
    .scx_drs_id = -1
  };
  for (int i = 0; i < 50; i++) {
    map.terrains.terrains[i] = -1;
  }

  char* read_ptr = *read_ptr_ptr;
  char* end_ptr = strstr(read_ptr, "/>");
  *read_ptr_ptr = end_ptr + 2;

  char* id_ptr = strstr(read_ptr, "id=\"");
  if (id_ptr == NULL || id_ptr > end_ptr) { return ERR_NO_ID; }
  sscanf(id_ptr, "id=\"%d\"", &map.id);
  if (map.id < 0) {
    map.id = 255 + (char)map.id;
  }
  if (map.id > 255) { return ERR_TOO_BIG_ID; }

  char* name_ptr = strstr(read_ptr, "name=\"");
  if (name_ptr == NULL || name_ptr > end_ptr) { return ERR_NO_NAME; }
  char name[80];
  sscanf(name_ptr, "name=\"%79[^\"]\"", name);
  if (strlen(name) < 1) { return ERR_NO_NAME; }
  map.name = calloc(1, strlen(name) + 1);
  strcpy(map.name, name);

  char* string_ptr = strstr(read_ptr, "string=\"");
  if (string_ptr == NULL || string_ptr > end_ptr) { return ERR_NO_STRING_ID; }
  sscanf(string_ptr, "string=\"%d\"", &map.string);

  char* drs_id_ptr = strstr(read_ptr, "drsId=\"");
  if (drs_id_ptr == NULL || drs_id_ptr > end_ptr) { return ERR_NO_DRS_ID; }
  sscanf(drs_id_ptr, "drsId=\"%d\"", &map.drs_id);

  char* scx_drs_id_ptr = strstr(read_ptr, "scxDrsId=\"");
  if (scx_drs_id_ptr != NULL && scx_drs_id_ptr < end_ptr) {
    sscanf(scx_drs_id_ptr, "scxDrsId=\"%d\"", &map.scx_drs_id);
  }

  if (map.scx_drs_id > 0) {
    map.type = RMS_REALWORLD;
  }

  char* description_ptr = strstr(read_ptr, "description=\"");
  if (description_ptr != NULL && description_ptr < end_ptr) {
    sscanf(description_ptr, "description=\"%d\"", &map.description);
  }

  char* override_ptr = strstr(read_ptr, "terrainOverrides=\"");
  char override_str[501];
  if (override_ptr != NULL && override_ptr < end_ptr) {
    sscanf(override_ptr, "terrainOverrides=\"%500[^\"]\"", override_str);
    int result = parse_map_terrain_overrides(override_str, &map.terrains);
    if (result != 0) {
      return result;
    }
  }

  // Derive ai const name: lowercase name
  char const_name[80];
  int const_len = sprintf(const_name, "%s", name);
  CharLowerBuffA(const_name, const_len);
  map.ai_const_name = calloc(1, const_len + 1);
  strcpy(map.ai_const_name, const_name);

  // Derive ai symbol name: uppercase name suffixed with -MAP,
  // with REAL-WORLD- prefix for real world maps
  char symbol_name[100];
  int symbol_len = sprintf(symbol_name, map.type == RMS_REALWORLD ? "REAL-WORLD-%s-MAP" : "%s-MAP", name);
  CharUpperBuffA(symbol_name, symbol_len);
  map.ai_symbol_name = calloc(1, symbol_len + 1);
  strcpy(map.ai_symbol_name, symbol_name);

  printf("[aoc-builtin-rms] Add modded map: %s\n", map.name);
  size_t i = count_custom_maps();
  custom_maps[i] = map;
  custom_maps[i + 1] = (custom_map_t) {0};

  *read_ptr_ptr = end_ptr + 2;
  return 0;
}

/**
 * Parse a <random-maps> section from a UserPatch mod description file.
 */
static void parse_maps(char* mod_config) {
  char* read_ptr = strstr(mod_config, "<random-maps>");
  char* end_ptr = strstr(read_ptr, "</random-maps>");
  debug("[aoc-builtin-rms] range: %p %p\n", read_ptr, end_ptr);

  if (read_ptr == NULL || end_ptr == NULL) return;

  while ((read_ptr = strstr(read_ptr, "<map")) && read_ptr < end_ptr) {
    int err = parse_map(&read_ptr);
    switch (err) {
      case ERR_NO_ID: MessageBoxA(NULL, "A <map /> is missing an id attribute", NULL, 0); break;
      case ERR_NO_NAME: MessageBoxA(NULL, "A <map /> is missing a name attribute", NULL, 0); break;
      case ERR_NO_STRING_ID: MessageBoxA(NULL, "A <map /> is missing a string attribute", NULL, 0); break;
      case ERR_NO_DRS_ID: MessageBoxA(NULL, "A <map /> is missing a drsId attribute", NULL, 0); break;
      case ERR_TOO_BIG_ID: MessageBoxA(NULL, "A <map />'s map ID is too large: must be at most 255", NULL, 0); break;
      case ERR_TOO_BIG_TERRAIN: MessageBoxA(NULL, "A <map /> has an incorrect terrainOverrides attribute. Only terrains 15000-15050 can be overridden", NULL, 0); break;
      default: break;
    }
  }
}

char* read_file(char* filename) {
  FILE* file = fopen(filename, "rb");
  if (file == NULL) return NULL;
  fseek(file, 0, SEEK_END);
  int size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char* config = calloc(1, size + 1);
  config[size] = '\0';
  int read = fread(config, 1, size, file);
  fclose(file);

  printf("%d != %d\n", size, read);
  if (size != read) {
    free(config);
    return NULL;
  }

  return config;
}

char* get_mod_path(char* short_name) {
  char mod_xml_name[MAX_PATH];
  if (sprintf(mod_xml_name, "Games\\%s.xml", short_name) < 0) return NULL;

  char* config = read_file(mod_xml_name);
  if (config == NULL) return NULL;

  char* path_element = strstr(config, "<path");
  char long_name[MAX_PATH];
  sscanf(path_element, "<path>%250[^<]</path>", long_name);

  char* mod_path = calloc(1, MAX_PATH);
  if (mod_path == NULL) return NULL;

  if (sprintf(mod_path, "Games\\%s", long_name) < 0) {
    free(mod_path);
    return NULL;
  }

  return mod_path;
}

static void init() {
  debug("[aoc-builtin-rms] init()\n");
  char* short_game_name = *(char**) offs_game_name;
  debug("[aoc-builtin-rms] mod name: %s\n", short_game_name);
  if (short_game_name == NULL) return;

  char* game_path = get_mod_path(short_game_name);
  debug("[aoc-builtin-rms] mod path: %s\n", game_path);
  if (game_path == NULL) return;

  char xml_path[MAX_PATH];
  sprintf(xml_path, "%s\\aoc-builtin-rms.xml", game_path);
  free(game_path);

  debug("[aoc-builtin-rms] config path: %s\n", xml_path);
  char* config = read_file(xml_path);
  if (config == NULL) return;

  parse_maps(config);
  free(config);

  if (count_custom_maps() == 0) {
    printf("[aoc-builtin-rms] No custom maps specified, stopping.\n");
    return;
  }
  printf("[aoc-builtin-rms] Found custom maps, installing hooks.\n");

  aoc_builtin_rms_init(custom_maps, count_custom_maps());
}

static void deinit() {
  debug("[aoc-builtin-rms] deinit()\n");
  for (size_t i = 0; custom_maps[i].id; i++) {
    if (custom_maps[i].name != NULL)
      free(custom_maps[i].name);
    if (custom_maps[i].ai_const_name != NULL)
      free(custom_maps[i].ai_const_name);
    if (custom_maps[i].ai_symbol_name != NULL)
      free(custom_maps[i].ai_symbol_name);
    custom_maps[i].id = 0;
  }
}

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, void* _) {
  debug("[aoc-builtin-rms] DllMain %ld\n", reason);
  switch (reason) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(dll);
      init();
      break;
    case DLL_PROCESS_DETACH: deinit(); break;
  }
  return 1;
}
