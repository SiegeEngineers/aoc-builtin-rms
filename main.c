#include <windows.h>
#include <stdio.h>
#include "ezxml.h"
#include "aoc-builtin-rms.h"

#define TERRAIN_TEXTURE_BASE 15000
#define TERRAIN_TEXTURE_MAX 15050

#ifdef DEBUG
#  define debug(...) printf(__VA_ARGS__)
#else
#  define debug(...)
#endif

/**
 * Location of the directory name of the active UP mod.
 */
static const size_t offs_mod_name = 0x7A5058;
static const size_t offs_mod_dir = 0x7A506C;

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

static int parse_map_terrain_overrides(const char* source, terrain_overrides_t* out) {
  const char* read_ptr = source;
  const char* end_ptr = strchr(source, '\0');

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
 * Parse a <map /> XML element.
 */
static int parse_map(ezxml_t node) {
  custom_map_t map = {
    .id = 0,
    .name = NULL,
    .string = -1,
    .description = -1,
    .ai_const_name = NULL,
    .ai_symbol_name = NULL,
    .type = Standard,
    .scx_drs_id = -1
  };
  for (int i = 0; i < 50; i++) {
    map.terrains.terrains[i] = -1;
  }

  const char* id = ezxml_attr(node, "id");
  if (id == NULL) { return ERR_NO_ID; }
  map.id = atoi(id);
  if (map.id < 0) {
    map.id = 255 + (char)map.id;
  }
  if (map.id > 255) { return ERR_TOO_BIG_ID; }

  const char* name = ezxml_attr(node, "name");
  if (name == NULL || strlen(name) < 1) { return ERR_NO_NAME; }
  map.name = calloc(1, strlen(name) + 1);
  strcpy(map.name, name);

  const char* string = ezxml_attr(node, "string");
  if (string == NULL) { return ERR_NO_STRING_ID; }
  map.string = atoi(string);

  const char* drs_id = ezxml_attr(node, "drsId");
  if (drs_id == NULL) { return ERR_NO_DRS_ID; }
  map.drs_id = atoi(drs_id);

  const char* scx_drs_id = ezxml_attr(node, "scxDrsId");
  if (scx_drs_id != NULL) {
    map.scx_drs_id = atoi(scx_drs_id);
  }

  if (map.scx_drs_id > 0) {
    map.type = RealWorld;
  }

  const char* description = ezxml_attr(node, "description");
  if (description != NULL) {
    map.description = atoi(description);
  }

  const char* override = ezxml_attr(node, "terrainOverrides");
  if (override != NULL) {
    int result = parse_map_terrain_overrides(override, &map.terrains);
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
  int symbol_len = sprintf(symbol_name, map.type == RealWorld ? "REAL-WORLD-%s-MAP" : "%s-MAP", name);
  CharUpperBuffA(symbol_name, symbol_len);
  map.ai_symbol_name = calloc(1, symbol_len + 1);
  strcpy(map.ai_symbol_name, symbol_name);

  printf("[aoc-builtin-rms] Add modded map: %s\n", map.name);
  size_t i = count_custom_maps();
  custom_maps[i] = map;
  custom_maps[i + 1] = (custom_map_t) {0};
  return 0;
}

/**
 * Parse the maps out of an aoc-builtin-rms.xml file.
 */
static void parse_maps(char* mod_config) {
  char err_message[256];
  ezxml_t document = ezxml_parse_str(mod_config, strlen(mod_config));
  if (!document) {
    MessageBoxA(NULL, "Could not parse aoc-builtin-rms.xml, please check its syntax", NULL, 0);
    return;
  }

  const char* root_name = ezxml_name(document);
  if (root_name == NULL || strncmp(root_name, "random-maps", 11) != 0) {
    sprintf(err_message, "The top-level element must be <random-maps>, got <%s>", root_name);
    MessageBoxA(NULL, err_message, NULL, 0);
    ezxml_free(document);
    return;
  }

  for (ezxml_t map = ezxml_child(document, "map"); map; map = ezxml_next(map)) {
    int err = parse_map(map);
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
  ezxml_free(document);
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

static void init() {
  debug("[aoc-builtin-rms] init()\n");
  char* mod_name = *(char**) offs_mod_name;
  debug("[aoc-builtin-rms] mod name: %s\n", mod_name);
  if (mod_name == NULL) return;

  char* mod_path = *(char**) offs_mod_dir;
  debug("[aoc-builtin-rms] mod path: %s\n", mod_path);
  if (mod_path == NULL) return;

  char xml_path[MAX_PATH];
  // mod_path includes trailing slash
  sprintf(xml_path, "%saoc-builtin-rms.xml", mod_path);

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
