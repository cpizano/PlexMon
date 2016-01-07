// plexmon.cpp.
//

#include "stdafx.h"
#include "plexmon.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

const wchar_t job_obj_name[] = L"plxmon@vtx";
const wchar_t install_pipe[] = L"plxmon@ins";

HINSTANCE ThisModule() {
  return reinterpret_cast<HINSTANCE>(&__ImageBase);
}

struct AppException {
  HardFailure failure;
  int line;
  AppException(HardFailure failure, int line) : failure(failure), line(line) {}
};

void HardfailMsgBox(HardFailure fail, int line) {
  auto err = ToString(fail);
  Log::hard_fail(fail, line);
  auto err_text = plx::StringPrintf("Exception [%s]\nLine: %d", err, line);
  auto full_err = plx::UTF16FromUTF8(plx::RangeFromString(err_text), true);
  ::MessageBox(NULL, full_err.c_str(), L"plexmon", MB_OK | MB_ICONEXCLAMATION);
}

struct Settings {
  plx::FilePath dropbox_root;
  std::string ping_url;

  Settings(std::wstring dropbox_root) : dropbox_root(dropbox_root) {}
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

  auto db_str = config["dropbox_root"].get_string();
  Settings settings(plx::UTF16FromUTF8(plx::RangeFromString(db_str).bytes(), true));
  return settings;
}

bool MigrateSettings(Settings* settings) {
  // make config copy and write out new config.
  return true;
}

std::wstring WideFromString(const std::string& str) {
  return std::wstring(begin(str), end(str));
}

bool FindHighestVersion(const plx::FilePath& dir_path, plx::Version* ver) {
  auto par = plx::FileParams::Directory_ShareAll();
  plx::File dir = plx::File::Create(dir_path, par, plx::FileSecurity());
  if (!dir.is_valid()) {
    Log::soft_fail(SoftFailure::invalid_dir, __LINE__);
    return false;
  }

  plx::Version highest;

  plx::FilesInfo finf = plx::FilesInfo::FromDir(dir, 10);
  int count_dirs = 0;
  for (finf.first(); !finf.done(); finf.next()) {
    if (!finf.is_directory())
      continue;
    if (finf.file_name().empty())
      continue;
    if (finf.file_name()[0] == '.')
      continue;
    if (std::isalpha(finf.file_name()[0]))
      continue;
    auto name = plx::StringFromRange(finf.file_name());
    auto version = plx::Version::FromRange(plx::RangeFromString(name));
    if (plx::Version::Compare(version, highest) > 0)
      highest = version;
    ++count_dirs;
  }

  if (!count_dirs)
    return false;

  *ver = highest;
  return true;
}

bool IsDeveloperBuild(const std::string& last_path) {
  return (last_path == "Debug") || (last_path == "Release");
}

plx::FilePath PlexmonExe(const plx::FilePath& path) {
  return path.append(L"plexmon.exe");
}

bool ValidPlexmonDir(const plx::FilePath& path) {
  auto op = plx::FileParams::Read_SharedRead();
  auto exe = plx::File::Create(PlexmonExe(path), op, plx::FileSecurity());
  if (!exe.is_valid()) {
    Log::soft_fail(SoftFailure::invalid_file, __LINE__);
    return false;
  }
  auto what = plx::File::Create(path.append(L".what"), op, plx::FileSecurity());
  if (!what.is_valid()) {
    Log::soft_fail(SoftFailure::invalid_file, __LINE__);
    return false;
  }
  return true;
}

bool LaunchPlexmonInstall(const plx::FilePath& path) {
  plx::ProcessParams pp(false, 0);
  plx::Process process = plx::Process::Create(PlexmonExe(path), L"--install", pp);
  return process.is_valid();
}

void IOCPRunner(plx::CompletionPort* cp, unsigned int timeout) {
  while (true) {
    auto res = cp->wait_for_io_op(timeout);
    if (res != plx::CompletionPort::op_ok)
      return;
  }
}

class NewVersionHandshake : public plx::OverlappedChannelHandler {
  plx::ServerPipe* srv_pipe_;
  plx::CompletionPort* cp_;
  std::thread* th_;

  bool success_;

  struct IPC {
    uint8_t buf[8];
    IPC() { memset(buf, 0, sizeof(buf)); }

    void set() { memcpy(buf, "alive001", sizeof(buf)); }
    bool chk() { return memcmp(buf, "alive001", sizeof(buf)) == 0; }
  };

public:
  NewVersionHandshake() 
    : srv_pipe_(nullptr), cp_(nullptr), th_(nullptr), success_(false) {}

  bool begin_old() {
    cp_ = new plx::CompletionPort(2);
    srv_pipe_ = new plx::ServerPipe(plx::ServerPipe::Create(
        install_pipe, plx::ServerPipe::overlapped));
    srv_pipe_->associate_cp(cp_, this);
    th_ = new std::thread(IOCPRunner, cp_, 5000);
    return srv_pipe_->connect(new IPC());
  }

  bool end_old() {
    th_->join();
    delete th_;
    delete srv_pipe_;
    delete cp_;
    return success_;
  }

  void cancel_old() {
    srv_pipe_->disconnect();
    cp_->release_waiter();
    end_old();
  }

  bool start_new() {
    auto params = plx::FileParams::ReadWrite_SharedRead(OPEN_EXISTING);
    auto pipe = plx::File::Create(
      plx::FilePath::for_pipe(install_pipe), params, plx::FileSecurity());
    if (!pipe.is_valid())
      return false;
    IPC ipc;  ipc.set();
    auto sz = pipe.write(plx::RangeFromArray(ipc.buf));
    return (sz == sizeof(ipc.buf));
  }

