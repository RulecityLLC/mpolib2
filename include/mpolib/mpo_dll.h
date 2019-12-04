// mpo_dll.h
// by Matt Ownby

// contains macros to make DLL compiling under win32 convenient

// if this is to be compiled as a win32 DLL
#ifdef MPOLIB_USRDLL

#ifdef MPOLIB_EXPORTS
#define EXPORT_ME __declspec(dllexport)
#else
#define EXPORT_ME __declspec(dllimport)
#endif

#else

// else make the macro empty (for static linking, unix, etc)
#define EXPORT_ME

#endif // _USRDLL
