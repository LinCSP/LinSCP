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

     // ── helpers ──────────────────────────────────────────────────────────────

     // Basename of a full path (last component after \ or /).
     inline const char *pathBaseName(const char *path) {
         const char *b = path;
         for (const char *p = path; *p; ++p)
             if (*p == '\\' || *p == '/') b = p + 1;
         return b;
     }

     // Fill modName (MAX_PATH) and return ASLR-independent offset of addr within its module.
     // Returns 0 and leaves modName="<unknown>" on failure.
     inline DWORD64 moduleOffset(void *addr, char modName[MAX_PATH]) {
         strcpy(modName, "<unknown>");
         HMODULE hm = nullptr;
         if (!GetModuleHandleExA(
                 GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                 GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                 (LPCSTR)addr, &hm) || !hm)
             return 0;
         GetModuleFileNameA(hm, modName, MAX_PATH);
         DWORD64 base = SymGetModuleBase64(GetCurrentProcess(),
                                           (DWORD64)(DWORD_PTR)addr);
         if (base == 0)
             base = (DWORD64)(DWORD_PTR)hm;   // fallback: use load address
         return (DWORD64)(DWORD_PTR)addr - base;
     }

     inline LONG WINAPI startupExceptionFilter(EXCEPTION_POINTERS *ep)
     {
         FILE *&f = startupLogFile();
         if (!f) f = fopen("linscp_startup.log", "a");
         if (!f) return EXCEPTION_CONTINUE_SEARCH;

         HANDLE hProc   = GetCurrentProcess();
         HANDLE hThread = GetCurrentThread();

         // ── crash address ────────────────────────────────────────────────────
         DWORD  code = ep->ExceptionRecord->ExceptionCode;
         void  *addr = ep->ExceptionRecord->ExceptionAddress;
         char   mod[MAX_PATH];
         DWORD64 off = moduleOffset(addr, mod);
         fprintf(f, "[CRASH] Exception 0x%08lX at +0x%016llX in %s\n",
                 (unsigned long)code, (unsigned long long)off, pathBaseName(mod));

         // ── symbol engine ────────────────────────────────────────────────────
         SymInitialize(hProc, NULL, TRUE);
         SymSetOptions(SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);

         // ── stack walk from crash context ────────────────────────────────────
         CONTEXT ctx = *ep->ContextRecord;
         STACKFRAME64 sf = {};
         DWORD machine;
#    if defined(_M_X64)
         machine              = IMAGE_FILE_MACHINE_AMD64;
         sf.AddrPC.Offset     = ctx.Rip;
         sf.AddrFrame.Offset  = ctx.Rbp;
         sf.AddrStack.Offset  = ctx.Rsp;
#    elif defined(_M_IX86)
         machine              = IMAGE_FILE_MACHINE_I386;
         sf.AddrPC.Offset     = ctx.Eip;
         sf.AddrFrame.Offset  = ctx.Ebp;
         sf.AddrStack.Offset  = ctx.Esp;
#    else
         machine              = IMAGE_FILE_MACHINE_AMD64;
         sf.AddrPC.Offset     = ctx.Rip;
         sf.AddrFrame.Offset  = ctx.Rbp;
         sf.AddrStack.Offset  = ctx.Rsp;
#    endif
         sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;

         fputs("[STACK]\n", f);
         for (int i = 0; i < 48; ++i) {
             if (!StackWalk64(machine, hProc, hThread, &sf, &ctx,
                              NULL, SymFunctionTableAccess64,
                              SymGetModuleBase64, NULL))
                 break;
             if (sf.AddrPC.Offset == 0) break;

             void   *pc    = (void*)(DWORD_PTR)sf.AddrPC.Offset;
             char    fm[MAX_PATH];
             DWORD64 foff  = moduleOffset(pc, fm);

             // Try to resolve symbol name
             alignas(SYMBOL_INFO) char symBuf[sizeof(SYMBOL_INFO) + 256];
             SYMBOL_INFO *sym = reinterpret_cast<SYMBOL_INFO*>(symBuf);
             sym->SizeOfStruct = sizeof(SYMBOL_INFO);
             sym->MaxNameLen   = 256;
             DWORD64 disp = 0;
             if (SymFromAddr(hProc, sf.AddrPC.Offset, &disp, sym) && sym->Name[0])
                 fprintf(f, "  [%2d] +0x%016llX  %s+0x%llX  (%s)\n",
                         i, (unsigned long long)foff,
                         sym->Name, (unsigned long long)disp,
                         pathBaseName(fm));
             else
                 fprintf(f, "  [%2d] +0x%016llX  (%s)\n",
                         i, (unsigned long long)foff, pathBaseName(fm));
         }

         // ── minidump ─────────────────────────────────────────────────────────
         HANDLE hDmp = CreateFileA("linscp_crash.dmp", GENERIC_WRITE, 0, NULL,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
         if (hDmp != INVALID_HANDLE_VALUE) {
             MINIDUMP_EXCEPTION_INFORMATION mei = {GetCurrentThreadId(), ep, FALSE};
             MiniDumpWriteDump(hProc, GetCurrentProcessId(), hDmp,
                               MiniDumpWithDataSegs, &mei, NULL, NULL);
             CloseHandle(hDmp);
             fputs("[CRASH] Minidump written: linscp_crash.dmp\n", f);
         }

         fflush(f);
         return EXCEPTION_CONTINUE_SEARCH;   // pass to WER → creates full dump
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
