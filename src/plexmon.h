#pragma once

#include "resource.h"

enum class HardFailure {
  none,
  bad_config,
  plex_throw,
  upgrade,
  file_io
};

enum class SoftFailure {
  generic        = 0,
  invalid_dir    = 16,
  invalid_file   = 17,
  create_failed  = 18,
  copy_failed    = 19,
  launch_failed  = 20,
  timed_out      = 21,
  pxl_exception  = 22,
};

const char* ToString(HardFailure f);

class Log {
public:
  static void init(const wchar_t* name);
  static void close();
  static void soft_fail(SoftFailure what, int line);
  static void hard_fail(HardFailure what, int line);
  static void installing(const plx::Version& v);
  static void newer_found(const plx::Version& v);
};