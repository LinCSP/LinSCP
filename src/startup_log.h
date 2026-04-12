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
#else
   inline void slog(const char *) {}
#endif
