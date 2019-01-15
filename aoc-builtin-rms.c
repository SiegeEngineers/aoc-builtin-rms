#include <windows.h>
#include <stdio.h>
#include <assert.h>
#include "aoc-builtin-rms.h"
#include "hook.h"

#define RMS_STANDARD 0
#define RMS_REALWORLD 1

#define TERRAIN_TEXTURE_BASE 15000
#define TERRAIN_TEXTURE_MAX 15050

#ifdef DEBUG
#  define dbg_print(...) printf("[aoc-builtin-rms] " __VA_ARGS__)
#else
#  define dbg_print(...)
#endif

static custom_map_t* custom_maps = NULL;
static size_t num_custom_maps = 0;

/* offsets for finding data */

/* location of the main game instance object */
static const size_t offs_game_instance = 0x7912A0;
/* offset of the map type attribute in the game_instance struct */
static const size_t offs_map_type = 0x13DC;
/* offset of the world instance pointer in the game_instance struct */
static const size_t offs_world = 0x424;
/* offset of the map data pointer in the world struct */
static const size_t offs_map = 0x34;
/* offset of the terrains data pointer in the map struct */
static const size_t offs_terrains = 0x8C;
static const size_t sizeof_terrain_struct = 0x1B4;
/* offset of the texture id and pointer in the terrain struct */
static const size_t offs_terrain_texture_id = 0x1C;
static const size_t offs_terrain_texture = 0x20;

/* offsets for hooking the random map file reading/parsing */

/* location of the controller call site that has correct filename,drs_id parameters */
static const size_t offs_rms_controller = 0x45F717;
/* location of the controller constructor */
static const size_t offs_rms_controller_constructor = 0x534C40;
typedef void* __thiscall (*fn_rms_load_scx)(void*, char*, int, void*);
typedef void* __thiscall (*fn_rms_controller_constructor)(void*, char*, int);

/* offsets for hooking the builtin random map file list */

static const size_t offs_dropdown_add_line = 0x550870;
static const size_t offs_text_add_line = 0x5473F0;
static const size_t offs_dropdown_add_string = 0x550840;
/* location of fn that gets the ID value of a text panel row */
static const size_t offs_text_get_value = 0x5479D0;
/* location of fn that sets the hover description for a text panel row */
static const size_t offs_text_set_rollover_id = 0x547C20;
/* location of the call to text_get_value that happens when applying rollover description IDs */
static const size_t offs_text_get_map_value = 0x5086E1;
typedef int __thiscall (*fn_dropdown_add_line)(void*, int, int);
typedef int __thiscall (*fn_text_add_line)(void*, int, int);
typedef int __thiscall (*fn_dropdown_add_string)(void*, char*, int);
typedef int __thiscall (*fn_text_get_value)(void*, int);
typedef int __thiscall (*fn_text_set_rollover_id)(void*, int, int);

/* offsets for defining the AI constants */
static const size_t offs_ai_define_symbol = 0x5F74F0;
static const size_t offs_ai_define_const = 0x5F7530;
static const size_t offs_ai_define_map_symbol = 0x4A27F7;
static const size_t offs_ai_define_map_const = 0x4A4470;
typedef int __thiscall (*fn_ai_define_symbol)(void*, char*);
typedef int __thiscall (*fn_ai_define_const)(void*, char*, int);

/* offsets for real world map hooks */
static const size_t offs_vtbl_map_generate = 0x638114;
static const size_t offs_map_generate = 0x45EE10;
static const size_t offs_load_scx = 0x40DF00;
typedef int __thiscall (*fn_map_generate)(void*, int, int, char*, void*, int);
typedef int __thiscall (*fn_load_scx)(void*, char*, int, void*);

/* offsets for terrain overrides */
static const size_t offs_texture_create = 0x4DAE00;
static const size_t offs_texture_destroy = 0x4DB110;
typedef void* __thiscall (*fn_texture_create)(void*, char*, int);
typedef void __thiscall (*fn_texture_destroy)(void*);

static fn_rms_controller_constructor aoc_rms_controller_constructor = 0;
static fn_dropdown_add_line aoc_dropdown_add_line = 0;
static fn_text_add_line aoc_text_add_line = 0;
static fn_dropdown_add_string aoc_dropdown_add_string = 0;
static fn_text_get_value aoc_text_get_value = 0;
static fn_text_set_rollover_id aoc_text_set_rollover_id = 0;
static fn_ai_define_symbol aoc_ai_define_symbol = 0;
static fn_ai_define_const aoc_ai_define_const = 0;
static fn_map_generate aoc_map_generate = 0;
static fn_load_scx aoc_load_scx = 0;
static fn_texture_create aoc_texture_create = 0;
static fn_texture_destroy aoc_texture_destroy = 0;

