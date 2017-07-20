#include <stdio.h>
#include <io.h>
#include "winpty.h"

typedef struct {
  FILE *fp;
  HANDLE h;
} io_t;

DWORD WINAPI
write_thread(LPVOID arg) {
  io_t *w = (io_t*) arg;
  DWORD nwrite;

  while (!feof(w->fp)) {
    int c = fgetc(w->fp);
    if (c < 0) break;
    if (c == '\n') {
      if (!WriteFile(w->h, "\r\n", 2, &nwrite, NULL)) {
        break;
      }
    } else {
      if (!WriteFile(w->h, &c, 1, &nwrite, NULL)) {
        break;
      }
    }
  }
  return 0;
}

DWORD WINAPI
read_thread(LPVOID arg) {
  io_t *w = (io_t*) arg;
  BOOL ret;
  unsigned char buf[1024];

  while (TRUE) {
    DWORD nread = 0;
    ret = ReadFile(w->h, buf, sizeof(buf), &nread, NULL);
    if (!ret || nread == 0) {
      break;
    }
    if (fwrite(buf, nread, 1, w->fp) < 0) {
      break;
    }
    fflush(w->fp);
  }
  return 0;
}

int
main(int argc, char* argv[]) {
  winpty_error_t *err;
  winpty_config_t *config;
  winpty_t *pty;
  winpty_spawn_config_t *spawn_config;
  BOOL ret;
  HANDLE child_process_handle, child_thread_handle;
  HANDLE conout_thread_handle, conerr_thread_handle, conin_thread_handle;
  DWORD conout_thread_id, conerr_thread_id, conin_thread_id;
  DWORD error;
  HANDLE conin_handle, conout_handle, conerr_handle;
  io_t conout_io, conerr_io, conin_io;
  wchar_t *args = GetCommandLineW();
  unsigned char buf[1024];

  config = winpty_config_new(0, &err);
  if (config == NULL) {
    winpty_error_free(err);
    return 1;
  }
  winpty_error_free(err);
  winpty_config_set_initial_size(config, 80, 40);
  pty = winpty_open(config, &err);
  if (pty == NULL) {
    winpty_error_free(err);
    winpty_config_free(config);
    return 1;
  }

  if (*args == '"') {
    args++;
    while(*args != '"') args++;
  } else {
    while(*args != ' ') args++;
  }
  while(*args == ' ') args++;

  _putws(args);
  spawn_config = winpty_spawn_config_new(
    WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN | WINPTY_SPAWN_FLAG_EXIT_AFTER_SHUTDOWN,
    NULL,
    args,
    NULL,
    NULL,
    &err);
  if (spawn_config == NULL) {
    winpty_error_free(err);
    winpty_free(pty);
    winpty_config_free(config);
    return 1;
  }

  /*
  ret = winpty_set_size(pty, 80, 40, &err);
  if (!ret) {
    winpty_error_free(err);
    winpty_free(pty);
    winpty_config_free(config);
    return 1;
  }
  */

  conin_handle = CreateFileW(
        winpty_conin_name(pty),
        GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, 0, NULL);
  conout_handle = CreateFileW(
        winpty_conout_name(pty),
        GENERIC_READ, 0, NULL,
        OPEN_EXISTING, 0, NULL);
  conerr_handle = CreateFileW(
        winpty_conerr_name(pty),
        GENERIC_READ, 0, NULL,
        OPEN_EXISTING, 0, NULL);

  conout_io.h = conout_handle;
  conout_io.fp = stdout;
  conout_thread_handle = CreateThread(
    NULL, 0, read_thread, &conout_io, 0, &conout_thread_id);

  conerr_io.h = conerr_handle;
  conerr_io.fp = stderr;
  conerr_thread_handle = CreateThread(
    NULL, 0, read_thread, &conerr_io, 0, &conerr_thread_id);

  conin_io.h = conin_handle;
  conin_io.fp = stdin;
  conin_thread_handle = CreateThread(
    NULL, 0, write_thread, &conin_io, 0, &conin_thread_id);

  ret = winpty_spawn(pty, spawn_config, &child_process_handle,
      &child_thread_handle, &error, &err);
  winpty_spawn_config_free(spawn_config);
  if (!ret) {
    winpty_error_free(err);
    winpty_free(pty);
    winpty_config_free(config);
    return 1;
  }

  Sleep(200);
  WaitForSingleObject(child_process_handle, INFINITE);
  Sleep(200);

  CloseHandle(child_process_handle);
  CloseHandle(conin_handle);
  CloseHandle(conout_handle);
  CloseHandle(conerr_handle);
  winpty_free(pty);
  winpty_config_free(config);
  return 0;
}
