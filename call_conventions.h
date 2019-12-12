#pragma once

#ifdef _MSC_VER

#define CDECL_PTR(ReturnType, ...) (ReturnType(__cdecl*)(__VA_ARGS__))
#define CDECL(Name, ...) __cdecl Name(__VA_ARGS__)

#define THISCALL_PTR(ReturnType, ThisArg, ...)                                 \
  (ReturnType(__fastcall*)(ThisArg, void*, ##__VA_ARGS__))
#define THISCALL_TYPEDEF(Name, ReturnType, ThisArg, ...)                       \
  typedef ReturnType (__fastcall *Name)(ThisArg, void*, ##__VA_ARGS__)
#define THISCALL(Name, ThisArg, ...)                                           \
  __fastcall Name(ThisArg, void* THISCALL_DUMMY_DONOTUSE, ##__VA_ARGS__)
#define THISCALL_CALL(Name, ThisArg, ...)                                      \
  Name(ThisArg, NULL, ##__VA_ARGS__)

#else

#define CDECL_PTR(ReturnType, ...) (ReturnType __cdecl(*)(__VA_ARGS__))
#define CDECL(Name, ...) __cdecl Name(__VA_ARGS__)

#define THISCALL_PTR(ReturnType, ThisArg, ...)                                 \
  (ReturnType __thiscall(*)(ThisArg, ##__VA_ARGS__))
#define THISCALL_TYPEDEF(Name, ReturnType, ThisArg, ...)                       \
  typedef ReturnType __thiscall(*Name)(ThisArg, ##__VA_ARGS__)
#define THISCALL(Name, ThisArg, ...) __thiscall Name(ThisArg, ##__VA_ARGS__)
#define THISCALL_CALL(Name, ThisArg, ...) Name(ThisArg, ##__VA_ARGS__)

#endif
