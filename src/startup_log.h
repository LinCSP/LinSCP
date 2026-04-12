#pragma once
// Startup diagnostics — активируется флагом -DLINSCP_STARTLOG=1 (debug workflow).
// Пишет в linscp_startup.log рядом с .exe, работает при любом subsystem.
#ifdef LINSCP_STARTLOG
#  include <cstdio>
   inline FILE *&startupLogFile() { static FILE *f = nullptr; return f; }
   inline void slog(const char *msg) {
       FILE *&f = startupLogFile();
       if (!f) f = fopen("linscp_startup.log", "w");
       if (!f) return;
       fputs(msg, f); fputc('\n', f); fflush(f);
   }

#  ifdef _WIN32
#    include <windows.h>
#    include <dbghelp.h>
#    pragma comment(lib, "dbghelp.lib")
     inline LONG WINAPI startupExceptionFilter(EXCEPTION_POINTERS *ep)
     {
         FILE *&f = startupLogFile();
         if (!f) f = fopen("linscp_startup.log", "a");
         if (f) {
             DWORD code = ep->ExceptionRecord->ExceptionCode;
             void *addr = ep->ExceptionRecord->ExceptionAddress;
             // Определяем имя модуля по адресу
             char modName[MAX_PATH] = "<unknown>";
             HMODULE hMod = nullptr;
             if (GetModuleHandleEx(
                     GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                     GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                     (LPCTSTR)addr, &hMod) && hMod)
                 GetModuleFileNameA(hMod, modName, MAX_PATH);
             fprintf(f, "[CRASH] Exception 0x%08lX at %p in %s\n",
                     (unsigned long)code, addr, modName);
             fflush(f);
         }
         return EXCEPTION_CONTINUE_SEARCH; // передать дальше → WER создаст дамп
     }
     inline void installCrashHandler() {
         SetUnhandledExceptionFilter(startupExceptionFilter);
     }
#  else
     inline void installCrashHandler() {}
#  endif

#else
   inline void slog(const char *) {}
   inline void installCrashHandler() {}
#endif
