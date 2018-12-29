#include <windows.h>
#include <stdio.h>

#define RMS_STANDARD 0
#define RMS_REALWORLD 1

struct CustomMap {
  /* Internal ID of the map */
  char id;
  /* Visible name of the map */
  char* name;
  /* DRS ID of the map */
  int drs_id;
  /* Kind of map (standard/real world) */
  char type;
  /* DRS ID of the scenario if a real world map (not yet supported) */
  int scx_drs_id;
};

struct CustomMap custom_maps[] = {
  { .id = -1, .name = "Hideout", .drs_id = 54208, .type = RMS_STANDARD },
  { .id = -2, .name = "Budapest", .drs_id = 54208, .type = RMS_REALWORLD },
  { .id = 0 }
};

/* offsets for finding data */
const size_t offs_game_instance = 0x7912A0;
const size_t offs_map_type = 0x13DC;

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

static fn_rms_controller_constructor aoc_rms_controller_constructor = 0;
static fn_dropdown_add_line aoc_dropdown_add_line = 0;
static fn_text_add_line aoc_text_add_line = 0;
static fn_dropdown_add_string aoc_dropdown_add_string = 0;

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
  /* Stuff to display custom maps in the dropdown menus */
  aoc_dropdown_add_line = (fn_dropdown_add_line) offs_dropdown_add_line;
  aoc_dropdown_add_string = (fn_dropdown_add_string) offs_dropdown_add_string;
  aoc_text_add_line = (fn_text_add_line) offs_text_add_line;
  install_jmphook((void*)offs_dropdown_add_line, dropdown_add_line_hook);

  /* Stuff to resolve the custom map ID to a DRS file */
  aoc_rms_controller_constructor = (fn_rms_controller_constructor) offs_rms_controller_constructor;
  install_callhook((void*)offs_rms_controller, rms_controller_hook);
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
