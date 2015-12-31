// plexmon.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "plexmon.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

HINSTANCE ThisModule() {
  return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

enum class HardFailure {
  none,
  bad_config,
  plex_error
};

struct AppException {
  HardFailure failure;
  int line;
  AppException(HardFailure failure, int line) : failure(failure), line(line) {}
};

void HardfailMsgBox(HardFailure fail, int line) {
  const char* err = nullptr;
  switch (fail) {
    case HardFailure::none: err = "none"; break;
    case HardFailure::bad_config: err = "bad config"; break;
    case HardFailure::plex_error: err = "plex error"; break;
    default: err = "(??)"; break;
  }

  auto err_text = plx::StringPrintf("Exception [%s]\nLine: %d", err, line);
  auto full_err = plx::UTF16FromUTF8(plx::RangeFromString(err_text), true);
  ::MessageBox(NULL, full_err.c_str(), L"CamCenter", MB_OK | MB_ICONEXCLAMATION);
}

struct Settings {
  std::string dropbox_root;
  std::string ping_url;
};

plx::File OpenConfigFile() {
  auto appdata_path = plx::GetAppDataPath(false);
  auto path = appdata_path.append(L"vortex\\plexmon\\config.json");
  plx::FileParams fparams = plx::FileParams::Read_SharedRead();
  return plx::File::Create(path, fparams, plx::FileSecurity());
}

Settings LoadSettings() {
  auto config = plx::JsonFromFile(OpenConfigFile());
  if (config.type() != plx::JsonType::OBJECT)
    throw plx::IOException(__LINE__, L"<unexpected json>");

  Settings settings;
  settings.dropbox_root = config["dropbox_root"].get_string();
  settings.ping_url = config["ping_url"].get_string();
  return settings;
}

uint64_t ParseUnsignedInteger(plx::Range<const uint8_t>& r) {
  size_t pos = 0;
  auto v = std::stoull(plx::StringFromRange(r), &pos);
  r.advance(pos);
  return v;
}

int64_t ParseSignedInteger(plx::Range<const uint8_t>& r) {
  size_t pos = 0;
  auto v = std::stoll(plx::StringFromRange(r), &pos);
  r.advance(pos);
  return v;
}


class Version {
  struct V {
    uint16_t major;
    uint16_t minor;
    uint16_t rev;
    uint16_t build;
  } v;

public:
  Version() : v() {}

  Version(uint16_t major, uint16_t minor, uint16_t rev, uint16_t build)
    : v({major, minor, rev, build}) {}

  static Version FromString(plx::Range<const uint8_t> r) {
    if (r.empty())
      throw plx::CodecException(__LINE__, nullptr);

    int ix = 0;
    uint16_t v[4] = {};
    while (true) {
      auto n = ParseUnsignedInteger(r);
      v[ix] = plx::To<uint16_t>(n);
      if (ix == 3)
        break;
      if (r.empty())
        break;
      if (r.front() != '.')
        throw plx::CodecException(__LINE__, &r);
      r.advance(1);
      ++ix;
    }
    return Version(v[0], v[1], v[2], v[3]);
  }

  static int Compare(const Version& l, const Version& r) {
    int x = l.v.major - r.v.major;
    if (x)
      return x;
    x = l.v.minor - r.v.minor;
    if (x)
      return x;
    x = l.v.rev - r.v.rev;
    if (x)
      return x;
    x = l.v.build - r.v.build;
    return x;
  }

};

std::string& PathComponentToUTF8(plx::Range<const wchar_t> s) {
  static std::string utf8;
  utf8.resize(250);
  // now do the conversion.
  if (!::WideCharToMultiByte(
    CP_UTF8, 0,
    s.start(),
    plx::To<int>(s.size()),
    &utf8[0],
    (int) utf8.size(),
    NULL, NULL)) {
    throw plx::CodecException(__LINE__, nullptr);
  }
  return utf8;
}


void FindHighestVersion(const plx::FilePath& dir_path) {
  auto par = plx::FileParams::Directory_ShareAll();
  plx::File dir = plx::File::Create(dir_path, par, plx::FileSecurity());
  if (!dir.is_valid())
    return;

  Version highest;

  plx::FilesInfo finf = plx::FilesInfo::FromDir(dir, 10);
  int count_dirs = 0;
  for (finf.first(); !finf.done(); finf.next()) {
    if (!finf.is_directory())
      continue;

    auto name = finf.file_name();
    auto str_ver = PathComponentToUTF8(name);
    auto version = Version::FromString(plx::RangeFromString(str_ver));

    //if (Version::Compare(version, highest))

    ++count_dirs;
  }

}


// Version::FromString(plx::RangeFromLitStr("5.22.3.1124"));

void TryUpgrade(Settings* settings) {
  auto str_ver = plx::UTF8FromUTF16(
      plx::RangeFromString(plx::GetExePath().leaf()));
  if ((str_ver == "Debug") || (str_ver == "Release"))
    return;
  auto our_ver = Version::FromString(plx::RangeFromString(str_ver));
 
}


class TopWindow : public plx::Window <TopWindow> {
public:
  TopWindow() {
    create_window(0, WS_POPUP, L"plxmon @ 2015",
      nullptr, nullptr,
      10, 10, 0, 0,
      nullptr, nullptr);
  }

  LRESULT message_handler(const UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
      case WM_DESTROY: {
        ::PostQuitMessage(0);
        return 0;
      }
    }

    return ::DefWindowProc(window(), message, wparam, lparam);
  }

};

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmdline, int cmd_show) {

  try {
    auto settings = LoadSettings();
    TryUpgrade(&settings);

    TopWindow top_window;

    MSG msg = { 0 };
    while (::GetMessage(&msg, NULL, 0, 0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    // Exit program.
    return (int)msg.wParam;

  }
  catch (plx::Exception& ex) {
    HardfailMsgBox(HardFailure::plex_error, ex.Line());
    return 2;
  }
  catch (AppException& ex) {
    HardfailMsgBox(ex.failure, ex.line);
    return 3;
  }
}

