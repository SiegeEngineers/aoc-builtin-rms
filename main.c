#include "aoc-builtin-rms.h"
#include <ezxml/ezxml.h>
#include <mmmod.h>
#include <stdio.h>
#include <windows.h>

#define TERRAIN_TEXTURE_BASE 15000
#define TERRAIN_TEXTURE_MAX 15050

#ifdef NDEBUG
#define dbg_print(...)
#else
#define dbg_print(...) printf("[aoc-builtin-rms] " __VA_ARGS__)
#endif

static MapSection custom_sections[100] = {{0}};
static CustomMap custom_maps[255] = {{0}};

/**
 * Count the number of custom sections.
 */
static size_t count_custom_sections() {
  size_t i = 0;
  for (; custom_sections[i].name; i++) {
  }
  return i;
}

/**
 * Count the number of custom maps.
 */
static size_t count_custom_maps() {
  size_t i = 0;
  for (; custom_maps[i].id; i++) {
  }
  return i;
}

typedef enum ParseMapResult {
  MapOk,
  NoId,
  NoName,
  NoStringId,
  NoDrsId,
  TooBigId,
  TooBigTerrain
} ParseMapResult;

static ParseMapResult
parse_map_terrain_overrides(const char* source, TerrainOverrides* out) {
  const char* read_ptr = source;
  const char* end_ptr = strchr(source, '\0');

  do {
    int orig = 0;
    int replace = 0;
    sscanf(read_ptr, "%d=%d", &orig, &replace);
    if (orig > 0 && replace > 0) {
      if (orig < TERRAIN_TEXTURE_BASE || orig >= TERRAIN_TEXTURE_MAX) {
        return TooBigTerrain;
      }
      dbg_print("found terrain override: %d -> %d\n", orig, replace);
      out->terrains[orig - TERRAIN_TEXTURE_BASE] = replace;
    }

    read_ptr = strchr(read_ptr, ',');
    if (read_ptr != NULL)
      read_ptr++;
  } while (read_ptr && read_ptr < end_ptr);
  return MapOk;
}

/**
 * Parse a <map /> XML element.
 */
