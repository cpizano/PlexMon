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

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmdline, int cmd_show) {

  try {
    //auto settings = LoadSettings();

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

