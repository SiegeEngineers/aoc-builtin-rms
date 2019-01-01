#include <windows.h>
#include <stdio.h>

#define RMS_STANDARD 0
#define RMS_REALWORLD 1

#ifdef DEBUG
#  define debug(...) printf(__VA_ARGS__)
#else
#  define debug(...)
#endif

struct CustomMap {
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
};

/**
 * Store custom builtin maps.
 * 100 is a random maximum number I picked that seems high enough for most mods.
 */
static struct CustomMap custom_maps[100] = {
  { 0 }
};

/* offsets for finding data */

/* location of the main game instance object */
static const size_t offs_game_instance = 0x7912A0;
/* offset of the map type attribute in the game_instance struct */
static const size_t offs_map_type = 0x13DC;
/* offset of the world instance pointer in the game_instance struct */
static const size_t offs_world = 0x424;
/* location of the XML source for the active UP mod */
static const size_t offs_game_xml = 0x7A5070;

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

/**
 * Parse a <map /> XML element from the string pointed to by `read_ptr_ptr`.
 * The pointer in `read_ptr_ptr` will point past the end of the <map/> element
 * after this function.
 */
static int parse_map(char** read_ptr_ptr) {
  struct CustomMap map = {
    .id = 0,
    .name = NULL,
    .string = -1,
    .description = -1,
    .ai_const_name = NULL,
    .ai_symbol_name = NULL,
    .type = RMS_STANDARD,
    .scx_drs_id = -1
  };

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
  name[strlen(name) - 1] = '\0'; // chop off the "
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

  printf("[aoe2-builtin-rms] Add modded map: %s\n", map.name);
  size_t i = count_custom_maps();
  custom_maps[i] = map;
  custom_maps[i + 1] = (struct CustomMap) {0};

  *read_ptr_ptr = end_ptr + 2;
  return 0;
}

/**
 * Parse a <random-maps> section from a UserPatch mod description file.
 */
static void parse_maps() {
  char* mod_config = *(char**)offs_game_xml;
  char* read_ptr = strstr(mod_config, "<random-maps>");
  char* end_ptr = strstr(read_ptr, "</random-maps>");
  debug("[aoe2-builtin-rms] range: %p %p\n", read_ptr, end_ptr);

  if (read_ptr == NULL || end_ptr == NULL) return;

  while ((read_ptr = strstr(read_ptr, "<map")) && read_ptr < end_ptr) {
    int err = parse_map(&read_ptr);
    switch (err) {
      case ERR_NO_ID: MessageBoxA(NULL, "A <map /> is missing an id attribute", NULL, 0); break;
      case ERR_NO_NAME: MessageBoxA(NULL, "A <map /> is missing a name attribute", NULL, 0); break;
      case ERR_NO_STRING_ID: MessageBoxA(NULL, "A <map /> is missing a string attribute", NULL, 0); break;
      case ERR_NO_DRS_ID: MessageBoxA(NULL, "A <map /> is missing a drsId attribute", NULL, 0); break;
      case ERR_TOO_BIG_ID: MessageBoxA(NULL, "A <map />'s map ID is too large: must be at most 255", NULL, 0); break;
      default: break;
    }
  }
}

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
    debug("[aoe2-builtin-rms] called hooked dropdown_add_line %p %p, %d %d\n", dd, text_panel, label, value);
  }

  // Original
  aoc_text_add_line(text_panel, label, value);

  if (additional_type != -1) {
    for (int i = 0; custom_maps[i].id; i++) {
      if (custom_maps[i].type != additional_type) continue;
      aoc_text_add_line(text_panel,
          custom_maps[i].string,
          custom_maps[i].id);
    }
  }
}

static int __thiscall text_get_map_value_hook(void* tt, int line_index) {
  debug("[aoe2-builtin-rms] called hooked text_get_map_value %p %d\n", tt, line_index);
  int selected_map_id = aoc_text_get_value(tt, line_index);

  for (int i = 0; custom_maps[i].id; i++) {
    if (custom_maps[i].id != selected_map_id) continue;
    if (custom_maps[i].description != -1) {
      aoc_text_set_rollover_id(tt, line_index, custom_maps[i].description);
      return 0; /* doesn't have a hardcoded ID so we won't call set_rollover_id twice */
    }
  }

  return selected_map_id;
}

static void* current_game_info;
static void __thiscall map_generate_hook(void* map, int size_x, int size_y, char* name, void* game_info, int num_players) {
  debug("[aoe2-builtin-rms] called hooked map_generate %s %p\n", name, game_info);
  /* We need to store this to be able to load the scx file later */
  current_game_info = game_info;
  aoc_map_generate(map, size_x, size_y, name, game_info, num_players);
}

