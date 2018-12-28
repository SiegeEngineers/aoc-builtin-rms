#include <windows.h>
#include <stdio.h>

/* struct AoCMap; */
/* size_t offs_map_generate = 0x45EE10; */
/* size_t offs_rms_controller = 0x534C40; */
size_t offs_dropdown_add = 0x5473F0;
size_t offs_dropdown_add_inner = 0x547680;
typedef void __thiscall (*fn_dropdown_add)(void*, int, int);
typedef void __thiscall (*fn_dropdown_add_inner)(void*, int, int);

/* static fn_dropdown_add aoc_dropdown_add = 0; */
static fn_dropdown_add_inner aoc_dropdown_add_inner = 0;

int arabia_string_id = 10875;

char is_last_map_dropdown_entry(int label, int value) {
  /* Yucatan */
  return label == 10894 && value == 27;
}

__thiscall void dropdown_add_hook(void* dd, int label, int value) {
  printf("[aoe2-builtin-rms] called hooked fn %d %d!\n", label, value);
  aoc_dropdown_add_inner(dd, label, value);
  if (is_last_map_dropdown_entry(label, value)) {
    aoc_dropdown_add_inner(dd, 10912, 33);
  }
  printf("[aoe2-builtin-rms] posthook\n");
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

void install_hook (void* orig_address, void* hook_address) {
  char patch[5] = { 0xE8, 0, 0, 0, 0 };
  int offset = PtrToUlong(hook_address) - PtrToUlong(orig_address + 5);
  memcpy(&patch[1], &offset, sizeof(offset));
  printf("[aoe2-builtin-rms] installing hook at %p jmp %x (%p %p)\n", orig_address, offset, hook_address, orig_address);
  overwrite_bytes(orig_address, patch, 5);
}

__declspec(dllexport) void init() {
  printf("[aoe2-builtin-rms] init()\n");
  aoc_dropdown_add_inner = (fn_dropdown_add_inner) offs_dropdown_add_inner;
  install_hook((void*)0x4EE308, dropdown_add_hook);
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
