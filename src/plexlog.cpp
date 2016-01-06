// plexlog.cpp.
//

#include "stdafx.h"
#include "plexmon.h"

#define spf plx::StringPrintf

class Logger {
  plx::File file_;
  unsigned long long tick_;

public:
  Logger(const plx::FilePath path)
    : file_(plx::File::Create(
          path, plx::FileParams::Append_SharedRead(),
          plx::FileSecurity())),
      tick_(::GetTickCount64()) {
    if (!file_.is_valid())
      throw AppException(HardFailure::file_io, __LINE__);
  }

  void add(const std::string& line) {
    file_.write(plx::RangeFromString(line));
  }

  unsigned long ts() const {
    auto d = ::GetTickCount64() - tick_;
    return static_cast<unsigned long>(d / 100);
  }
};

static Logger* elg = nullptr;

void Log::init(const wchar_t * name) {
  auto appdata_path = plx::GetAppDataPath(false);
  auto path = appdata_path.append(name);
  elg = new Logger(path);
  elg->add("\n@ Plex log <plexmon> [0.1] " __DATE__ "\n");
  elg->add(spf("%lu pid %d tid %d tick %llu\n",
      ::GetCurrentProcessId(), ::GetCurrentThreadId(), ::GetTickCount64()));
}

void Log::close() {
  if (!elg)
    return;
  elg->add(spf("%lu endlog pid %d\n",
      elg->ts(), ::GetCurrentProcessId()));
  elg->add("@ log end\n");
  delete elg;
}