static char map_filename_str[MAX_PATH];
static char scx_filename_str[MAX_PATH];
static void* __thiscall rms_controller_hook(void* controller, char* filename, int drs_id) {
  debug("[aoe2-builtin-rms] called hooked rms_controller %s %d\n", filename, drs_id);
  int map_type = get_map_type();
  debug("[aoe2-builtin-rms] map type: %d\n", map_type);
  for (int i = 0; custom_maps[i].id; i++) {
    if (custom_maps[i].id == map_type) {
      sprintf(map_filename_str, "%s.rms", custom_maps[i].name);
      filename = map_filename_str;
      drs_id = custom_maps[i].drs_id;
      debug("[aoe2-builtin-rms] filename/id is now: %s %d\n", filename, drs_id);

      if (custom_maps[i].scx_drs_id > 0) {
        sprintf(scx_filename_str, "real_world_%s.scx", custom_maps[i].name);
        debug("[aoe2-builtin-rms] real world map: loading %s %d\n",
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
    for (int i = 0; custom_maps[i].id; i++) {
      if (custom_maps[i].id == map_type) {
        name = custom_maps[i].ai_symbol_name;
        debug("[aoe2-builtin-rms] defining ai symbol: %s\n", name);
        break;
      }
    }
  }
  return aoc_ai_define_symbol(ai, name);
}

static int __thiscall ai_define_map_const_hook(void* ai, char* name, int value) {
  for (int i = 0; custom_maps[i].id; i++) {
    debug("[aoe2-builtin-rms] defining ai const: %s = %d\n", custom_maps[i].ai_const_name, custom_maps[i].id);
    aoc_ai_define_const(ai,
        custom_maps[i].ai_const_name,
        custom_maps[i].id);
  }
  return aoc_ai_define_const(ai, "scenario-map", -1);
}

/**
 * Overwrite some bytes in the current process.
 * Returns TRUE if it worked, FALSE if not.
 */
static BOOL overwrite_bytes(void* ptr, void* value, size_t size) {
  DWORD old;
  DWORD tmp;
  if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READWRITE, &old)) {
    debug("[aoe2-builtin-rms] Couldn't unprotect?! @ %p\n", ptr);
    return FALSE;
  }
  memcpy(ptr, value, size);
  VirtualProtect(ptr, size, old, &tmp);
  return TRUE;
}

/**
 * Install a hook that works by JMP-ing to the new implementation.
 * This is handy for hooking existing functions, override their first bytes by a JMP and all the arguments will still be on the stack/in registries. C functions can just be used (with appropriate calling convention) as if they were called directly by the application.
 */
static BOOL install_jmphook (void* orig_address, void* hook_address) {
  char patch[6] = {
    0xE9, // jmp
    0, 0, 0, 0, // addr
    0xC3 // ret
  };
  int offset = PtrToUlong(hook_address) - PtrToUlong(orig_address + 5);
  memcpy(&patch[1], &offset, sizeof(offset));
  debug("[aoe2-builtin-rms] installing hook at %p JMP %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  return overwrite_bytes(orig_address, patch, 6);
}

/**
 * Install a hook that works by CALL-ing to the new implementation.
 * Handy for hooking existing CALL-sites, overriding essentially just the address.
 */
static BOOL install_callhook (void* orig_address, void* hook_address) {
  char patch[5] = {
    0xE8, // call
    0, 0, 0, 0 // addr
  };
  int offset = PtrToUlong(hook_address) - PtrToUlong(orig_address + 5);
  memcpy(&patch[1], &offset, sizeof(offset));
  debug("[aoe2-builtin-rms] installing hook at %p CALL %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  return overwrite_bytes(orig_address, patch, 5);
}

/**
 * Install a hook that works by overriding a pointer in a vtable.
 * Handy for hooking class methods.
 */
static BOOL install_vtblhook (void* orig_address, void* hook_address) {
  /* int offset = PtrToUlong(hook_address) - PtrToUlong(orig_address + 5); */
  int offset = PtrToUlong(hook_address);
  debug("[aoe2-builtin-rms] installing hook at %p VTBL %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  return overwrite_bytes(orig_address, (char*)&offset, 4);
}

static void init() {
  debug("[aoe2-builtin-rms] init()\n");
  parse_maps();

  if (count_custom_maps() == 0) {
    printf("[aoe2-builtin-rms] No custom maps specified, stopping.\n");
    return;
  }
  printf("[aoe2-builtin-rms] Found custom maps, installing hooks.\n");

  /* Stuff to display custom maps in the dropdown menus */
  aoc_dropdown_add_line = (fn_dropdown_add_line) offs_dropdown_add_line;
  aoc_dropdown_add_string = (fn_dropdown_add_string) offs_dropdown_add_string;
  aoc_text_add_line = (fn_text_add_line) offs_text_add_line;
  aoc_text_get_value = (fn_text_get_value) offs_text_get_value;
  aoc_text_set_rollover_id = (fn_text_set_rollover_id) offs_text_set_rollover_id;
  install_jmphook((void*) offs_dropdown_add_line, dropdown_add_line_hook);
  install_callhook((void*) offs_text_get_map_value, text_get_map_value_hook);

  /* Stuff to resolve the custom map ID to a DRS file */
  aoc_rms_controller_constructor = (fn_rms_controller_constructor) offs_rms_controller_constructor;
  install_callhook((void*) offs_rms_controller, rms_controller_hook);

  /* Stuff to add AI constants */
  aoc_ai_define_symbol = (fn_ai_define_symbol) offs_ai_define_symbol;
  aoc_ai_define_const = (fn_ai_define_const) offs_ai_define_const;
  install_callhook((void*) offs_ai_define_map_symbol, ai_define_map_symbol_hook);
  install_callhook((void*) offs_ai_define_map_const, ai_define_map_const_hook);

  /* Stuff to make real world maps work */
  aoc_map_generate = (fn_map_generate) offs_map_generate;
  aoc_load_scx = (fn_load_scx) offs_load_scx;
  install_vtblhook((void*) offs_vtbl_map_generate, map_generate_hook);
}

static void deinit() {
  debug("[aoe2-builtin-rms] deinit()\n");
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
  debug("[aoe2-builtin-rms] DllMain %ld\n", reason);
  switch (reason) {
    case DLL_PROCESS_ATTACH: init(); break;
    case DLL_PROCESS_DETACH: deinit(); break;
  }
  return 1;
}