  void OnConnect(plx::OverlappedContext* ovc, unsigned long error) override {
    if (error)
      return;
    auto ipc = reinterpret_cast<IPC*>(ovc->ctx);
    auto m = plx::RangeFromArray(ipc->buf);
    if (!srv_pipe_->read(m, ipc))
      throw AppException(HardFailure::upgrade, __LINE__);
  }

  void OnRead(plx::OverlappedContext* ovc, unsigned long error) override {
    if (error)
      return;
    auto ipc = reinterpret_cast<IPC*>(ovc->ctx);
    success_ = ipc->chk();
    delete ipc;
    if (!srv_pipe_->disconnect())
      throw AppException(HardFailure::upgrade, __LINE__);
    cp_->release_waiter();
  }

  void OnWrite(plx::OverlappedContext* ovc, unsigned long error) override {
  }
};

plx::Version GetSelfVersion() {
  static std::string str_ver = plx::UTF8FromUTF16(
    plx::RangeFromString(plx::GetExePath().leaf()));

  return IsDeveloperBuild(str_ver) ?
    plx::Version(0, 0, 0, 10) :
    plx::Version::FromRange(plx::RangeFromString(str_ver));
}

bool TryUpgrade(Settings* settings) {
  auto our_version = GetSelfVersion();

  plx::Version newest_version;
  auto db_plxmon_path = plx::FilePath(
      settings->dropbox_root).append(L"vortex\\plexmon");
  if (!FindHighestVersion(db_plxmon_path, &newest_version))
    return false;

  if (plx::Version::Compare(newest_version, our_version) <= 0)
    return false;

  Log::newer_found(newest_version);

  auto new_leaf = WideFromString(newest_version.to_string());
  plx::FilePath new_db_dir(db_plxmon_path.append(new_leaf));

  if (!ValidPlexmonDir(new_db_dir))
    return false;

  auto install_dir = plx::GetExePath().parent().append(new_leaf);
  if (!::CreateDirectoryW(install_dir.raw(), NULL)) {
    if (::GetLastError() != ERROR_ALREADY_EXISTS) {
      Log::soft_fail(SoftFailure::create_failed, __LINE__);
      return false;
    }
  }

  if (!::CopyFileW(PlexmonExe(new_db_dir).raw(),
                   PlexmonExe(install_dir).raw(), TRUE)) {
    Log::soft_fail(SoftFailure::copy_failed, __LINE__);
    return false;
  }

  NewVersionHandshake handshake;
  handshake.begin_old();
 
  if (!LaunchPlexmonInstall(install_dir)) {
    handshake.cancel_old();
    Log::soft_fail(SoftFailure::launch_failed, __LINE__);
    return false;
  }

  if (!handshake.end_old()) {
    Log::soft_fail(SoftFailure::timed_out, __LINE__);
    return false;
  }
  return true;
}

bool InstallSelf() {
  Log::init(L"vortex\\plexmon\\install_log.txt");
  Log::installing(GetSelfVersion());

  try {
    auto settings = LoadSettings();
    MigrateSettings(&settings);
  }
  catch (plx::Exception& ex) {
    Log::soft_fail(SoftFailure::pxl_exception, ex.Line());
    Log::close();
    return false;
  }

  NewVersionHandshake handshake;
  auto rv = handshake.start_new();
  Log::close();
  return rv;
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

class JobObjHandler : public plx::JobObjEventHandler {
public:
  std::vector<unsigned int> new_pids;
  std::vector<unsigned int> dead_pids;

  void AbnormalExit(unsigned int pid, unsigned long error) override {
    dead_pids.push_back(pid);
  }
  void NormalExit(unsigned int pid, unsigned long status) override {
    dead_pids.push_back(pid);
  }
  void NewProcess(unsigned int pid) override {
    new_pids.push_back(pid);
  }
  void ActiveCountZero() override {
  }
  void ActiveProcessLimit() override {
  }
  void MemoryLimit(unsigned int pid) {
  }
  void TimeLimit(unsigned int pid) {
  }
};

void JobThread(plx::CompletionPort* cp) {
  JobObjHandler job_handler;
  plx::JobObjecNotification runner(cp, &job_handler);

  plx::JobObject job = plx::JobObject::Create(
      job_obj_name, plx::JobObjectLimits(),&runner);

  while (true) {
    auto rv = runner.wait_for_event(INFINITE);
    if (rv == plx::CompletionPort::op_exit)
      break;
  }
}

int __stdcall wWinMain(HINSTANCE instance, HINSTANCE, wchar_t* cmdline, int cmd_show) {
  int rv = 0;

  try {
    int argc = 0;
    auto argv = CommandLineToArgvW(cmdline, &argc);
    plx::CmdLine cmd(argc, argv);

    if (cmd.has_switch(L"install")) {
      if (!InstallSelf()) {
        return 0;
      }
    }

    Log::init(L"vortex\\plexmon\\op_log.txt");

    auto settings = LoadSettings();
    if (TryUpgrade(&settings))
      return 0;

    plx::CompletionPort job_cp(1);
    std::thread job_thread(JobThread, &job_cp);

    TopWindow top_window;
    MSG msg = { 0 };
    while (::GetMessage(&msg, NULL, 0, 0)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }

    job_cp.release_waiter();
    job_thread.join();

    rv = (int) msg.wParam;
  }
  catch (AppException& ex) {
    HardfailMsgBox(ex.failure, ex.line);
  }
  catch (plx::Exception& ex) {
    HardfailMsgBox(HardFailure::plex_throw, ex.Line());
  } 

  Log::close();
  return rv;
}
