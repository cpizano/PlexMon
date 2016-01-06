#pragma once

#include "resource.h"

enum class HardFailure {
  none,
  bad_config,
  plex_error,
  upgrade,
  file_io
};

struct AppException {
  HardFailure failure;
  int line;
  AppException(HardFailure failure, int line) : failure(failure), line(line) {}
};


class Log {
public:
  static void init(const wchar_t* name);
  static void close();

};