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

