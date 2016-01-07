// plexlog.cpp.
//

#include "stdafx.h"

#include <time.h>
#include "plexmon.h"

#define spf plx::StringPrintf

const char* ToString(SoftFailure f) {
  using sf = SoftFailure;
  switch (f) {
    case sf::invalid_dir:     return "[invalid dir]";
    case sf::invalid_file:    return "[invalid file]";
    case sf::create_failed:   return "[create failed]";
    case sf::copy_failed:     return "[copy faile]";
    case sf::launch_failed:   return "[launch failed]";
    case sf::timed_out:       return "[timed out]";
    case sf::pxl_exception:   return "[plx exception]";
  }
  return "[?]";
}

const char* ToString(HardFailure f) {
  switch (f) {
    case HardFailure::none:           return "[none]";
    case HardFailure::bad_config:     return "[bad config]";
    case HardFailure::plex_throw:     return "[plex throw]";
    case HardFailure::upgrade:        return "[upgrade]";
    case HardFailure::file_io:        return "[file io]";
  }
  return "[?]";
}

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
      throw plx::IOException(__LINE__, path.raw());
  }

  void add(const std::string& line) {
    file_.write(plx::RangeFromString(line));
  }

  void add_now_readable() {
    time_t ltime;
    time(&ltime);
    char buf[26];
    ctime_s(buf, sizeof(buf), &ltime);
    add(buf);
  }

  unsigned long ts() const {
    auto d = ::GetTickCount64() - tick_;
    return static_cast<unsigned long>(d / 100);
  }
};

static Logger* elg = nullptr;

void Log::init(const wchar_t * name) {
  if (elg)
    __debugbreak();

  auto appdata_path = plx::GetAppDataPath(false);
  auto path = appdata_path.append(name);
  elg = new Logger(path);
  elg->add("\n@ plex log <plexmon> [0.1] " __DATE__ " now : ");
  elg->add_now_readable();
  elg->add(spf("%lu pid %d tid %d tick %llu\n", elg->ts(),
      ::GetCurrentProcessId(), ::GetCurrentThreadId(), ::GetTickCount64()));
}

void Log::close() {
  if (!elg)
    return;
  elg->add(spf("%lu endlog pid %d\n",
      elg->ts(), ::GetCurrentProcessId()));
  elg->add("@ log end\n");
  delete elg;
  elg = nullptr;
}

//  elg->add(spf("%lu xx \n", elg->ts()));

void Log::soft_fail(SoftFailure what, int line) {
  elg->add(spf("%lu soft_fail line %d issue %s \n", elg->ts(), line, ToString(what)));
}

void Log::hard_fail(HardFailure what, int line) {
  elg->add(spf("%lu hard_fail line %d issue %s \n", elg->ts(), line, ToString(what)));
}

void Log::installing(const plx::Version & v) {
  elg->add(spf("%lu installing ver %s\n", elg->ts(), v.to_string().c_str()));
}

void Log::newer_found(const plx::Version & v) {
  elg->add(spf("%lu newer_found ver %s\n", elg->ts(), v.to_string().c_str()));
}


