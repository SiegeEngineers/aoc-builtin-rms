#include <windows.h>
#include <stdio.h>

#define RMS_STANDARD 0
#define RMS_REALWORLD 1

struct CustomMap {
  /* Internal ID of the map */
  char id;
  /* Visible name of the map */
  char* name;
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
struct CustomMap custom_maps[100] = {
  { 0 }
};

/* offsets for finding data */
const size_t offs_game_instance = 0x7912A0;
const size_t offs_map_type = 0x13DC;
const size_t offs_game_xml = 0x7A5070;

/* offsets for hooking the random map file reading/parsing */
const size_t offs_rms_controller = 0x45F717;
const size_t offs_rms_controller_constructor = 0x534C40;
typedef void* __thiscall (*fn_rms_load_scx)(void*, char*, int, void*);
typedef void* __thiscall (*fn_rms_controller_constructor)(void*, char*, int);

/* offsets for hooking the builtin random map file list */
const size_t offs_dropdown_add_line = 0x550870;
const size_t offs_text_add_line = 0x5473F0;
const size_t offs_dropdown_add_string = 0x550840;
typedef int __thiscall (*fn_dropdown_add_line)(void*, int, int);
typedef int __thiscall (*fn_text_add_line)(void*, int, int);
typedef int __thiscall (*fn_dropdown_add_string)(void*, char*, int);

/* offsets for defining the AI constants */
const size_t offs_ai_define_symbol = 0x5F74F0;
const size_t offs_ai_define_const = 0x5F7530;
const size_t offs_ai_define_map_symbol = 0x4A27F7;
const size_t offs_ai_define_map_const = 0x4A4470;
typedef int __thiscall (*fn_ai_define_symbol)(void*, char*);
typedef int __thiscall (*fn_ai_define_const)(void*, char*, int);

static fn_rms_controller_constructor aoc_rms_controller_constructor = 0;
static fn_dropdown_add_line aoc_dropdown_add_line = 0;
static fn_text_add_line aoc_text_add_line = 0;
static fn_dropdown_add_string aoc_dropdown_add_string = 0;
static fn_ai_define_symbol aoc_ai_define_symbol = 0;
static fn_ai_define_const aoc_ai_define_const = 0;

/**
 * Count the number of custom maps.
 */
size_t count_custom_maps() {
  size_t i = 0;
  for (; custom_maps[i].id; i++) {}
  return i;
}

#define ERR_NO_ID 1
#define ERR_NO_NAME 2
#define ERR_NO_DRS_ID 3
int parse_map(char type, char** read_ptr_out) {
  struct CustomMap map = {
    .id = 0,
    .name = "",
    .description = -1,
    .ai_const_name = "",
    .ai_symbol_name = "",
    .type = type,
    .scx_drs_id = 0
  };

  char* read_ptr = *read_ptr_out;
  char* end_ptr = strstr(read_ptr, "/>");
  *read_ptr_out = end_ptr + 2;

  char* id_ptr = strstr(read_ptr, "id=\"");
  if (id_ptr == NULL || id_ptr > end_ptr) { return ERR_NO_ID; }
  int map_id = 0;
  sscanf(id_ptr, "id=\"%d\"", &map_id);
  map.id = (char)map_id;

  char* name_ptr = strstr(read_ptr, "name=\"");
  if (name_ptr == NULL || name_ptr > end_ptr) { return ERR_NO_NAME; }
  char name[80];
  sscanf(name_ptr, "name=\"%79s\"", name);
  if (strlen(name) < 1) { return ERR_NO_NAME; }
  name[strlen(name) - 1] = '\0'; // chop off the "
  map.name = calloc(1, strlen(name) + 1);
  strcpy(map.name, name);

  char* drs_id_ptr = strstr(read_ptr, "drsId=\"");
  if (drs_id_ptr == NULL || drs_id_ptr > end_ptr) { return ERR_NO_DRS_ID; }
  sscanf(drs_id_ptr, "drsId=\"%d\"", &map.drs_id);

  // Derive ai const name: lowercase name
  char const_name[80];
  int const_len = sprintf(const_name, "%s", name);
  CharLowerBuffA(const_name, const_len);
  map.ai_const_name = calloc(1, const_len + 1);
  strcpy(map.ai_const_name, const_name);

  // Derive ai symbol name: uppercase name suffixed with -MAP,
  // with REAL-WORLD- prefix for real world maps
  char symbol_name[100];
  int symbol_len = sprintf(symbol_name, type == RMS_REALWORLD ? "REAL-WORLD-%s-MAP" : "%s-MAP", name);
  CharUpperBuffA(symbol_name, symbol_len);
  map.ai_symbol_name = calloc(1, symbol_len + 1);
  strcpy(map.ai_symbol_name, symbol_name);

  printf("[aoe2-builtin-rms] Add modded map: %s\n", map.name);
  size_t i = count_custom_maps();
  custom_maps[i] = map;
  custom_maps[i + 1] = (struct CustomMap) {0};

  *read_ptr_out = end_ptr + 2;
  return 0;
}

void parse_maps() {
  char* mod_config = *(char**)offs_game_xml;
  char* read_ptr = strstr(mod_config, "<random-maps>");
  char* end_ptr = strstr(read_ptr, "</random-maps>");
  printf("[aoe2-builtin-rms] range: %p %p\n", read_ptr, end_ptr);

  if (read_ptr == NULL || end_ptr == NULL) return;

  while ((read_ptr = strstr(read_ptr, "<map")) && read_ptr < end_ptr) {
    // todo check for real world maps
    parse_map(RMS_STANDARD, &read_ptr);
  }
}

int get_map_type() {
  int base_offset = *(int*)offs_game_instance;
  return *(int*)(base_offset + offs_map_type);
}

char is_last_map_dropdown_entry(int label, int value) {
  /* Yucatan */
  return label == 10894 && value == 27;
}
char is_last_real_world_dropdown_entry(int label, int value) {
  /* Byzantium */
  return label == 13553 && value == 43;
}

void __thiscall dropdown_add_line_hook(void* dd, int label, int value) {
  int dd_offset = (int)dd;
  void* text_panel = *(void**)(dd_offset + 256);
  printf("[aoe2-builtin-rms] called hooked dropdown_add_line %p %p, %d %d!\n", dd, text_panel, label, value);
  if (text_panel != NULL) {
    aoc_text_add_line(text_panel, label, value);
  }

  int additional_type = -1;
  if (is_last_map_dropdown_entry(label, value))
    additional_type = RMS_STANDARD;
  if (is_last_real_world_dropdown_entry(label, value))
    additional_type = RMS_REALWORLD;

  if (additional_type != -1) {
    for (int i = 0; custom_maps[i].id; i++) {
      if (custom_maps[i].type != additional_type) continue;
      aoc_dropdown_add_string(dd,
          custom_maps[i].name,
          custom_maps[i].id);
    }
  }
}

static char map_filename_str[MAX_PATH];
void* __thiscall rms_controller_hook(void* controller, char* filename, int drs_id) {
  printf("[aoe2-builtin-rms] called hooked rms_controller %s %d!\n", filename, drs_id);
  int map_type = get_map_type();
  printf("[aoe2-builtin-rms] map type: %d\n", map_type);
  for (int i = 0; custom_maps[i].id; i++) {
    if (custom_maps[i].id == map_type) {
      sprintf(map_filename_str, "%s.rms", custom_maps[i].name);
      filename = map_filename_str;
      drs_id = custom_maps[i].drs_id;
      printf("[aoe2-builtin-rms] filename/id is now: %s %d\n", filename, drs_id);
      break;
    }
  }
  aoc_rms_controller_constructor(controller, filename, drs_id);
  return controller;
}

int __thiscall ai_define_map_symbol_hook(void* ai, char* name) {
  if (strcmp(name, "SCENARIO-MAP") == 0) {
    int map_type = get_map_type();
    for (int i = 0; custom_maps[i].id; i++) {
      if (custom_maps[i].id == map_type) {
        name = custom_maps[i].ai_symbol_name;
        printf("[aoe2-builtin-rms] defining ai symbol: %s\n", name);
        break;
      }
    }
  }
  return aoc_ai_define_symbol(ai, name);
}

int __thiscall ai_define_map_const_hook(void* ai, char* name, int value) {
  for (int i = 0; custom_maps[i].id; i++) {
    printf("[aoe2-builtin-rms] defining ai const: %s = %d\n", custom_maps[i].ai_const_name, custom_maps[i].id);
    aoc_ai_define_const(ai,
        custom_maps[i].ai_const_name,
        custom_maps[i].id);
  }
  return aoc_ai_define_const(ai, "scenario-map", -1);
}

char* get_error_message(HRESULT hr) {
  LPTSTR message = NULL;
  FormatMessage(
    FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
    NULL,
    hr,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    (LPTSTR)&message,
    0,
    NULL
  );

  return message;
}

void overwrite_bytes(void* ptr, void* value, size_t size) {
  DWORD old;
  DWORD tmp;
  if (!VirtualProtect(ptr, size, PAGE_EXECUTE_READWRITE, &old)) {
    printf("[aoe2-builtin-rms] Couldn't unprotect?! @ %p %d %d\n", ptr, size, PAGE_EXECUTE_READWRITE);
    printf("[aoe2-builtin-rms] %s", get_error_message(HRESULT_FROM_WIN32(GetLastError())));
  } else {
    memcpy(ptr, value, size);
    VirtualProtect(ptr, size, old, &tmp);
  }
}

/**
 * Install a hook that works by JMP-ing to the new implementation.
 * This is handy for hooking existing functions, override their first bytes by a JMP and all the arguments will still be on the stack/in registries. C functions can just be used (with appropriate calling convention) as if they were called directly by the application.
 */
void install_jmphook (void* orig_address, void* hook_address) {
  char patch[6] = {
    0xE9, // jmp
    0, 0, 0, 0, // addr
    0xC3 // ret
  };
  int offset = PtrToUlong(hook_address) - PtrToUlong(orig_address + 5);
  memcpy(&patch[1], &offset, sizeof(offset));
  printf("[aoe2-builtin-rms] installing hook at %p JMP %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  overwrite_bytes(orig_address, patch, 6);
}

/**
 * Install a hook that works by CALL-ing to the new implementation.
 * Handy for hooking existing CALL-sites, overriding essentially just the address.
 */
void install_callhook (void* orig_address, void* hook_address) {
  char patch[5] = {
    0xE8, // call
    0, 0, 0, 0 // addr
  };
  int offset = PtrToUlong(hook_address) - PtrToUlong(orig_address + 5);
  memcpy(&patch[1], &offset, sizeof(offset));
  printf("[aoe2-builtin-rms] installing hook at %p CALL %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  overwrite_bytes(orig_address, patch, 5);
}

void init() {
  printf("[aoe2-builtin-rms] init()\n");
  parse_maps();

  if (count_custom_maps() == 0) {
    printf("[aoe2-builtin-rms] no custom maps specified, stopping\n");
    return;
  }

  /* Stuff to display custom maps in the dropdown menus */
  aoc_dropdown_add_line = (fn_dropdown_add_line) offs_dropdown_add_line;
  aoc_dropdown_add_string = (fn_dropdown_add_string) offs_dropdown_add_string;
  aoc_text_add_line = (fn_text_add_line) offs_text_add_line;
  install_jmphook((void*)offs_dropdown_add_line, dropdown_add_line_hook);

  /* Stuff to resolve the custom map ID to a DRS file */
  aoc_rms_controller_constructor = (fn_rms_controller_constructor) offs_rms_controller_constructor;
  install_callhook((void*)offs_rms_controller, rms_controller_hook);

  /* Stuff to add AI constants */
  aoc_ai_define_symbol = (fn_ai_define_symbol) offs_ai_define_symbol;
  aoc_ai_define_const = (fn_ai_define_const) offs_ai_define_const;
  install_callhook((void*)offs_ai_define_map_symbol, ai_define_map_symbol_hook);
  install_callhook((void*)offs_ai_define_map_const, ai_define_map_const_hook);
}

void deinit() {
  printf("[aoe2-builtin-rms] deinit()\n");
}

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, void* _) {
  printf("[aoe2-builtin-rms] DllMain %ld\n", reason);
  switch (reason) {
    case DLL_PROCESS_ATTACH: init(); break;
    case DLL_PROCESS_DETACH: deinit(); break;
  }
  return 1;
}
