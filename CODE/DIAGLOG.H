/*
** RA1 diagnostic logging interface.
**
** This deliberately uses the legacy C runtime so it remains compatible with
** the original Watcom build and older Windows versions.
*/

#ifndef RA_DIAGLOG_H
#define RA_DIAGLOG_H

#define RA_DIAG_LOG_TRACE 0
#define RA_DIAG_LOG_INFO  1
#define RA_DIAG_LOG_WARN  2
#define RA_DIAG_LOG_ERROR 3
#define RA_DIAG_LOG_FATAL 4

/*
** Creates/overwrites RA_DIAG.LOG in the game's working directory.
** Every message is flushed immediately so the final breadcrumb usually
** survives a crash.
*/
void RA_DiagLog_Initialize(void);
void RA_DiagLog_Write(int level, char const *format, ...);
void RA_DiagLog_Flush(void);
void RA_DiagLog_Close(void);

/* Installs a best-effort Win32 unhandled-exception breadcrumb writer. */
void RA_DiagLog_InstallCrashHandler(void);

#endif /* RA_DIAGLOG_H */
