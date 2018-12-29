#include <windows.h>
#include <stdio.h>

/* size_t offs_map_generate = 0x45EE10; */
/* size_t offs_rms_controller = 0x534C40; */
size_t offs_dropdown_add_line = 0x550870;
size_t offs_text_add_line = 0x5473F0;
size_t offs_dropdown_add_string = 0x550840;
typedef int __thiscall (*fn_dropdown_add_line)(void*, int, int);
typedef int __thiscall (*fn_text_add_line)(void*, int, int);
typedef int __thiscall (*fn_dropdown_add_string)(void*, char*, int);

static fn_dropdown_add_line aoc_dropdown_add_line = 0;
static fn_text_add_line aoc_text_add_line = 0;
static fn_dropdown_add_string aoc_dropdown_add_string = 0;

char is_last_map_dropdown_entry(int label, int value) {
  /* Yucatan */
  return label == 10894 && value == 27;
}

void __thiscall dropdown_add_line_hook(void* dd, int label, int value) {
  int dd_offset = (int)dd;
  void* text_panel = *(void**)(dd_offset + 256);
  printf("[aoe2-builtin-rms] called hooked fn %p %p, %d %d!\n", dd, text_panel, label, value);
  if (text_panel != NULL) {
    aoc_text_add_line(text_panel, label, value);
  }
  if (is_last_map_dropdown_entry(label, value)) {
    aoc_dropdown_add_string(dd, "FauxMap", 33);
  }
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
  printf("[aoe2-builtin-rms] installing hook at %p jmp %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  overwrite_bytes(orig_address, patch, 6);
}

__declspec(dllexport) void init() {
  printf("[aoe2-builtin-rms] init()\n");
  aoc_dropdown_add_line = (fn_dropdown_add_line) offs_dropdown_add_line;
  aoc_dropdown_add_string = (fn_dropdown_add_string) offs_dropdown_add_string;
  aoc_text_add_line = (fn_text_add_line) offs_text_add_line;
  install_jmphook((void*)offs_dropdown_add_line, dropdown_add_line_hook);
}

__declspec(dllexport) void deinit() {
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
