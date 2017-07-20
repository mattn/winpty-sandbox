#include <stdio.h>
#include "winpty.h"

int
main(int argc, char* argv[]) {
  winpty_error_t *err;
  winpty_config_t *config;
  winpty_t *pty;
  winpty_spawn_config_t *spawn_config;
  BOOL ret;
  HANDLE child;
  HANDLE thread;
  DWORD error;
  HANDLE conin, conout, conerr;
  unsigned char buf[1024];

  config = winpty_config_new(0, &err);
  if (config == NULL) {
    winpty_error_free(err);
    return 1;
  }
  winpty_error_free(err);
  winpty_config_set_initial_size(config, 40, 40);
  pty = winpty_open(config, &err);
  if (pty == NULL) {
    winpty_error_free(err);
    winpty_config_free(config);
    return 1;
  }

  spawn_config = winpty_spawn_config_new(
    WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
    NULL,
    L"ls -la",
    NULL,
    NULL,
    &err);
  if (spawn_config == NULL) {
    winpty_error_free(err);
    winpty_free(pty);
    winpty_config_free(config);
    return 1;
  }

  ret = winpty_spawn(pty, spawn_config, &child, &thread, &error, &err);
  winpty_spawn_config_free(spawn_config);
  if (ret == FALSE) {
    winpty_error_free(err);
    winpty_free(pty);
    winpty_config_free(config);
    return 1;
  }

  ret = winpty_set_size(pty, 80, 40, &err);
  if (ret == FALSE) {
    winpty_error_free(err);
    winpty_free(pty);
    winpty_config_free(config);
    return 1;
  }
  conin = CreateFileW(
        winpty_conin_name(pty),
        GENERIC_WRITE, 0, NULL,
        OPEN_EXISTING, 0, NULL);
  conout = CreateFileW(
        winpty_conout_name(pty),
        GENERIC_READ, 0, NULL,
        OPEN_EXISTING, 0, NULL);
  conerr = CreateFileW(
        winpty_conerr_name(pty),
        GENERIC_READ, 0, NULL,
        OPEN_EXISTING, 0, NULL);

  while (TRUE) {
    DWORD nread = 0;
    ret = ReadFile(conout, buf, sizeof(buf), &nread, NULL);
    if (ret == FALSE || nread == 0) {
      break;
    }
	puts(buf);
  }

  CloseHandle(child);
  CloseHandle(conin);
  CloseHandle(conout);
  winpty_free(pty);
  winpty_config_free(config);
  return 0;
}