static int get_map_type() {
  int base_offset = *(int*)offs_game_instance;
  return *(int*)(base_offset + offs_map_type);
}

static void* get_world() {
  int base_offset = *(int*)offs_game_instance;
  return *(void**)(base_offset + offs_world);
}

static char is_last_map_dropdown_entry(int label, int value) {
  return label == 10894 && value == 27; /* Yucatan */
}
static char is_last_real_world_dropdown_entry(int label, int value) {
  return label == 13553 && value == 43; /* Byzantium */
}

static void __thiscall dropdown_add_line_hook(void* dd, int label, int value) {
  int dd_offset = (int)dd;
  void* text_panel = *(void**)(dd_offset + 256);
  if (text_panel == NULL) {
    return;
  }

  int additional_type = -1;
  if (is_last_map_dropdown_entry(label, value))
    additional_type = RMS_STANDARD;
  if (is_last_real_world_dropdown_entry(label, value))
    additional_type = RMS_REALWORLD;

  if (additional_type !=  -1) {
    dbg_print("called hooked dropdown_add_line %p %p, %d %d\n", dd, text_panel, label, value);
  }

  // Original
  aoc_text_add_line(text_panel, label, value);

  if (additional_type != -1) {
    for (int i = 0; i < num_custom_maps; i++) {
      if (custom_maps[i].type != additional_type) continue;
      aoc_text_add_line(text_panel,
          custom_maps[i].string,
          custom_maps[i].id);
    }
  }
}

static int __thiscall text_get_map_value_hook(void* tt, int line_index) {
  dbg_print("called hooked text_get_map_value %p %d\n", tt, line_index);
  int selected_map_id = aoc_text_get_value(tt, line_index);

  for (int i = 0; i < num_custom_maps; i++) {
    if (custom_maps[i].id != selected_map_id) continue;
    if (custom_maps[i].description != -1) {
      aoc_text_set_rollover_id(tt, line_index, custom_maps[i].description);
      return 0; /* doesn't have a hardcoded ID so we won't call set_rollover_id twice */
    }
  }

  return selected_map_id;
}

static void replace_terrain_texture(void* texture, int terrain_id, int slp_id) {
  dbg_print("Replacing texture for terrain #%d by SLP %d\n", terrain_id, slp_id);
  aoc_texture_destroy(texture);
  char name[100];
  sprintf(name, "terrain%d.shp", terrain_id);
  aoc_texture_create(texture, name, slp_id);
}

static void apply_terrain_overrides(terrain_overrides_t* overrides) {
  void* world = get_world();
  size_t world_offset = (size_t) world;
  void* map = *(void**)(world_offset + offs_map);
  size_t map_offset = (size_t) map;
  for (int i = 0; i < 42; i++) {
    size_t terrain_offset = map_offset + offs_terrains + (sizeof_terrain_struct * i);
    int* texture_id = (int*)(terrain_offset + offs_terrain_texture_id);
    void* texture = *(void**)(terrain_offset + offs_terrain_texture);
    if (*texture_id < TERRAIN_TEXTURE_BASE || *texture_id >= TERRAIN_TEXTURE_MAX) continue;

    int new_texture_id = overrides->terrains[*texture_id - TERRAIN_TEXTURE_BASE];
    // In UserPatch 1.5, ZR@ terrains are applied very late (after this function runs)
    // They work by looping through _all_ terrain types on _every_ map, even non-ZR@,
    // and re-constructing the texture instances (in the same way as replace_terrain_texture).
    // So, it also reconstructs non-changed textures, based on the texture_id. That's why
    // we overwrite it here. the game doesn't actually use it.
    if (new_texture_id != -1) {
      *texture_id = new_texture_id;
      replace_terrain_texture(texture, i, new_texture_id);
    }
  }
}

static void* current_game_info;
static void __thiscall map_generate_hook(void* map, int size_x, int size_y, char* name, void* game_info, int num_players) {
  dbg_print("called hooked map_generate %s %p\n", name, game_info);
  /* We need to store this to be able to load the scx file later */
  current_game_info = game_info;
  aoc_map_generate(map, size_x, size_y, name, game_info, num_players);
}