static ParseMapResult parse_map(ezxml_t node, CustomMapType type) {
  CustomMap map = {.id = 0,
                   .name = NULL,
                   .string = -1,
                   .description = -1,
                   .ai_const_name = NULL,
                   .ai_symbol_name = NULL,
                   .type = type,
                   .scx_drs_id = -1};
  for (int i = 0; i < 50; i++) {
    map.terrains.terrains[i] = -1;
  }

  const char* id = ezxml_attr(node, "id");
  if (id == NULL) {
    return NoId;
  }
  map.id = atoi(id);
  if (map.id < 0) {
    map.id = 255 + (char)map.id;
  }
  if (map.id > 255) {
    return TooBigId;
  }

  const char* name = ezxml_attr(node, "name");
  if (name == NULL || strlen(name) < 1) {
    return NoName;
  }
  map.name = calloc(1, strlen(name) + 1);
  strcpy(map.name, name);

  const char* string = ezxml_attr(node, "string");
  if (string == NULL) {
    return NoStringId;
  }
  map.string = atoi(string);

  const char* drs_id = ezxml_attr(node, "drsId");
  if (drs_id == NULL) {
    return NoDrsId;
  }
  map.drs_id = atoi(drs_id);

  const char* scx_drs_id = ezxml_attr(node, "scxDrsId");
  if (scx_drs_id != NULL) {
    map.scx_drs_id = atoi(scx_drs_id);
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
  char const_pattern[80];
  strcpy(const_pattern, "%s");
  if (type == RealWorld) {
    strcpy(const_pattern, "real-world-%s");
  } else if (type >= CustomSection) {
    char* prefix = custom_sections[type - CustomSection].ai_const_prefix;
    if (prefix != NULL) {
      sprintf(const_pattern, "%s-%%s", prefix);
    }
  }
  int const_len = sprintf(const_name, const_pattern, name);
  CharLowerBuffA(const_name, const_len);
  map.ai_const_name = calloc(1, const_len + 1);
  strcpy(map.ai_const_name, const_name);

  // Derive ai symbol name: uppercase name suffixed with -MAP,
  // with REAL-WORLD- prefix for real world maps
  char symbol_name[100];
  char symbol_pattern[80];
  strcpy(symbol_pattern, "%s");
  if (type == RealWorld) {
    strcpy(symbol_pattern, "REAL-WORLD-%s");
  } else if (type >= CustomSection) {
    char* prefix = custom_sections[type - CustomSection].ai_symbol_prefix;
    if (prefix != NULL) {
      sprintf(symbol_pattern, "%s-%%s", prefix);
    }
  }
  int symbol_len = sprintf(symbol_name, symbol_pattern, name);
  CharUpperBuffA(symbol_name, symbol_len);
  map.ai_symbol_name = calloc(1, symbol_len + 1);
  strcpy(map.ai_symbol_name, symbol_name);

  printf("[aoc-builtin-rms] Add modded map: %s\n", map.name);
  size_t i = count_custom_maps();
  custom_maps[i] = map;
  custom_maps[i + 1] = (CustomMap){0};
  return MapOk;
}

typedef enum parse_section_result {
  SectionOk,
  InvalidTag,
  NoCustomName
} parse_section_result_t;
static parse_section_result_t parse_section(ezxml_t node) {
  const char* tag_name = ezxml_name(node);
  CustomMapType type = CustomSection;
  dbg_print("found section tag: %s\n", tag_name);
  if (strcmp(tag_name, "standard") == 0) {
    type = Standard;
  } else if (strcmp(tag_name, "real-world") == 0) {
    type = RealWorld;
  } else if (strcmp(tag_name, "section") == 0) {
    size_t i = count_custom_sections();
    const char* name = ezxml_attr(node, "name");
    if (name == NULL) {
      return NoCustomName;
    }
    custom_sections[i].name = atoi(name);

    const char* default_map = ezxml_attr(node, "default");
    if (default_map == NULL) {
      custom_sections[i].default_map = -1;
    } else {
      custom_sections[i].default_map = atoi(default_map);
      if (custom_sections[i].default_map < 0) {
        custom_sections[i].default_map =
            255 + (char)custom_sections[i].default_map;
      }
    }

    const char* ai_symbol_prefix = ezxml_attr(node, "aiSymbolPrefix");
    if (ai_symbol_prefix != NULL) {
      custom_sections[i].ai_symbol_prefix = strdup(ai_symbol_prefix);
    } else {
      custom_sections[i].ai_symbol_prefix = NULL;
    }

    const char* ai_const_prefix = ezxml_attr(node, "aiConstPrefix");
    if (ai_const_prefix != NULL) {
      custom_sections[i].ai_const_prefix = strdup(ai_const_prefix);
    } else {
      custom_sections[i].ai_const_prefix = NULL;
    }

    type = CustomSection + i;
  } else {
    return InvalidTag;
  }

  for (ezxml_t map = ezxml_child(node, "map"); map != NULL;
       map = ezxml_next(map)) {
    ParseMapResult err = parse_map(map, type);
    switch (err) {
    case NoId:
      MessageBoxA(NULL, "A <map /> is missing an id attribute", NULL, 0);
      break;
    case NoName:
      MessageBoxA(NULL, "A <map /> is missing a name attribute", NULL, 0);
      break;
    case NoStringId:
      MessageBoxA(NULL, "A <map /> is missing a string attribute", NULL, 0);
      break;
    case NoDrsId:
      MessageBoxA(NULL, "A <map /> is missing a drsId attribute", NULL, 0);
      break;
    case TooBigId:
      MessageBoxA(NULL, "A <map />'s map ID is too large: must be at most 255",
                  NULL, 0);
      break;
    case TooBigTerrain:
      MessageBoxA(NULL,
                  "A <map /> has an incorrect terrainOverrides attribute. Only "
                  "terrains 15000-15050 can be overridden",
                  NULL, 0);
      break;
    case MapOk:
      if (custom_sections[count_custom_sections() - 1].default_map == -1) {
        custom_sections[count_custom_sections() - 1].default_map =
            custom_maps[count_custom_maps() - 1].id;
      }
      break;
    default:
      break;
    }
  }

  return SectionOk;
}

/**
 * Parse the maps out of an aoc-builtin-rms.xml file.
 */
static void parse_maps(char* mod_config) {
  char err_message[256];
  ezxml_t document = ezxml_parse_str(mod_config, strlen(mod_config));
  if (!document) {
    MessageBoxA(NULL,
                "Could not parse aoc-builtin-rms.xml, please check its syntax",
                NULL, 0);
    return;
  }

  const char* root_name = ezxml_name(document);
  if (root_name == NULL || strncmp(root_name, "random-maps", 11) != 0) {
    sprintf(err_message,
            "The top-level element must be <random-maps>, got <%s>", root_name);
    MessageBoxA(NULL, err_message, NULL, 0);
    ezxml_free(document);
    return;
  }

  for (ezxml_t section = document->child; section != NULL;
       section = section->ordered) {
    parse_section_result_t err = parse_section(section);
    switch (err) {
    case InvalidTag:
      MessageBoxA(NULL,
                  "Only <standard>, <real-world>, <section> are allowed in "
                  "this context",
                  NULL, 0);
      break;
    default:
      break;
    }
  }

  ezxml_free(document);
}

static char* read_file(char* filename) {
  FILE* file = fopen(filename, "rb");
  if (file == NULL)
    return NULL;
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

void mmm_load(mmm_mod_info* info) {
  info->name = "Custom Builtin RMS";
  info->version = AOC_BUILTIN_RMS_VERSION;
}

void mmm_after_setup(mmm_mod_info* info) {
  dbg_print("init()\n");
  const char* mod_name = info->meta->mod_short_name;
  dbg_print("mod name: %s\n", mod_name);
  if (mod_name == NULL)
    return;

  const char* mod_path = info->meta->mod_base_dir;
  dbg_print("mod path: %s\n", mod_path);
  if (mod_path == NULL)
    return;

  char xml_path[MAX_PATH];
  // mod_path includes trailing slash
  sprintf(xml_path, "%saoc-builtin-rms.xml", mod_path);

  dbg_print("config path: %s\n", xml_path);
  char* config = read_file(xml_path);
  if (config == NULL)
    return;

  parse_maps(config);
  free(config);

  if (count_custom_maps() == 0) {
    printf("[aoc-builtin-rms] No custom maps specified, stopping.\n");
    return;
  }
  printf("[aoc-builtin-rms] Found custom maps, installing hooks.\n");

  aoc_builtin_rms_init(custom_sections, count_custom_sections(), custom_maps,
                       count_custom_maps());
}

void mmm_unload(mmm_mod_info* info) {
  dbg_print("deinit()\n");
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
  switch (reason) {
  case DLL_PROCESS_ATTACH:
    DisableThreadLibraryCalls(dll);
    break;
  }
  return 1;
}