static char map_filename_str[MAX_PATH];
static char scx_filename_str[MAX_PATH];
static void* __thiscall rms_controller_hook(void* controller, char* filename, int drs_id) {
  dbg_print("called hooked rms_controller %s %d\n", filename, drs_id);
  int map_type = get_map_type();
  dbg_print("map type: %d\n", map_type);
  for (int i = 0; i < num_custom_maps; i++) {
    if (custom_maps[i].id == map_type) {
      sprintf(map_filename_str, "%s.rms", custom_maps[i].name);
      filename = map_filename_str;
      drs_id = custom_maps[i].drs_id;
      dbg_print("filename/id is now: %s %d\n", filename, drs_id);

      apply_terrain_overrides(&custom_maps[i].terrains);

      if (custom_maps[i].scx_drs_id > 0) {
        sprintf(scx_filename_str, "real_world_%s.scx", custom_maps[i].name);
        dbg_print("real world map: loading %s %d\n",
            scx_filename_str, custom_maps[i].scx_drs_id);
        aoc_load_scx(get_world(),
            scx_filename_str,
            custom_maps[i].scx_drs_id,
            current_game_info);
      }

      break;
    }
  }

  return aoc_rms_controller_constructor(controller, filename, drs_id);
}

static int __thiscall ai_define_map_symbol_hook(void* ai, char* name) {
  if (strcmp(name, "SCENARIO-MAP") == 0) {
    int map_type = get_map_type();
    for (int i = 0; i < num_custom_maps; i++) {
      if (custom_maps[i].id == map_type) {
        name = custom_maps[i].ai_symbol_name;
        dbg_print("defining ai symbol: %s\n", name);
        break;
      }
    }
  }
  return aoc_ai_define_symbol(ai, name);
}

static int __thiscall ai_define_map_const_hook(void* ai, char* name, int value) {
  for (int i = 0; i < num_custom_maps; i++) {
    dbg_print("defining ai const: %s = %d\n", custom_maps[i].ai_const_name, custom_maps[i].id);
    aoc_ai_define_const(ai,
        custom_maps[i].ai_const_name,
        custom_maps[i].id);
  }
  return aoc_ai_define_const(ai, "scenario-map", -1);
}

hook_t hooks[20];
void aoc_builtin_rms_init(custom_map_t* new_custom_maps, size_t new_num_custom_maps) {
  dbg_print("init()\n");

  assert(custom_maps == NULL);
  assert(num_custom_maps == 0);
  custom_maps = new_custom_maps;
  num_custom_maps = new_num_custom_maps;

  int h = 0; // hooks counter

  /* Stuff to display custom maps in the dropdown menus */
  aoc_dropdown_add_line = (fn_dropdown_add_line) offs_dropdown_add_line;
  aoc_dropdown_add_string = (fn_dropdown_add_string) offs_dropdown_add_string;
  aoc_text_add_line = (fn_text_add_line) offs_text_add_line;
  aoc_text_get_value = (fn_text_get_value) offs_text_get_value;
  aoc_text_set_rollover_id = (fn_text_set_rollover_id) offs_text_set_rollover_id;
  hooks[h++] = install_jmphook((void*) offs_dropdown_add_line, dropdown_add_line_hook);
  hooks[h++] = install_callhook((void*) offs_text_get_map_value, text_get_map_value_hook);

  /* Stuff to override terrain textures */
  aoc_texture_create = (fn_texture_create) offs_texture_create;
  aoc_texture_destroy = (fn_texture_destroy) offs_texture_destroy;

  /* Stuff to resolve the custom map ID to a DRS file */
  aoc_rms_controller_constructor = (fn_rms_controller_constructor) offs_rms_controller_constructor;
  hooks[h++] = install_callhook((void*) offs_rms_controller, rms_controller_hook);

  /* Stuff to add AI constants */
  aoc_ai_define_symbol = (fn_ai_define_symbol) offs_ai_define_symbol;
  aoc_ai_define_const = (fn_ai_define_const) offs_ai_define_const;
  hooks[h++] = install_callhook((void*) offs_ai_define_map_symbol, ai_define_map_symbol_hook);
  hooks[h++] = install_callhook((void*) offs_ai_define_map_const, ai_define_map_const_hook);

  /* Stuff to make real world maps work */
  aoc_map_generate = (fn_map_generate) offs_map_generate;
  aoc_load_scx = (fn_load_scx) offs_load_scx;
  hooks[h++] = install_vtblhook((void*) offs_vtbl_map_generate, map_generate_hook);
  hooks[h] = NULL;
}

void deinit() {
  dbg_print("deinit()\n");

  for (int h = 0; hooks[h] != NULL; h++) {
    revert_hook(hooks[h]);
  }

  custom_maps = NULL;
  num_custom_maps = 0;
}
