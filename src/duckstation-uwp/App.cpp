#define NOMINMAX

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Composition.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.ViewManagement.Core.h>
#include <winrt/Windows.Graphics.Display.Core.h>
#include <winrt/Windows.Gaming.Input.h>
#include <winrt/Windows.System.h>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include "core/achievements.h"
#include "core/fullscreen_ui.h"
#include "core/game_list.h"
#include "core/host.h"
#include "core/settings.h"
#include "core/system.h"
#include "common/crash_handler.h"
#include "common/error.h"
#include "common/file_system.h"
#include "common/log.h"
#include "common/path.h"
#include "common/threading.h"
#include "util/imgui_manager.h"
#include "util/ini_settings_interface.h"
#include "util/input_manager.h"
#include "util/platform_misc.h"

#include "UWPUtils.h"

#include <thread>
#include <chrono>

#include "win32_key_names.h"


// Entrypoint into the emulator
#include "duckstation-nogui/nogui_host.h"
#include "winrt_nogui_platform.h"
#include <util/imgui_fullscreen.h>

Log_SetChannel(App);

using namespace winrt;

using namespace winrt::Windows;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::Graphics::Display::Core;
using namespace winrt::Windows::Foundation::Numerics;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Composition;

static constexpr u32 SETTINGS_VERSION = 3;
static constexpr auto CPU_THREAD_POLL_INTERVAL = std::chrono::milliseconds(8); // how often we'll poll controllers when paused

namespace WinRTHost {
// this sucks. a necessary evil, i suppose...
// why couldn't they have just stuck with fullscreen.h?
class AsyncOpProgressCallback final : public BaseProgressCallback
{
public:
  AsyncOpProgressCallback(std::string name);
  ~AsyncOpProgressCallback() override;

  ALWAYS_INLINE const std::string& GetName() const { return m_name; }

  void PushState() override;
  void PopState() override;

  void SetCancellable(bool cancellable) override;
  void SetTitle(const char* title) override;
  void SetStatusText(const char* text) override;
  void SetProgressRange(u32 range) override;
  void SetProgressValue(u32 value) override;

  void DisplayError(const char* message) override;
  void DisplayWarning(const char* message) override;
  void DisplayInformation(const char* message) override;
  void DisplayDebugMessage(const char* message) override;

  void ModalError(const char* message) override;
  bool ModalConfirmation(const char* message) override;
  void ModalInformation(const char* message) override;

  void SetCancelled();

private:
  void Redraw(bool force);

  std::string m_name;
  int m_last_progress_percent = -1;
};

  static bool InitializeConfig();
  static bool InBatchMode();
  static std::optional<WindowInfo> GetPlatformWindowInfo();

  static std::string GetResourcePath(std::string_view name, bool allow_override);

  static void StartCPUThread();
  static void StopCPUThread();
  static void CPUThreadEntryPoint();
  static void CPUThreadMainLoop();
  static void StartAsyncOp(std::function<void(ProgressCallback*)> callback);
  static void CancelAsyncOp();
  static void AsyncOpThreadEntryPoint(std::function<void(ProgressCallback*)> callback);
  static void ProcessCPUThreadEvents(bool block);
  static void SaveSettings(); 
  static void SetXboxSettings(INISettingsInterface& si);

  //////////////////////////////////////////////////////////////////////////
  // Local variable declarations
  //////////////////////////////////////////////////////////////////////////
  static winrt::Windows::UI::Core::CoreWindow* s_corewind = NULL;

  static std::unique_ptr<INISettingsInterface> s_base_settings_interface;
  static bool s_batch_mode = false;
  static bool s_is_fullscreen = false;
  static bool s_was_paused_by_focus_loss = false;

  static Threading::Thread s_cpu_thread;
  static Threading::KernelSemaphore s_platform_window_updated;
  static std::atomic_bool s_running{false};
  static std::mutex s_cpu_thread_events_mutex;
  static std::condition_variable s_cpu_thread_event_done;
  static std::condition_variable s_cpu_thread_event_posted;
  static std::deque<std::pair<std::function<void()>, bool>> s_cpu_thread_events;
  static u32 s_blocking_cpu_events_pending = 0; // TODO: Token system would work better here.

  static std::mutex s_async_op_mutex;
  static std::thread s_async_op_thread;
  static std::string s_uwpPath;
  static AsyncOpProgressCallback* s_async_op_progress = nullptr;

  static bool is_running_on_xbox;
  } // namespace WinRTHost

// if the program is running on an Xbox console, set Xbox specific options
void WinRTHost::SetXboxSettings(INISettingsInterface& si)
{
  if (!is_running_on_xbox) { return; };
  si.SetStringValue("GPU", "Renderer", "D3D12");

  si.SetBoolValue("Main", "SyncToHostRefreshRate", true);
  si.SetStringValue("Display", "SyncMode", "VSync");
  si.SetBoolValue("Display", "DisplayAllFrames", true);
  si.SetFloatValue("Display", "MaxFPS", 60.0f);

  si.SetStringValue("CPU", "FastmemMode", "LUT");
  si.SetStringValue("Main", "ControllerBackend", "XInput");

  // Set up an analog controller in port 1.
  si.SetStringValue("Pad1", "Type",       "AnalogController");
  si.SetStringValue("Pad1", "Up",         "XInput-0/DPadUp");
  si.SetStringValue("Pad1", "Down",       "XInput-0/DPadDown");
  si.SetStringValue("Pad1", "Left",       "XInput-0/DPadLeft");
  si.SetStringValue("Pad1", "Right",      "XInput-0/DPadRight");
  si.SetStringValue("Pad1", "Select", "XInput-0/Back");
  si.SetStringValue("Pad1", "Start",      "XInput-0/Start");
  si.SetStringValue("Pad1", "Triangle",   "XInput-0/Y");
  si.SetStringValue("Pad1", "Cross",      "XInput-0/A");
  si.SetStringValue("Pad1", "Circle",     "XInput-0/B");
  si.SetStringValue("Pad1", "Square",     "XInput-0/X");
  si.SetStringValue("Pad1", "L1",         "XInput-0/LeftShoulder");
  si.SetStringValue("Pad1", "L2",         "XInput-0/+LeftTrigger");
  si.SetStringValue("Pad1", "R1",         "XInput-0/RightShoulder");
  si.SetStringValue("Pad1", "R2",         "XInput-0/+RightTrigger");
  si.SetStringValue("Pad1", "L3",         "XInput-0/LeftStick");
  si.SetStringValue("Pad1", "R3",         "XInput-0/RightStick");
  si.SetStringValue("Pad1", "LLeft",      "XInput-0/-LeftX");
  si.SetStringValue("Pad1", "LRight",     "XInput-0/+LeftX");
  si.SetStringValue("Pad1", "LDown",      "XInput-0/+LeftY");
  si.SetStringValue("Pad1", "LUp",        "XInput-0/-LeftY");
  si.SetStringValue("Pad1", "RLeft",      "XInput-0/-RightX");
  si.SetStringValue("Pad1", "RRight",     "XInput-0/+RightX");
  si.SetStringValue("Pad1", "RDown",      "XInput-0/+RightY");
  si.SetStringValue("Pad1", "RUp",        "XInput-0/-RightY");
  si.SetStringValue("Pad1", "SmallMotor", "XInput-0/SmallMotor");
  si.SetStringValue("Pad1", "LargeMotor", "XInput-0/LargeMotor");

  // we have chords!
  si.SetStringValue("Hotkeys", "OpenPauseMenu", "XInput-0/LeftStick & XInput-0/RightStick");
}

bool WinRTHost::InitializeConfig()
{
  // TODO: Might need thise
  std::string settings_filename = Path::Combine(EmuFolders::DataRoot, "settings.ini");

  Log_InfoPrintf("Loading config from %s.", settings_filename.c_str());
  s_base_settings_interface = std::make_unique<INISettingsInterface>(std::move(settings_filename));
  Host::Internal::SetBaseSettingsLayer(s_base_settings_interface.get());

  u32 settings_version;
  if (!s_base_settings_interface->Load() ||
      !s_base_settings_interface->GetUIntValue("Main", "SettingsVersion", &settings_version) ||
      settings_version != SETTINGS_VERSION)
  {
    if (s_base_settings_interface->ContainsValue("Main", "SettingsVersion"))
    {
      // NOTE: No point translating this, because there's no config loaded, so no language loaded.
      Host::ReportErrorAsync("Error", fmt::format("Settings version {} does not match expected version {}, resetting.",
                                                  settings_version, SETTINGS_VERSION));
    }

    s_base_settings_interface->SetUIntValue("Main", "SettingsVersion", SETTINGS_VERSION);

    ::System::SetDefaultSettings(*s_base_settings_interface);
    EmuFolders::SetDefaults();
    EmuFolders::Save(*s_base_settings_interface);

	InputManager::SetDefaultSourceConfig(*s_base_settings_interface);
    Settings::SetDefaultControllerConfig(*s_base_settings_interface);
    Settings::SetDefaultHotkeyConfig(*s_base_settings_interface);

    // set up xbox settings after we have done all this stuff
    // otherwise, keyboard will override the Xbox controller, which sucks!
    SetXboxSettings(*s_base_settings_interface);
  }
    
  s_base_settings_interface->Save();

  EmuFolders::LoadConfig(*s_base_settings_interface.get());
  EmuFolders::EnsureFoldersExist();

  // We need to create the console window early, otherwise it appears behind the main window.
  if (!Log::IsConsoleOutputEnabled() &&
      s_base_settings_interface->GetBoolValue("Logging", "LogToConsole", Settings::DEFAULT_LOG_TO_CONSOLE))
  {
    Log::SetConsoleOutputParams(true, s_base_settings_interface->GetBoolValue("Logging", "LogTimestamps", true));
  }

  return true;
}

bool WinRTHost::InBatchMode()
{
    return s_batch_mode;
}

std::optional<WindowInfo> WinRTHost::GetPlatformWindowInfo()
{
    WindowInfo wi;

    if (s_corewind)
    {
      u32 width = 1920, height = 1080;
      float scale = 1.0;
      if (is_running_on_xbox)
      {
        HdmiDisplayInformation hdi = HdmiDisplayInformation::GetForCurrentView();
        if (hdi)
        {
          width = hdi.GetCurrentDisplayMode().ResolutionWidthInRawPixels();
          height = hdi.GetCurrentDisplayMode().ResolutionHeightInRawPixels();
          // Our UI is based on 1080p, and we're adding a modifier to zoom in by 80%
          scale = ((float)width / 1920.0f) * 1.8f;
        }
      }

      wi.surface_width = width;
      wi.surface_height = height;
      wi.surface_scale = 1.0f;
      wi.type = WindowInfo::Type::Win32;
      wi.window_handle = reinterpret_cast<void*>(winrt::get_abi(*s_corewind));
    }
    else
    {
      wi.type = WindowInfo::Type::Surfaceless;
    }

    return wi;
}

void WinRTHost::SaveSettings()
{
    auto lock = Host::GetSettingsLock();
    if (!s_base_settings_interface->Save())
      Log_ErrorPrintf("Failed to save settings.");
}

void WinRTHost::StartCPUThread()
{
    s_running.store(true, std::memory_order_release);
    s_cpu_thread.Start(CPUThreadEntryPoint);
}

void WinRTHost::StopCPUThread()
{
    if (!s_cpu_thread.Joinable()) { return; }

    {
      std::unique_lock lock(s_cpu_thread_events_mutex);
      s_running.store(false, std::memory_order_release);
      s_cpu_thread_event_posted.notify_one();
    }
    s_cpu_thread.Join();
}


void WinRTHost::CPUThreadEntryPoint()
{
    Threading::SetNameOfCurrentThread("CPU Thread");

    // input source setup must happen on emu thread
    ::System::Internal::ProcessStartup();


    // start the fullscreen UI and get it going
    if (Host::CreateGPUDevice(Settings::GetRenderAPIForRenderer(g_settings.gpu_renderer)) && FullscreenUI::Initialize())
    {
      // kick a game list refresh if we're not in batch mode
      if (!InBatchMode())
        Host::RefreshGameListAsync(false);

      WinRTHost::CPUThreadMainLoop();

      Host::CancelGameListRefresh();
    }

    // finish any events off (e.g. shutdown system with save)
    ProcessCPUThreadEvents(false);

    if (::System::IsValid())
      ::System::ShutdownSystem(false);
    Host::ReleaseGPUDevice();
    Host::ReleaseRenderWindow();

    ::System::Internal::ProcessShutdown();
}

void WinRTHost::CPUThreadMainLoop()
{
    while (s_running.load(std::memory_order_acquire))
    {
      if (::System::IsRunning())
      {
          ::System::Execute();
          continue;
      }

      Host::PumpMessagesOnCPUThread();
      ::System::Internal::IdlePollUpdate();
      ::System::PresentDisplay(false);
      if (!g_gpu_device->IsVSyncActive()) // stenzek still hasn't updated this on his own NoGUI host. lmao?
        g_gpu_device->ThrottlePresentation();
    }
}

void WinRTHost::ProcessCPUThreadEvents(bool block)
{
    std::unique_lock lock(s_cpu_thread_events_mutex);

    for (;;)
    {
      if (s_cpu_thread_events.empty())
      {
        if (!block || !s_running.load(std::memory_order_acquire))
          return;

        // we still need to keep polling the controllers when we're paused
        do
        {
          InputManager::PollSources();
        } while (!s_cpu_thread_event_posted.wait_for(lock, CPU_THREAD_POLL_INTERVAL,
                                                     []() { return !s_cpu_thread_events.empty(); }));
      }

      // return after processing all events if we had one
      block = false;

      auto event = std::move(s_cpu_thread_events.front());
      s_cpu_thread_events.pop_front();
      lock.unlock();
      event.first();
      lock.lock();

      if (event.second)
      {
        s_blocking_cpu_events_pending--;
        s_cpu_thread_event_done.notify_one();
      }
    }
}

void WinRTHost::StartAsyncOp(std::function<void(ProgressCallback*)> callback)
{
    CancelAsyncOp();
    s_async_op_thread = std::thread(AsyncOpThreadEntryPoint, std::move(callback));
}

void WinRTHost::CancelAsyncOp()
{
    std::unique_lock lock(s_async_op_mutex);
    if (!s_async_op_thread.joinable())
      return;

    if (s_async_op_progress)
      s_async_op_progress->SetCancelled();

    lock.unlock();
    s_async_op_thread.join();
}

void WinRTHost::AsyncOpThreadEntryPoint(std::function<void(ProgressCallback*)> callback)
{
    Threading::SetNameOfCurrentThread("Async Op");

    AsyncOpProgressCallback fs_callback("async_op");
    std::unique_lock lock(s_async_op_mutex);
    s_async_op_progress = &fs_callback;

    lock.unlock();
    callback(&fs_callback);
    lock.lock();

    s_async_op_progress = nullptr;
}

ALWAYS_INLINE std::string WinRTHost::GetResourcePath(std::string_view filename, bool allow_override)
{
    return allow_override ? EmuFolders::GetOverridableResourcePath(filename) :
                            Path::Combine(EmuFolders::Resources, filename);
}

void Host::RunOnCPUThread(std::function<void()> function, bool block /* = false */)
{
    std::unique_lock lock(WinRTHost::s_cpu_thread_events_mutex);
    WinRTHost::s_cpu_thread_events.emplace_back(std::move(function), block);
    WinRTHost::s_cpu_thread_event_posted.notify_one();
    if (block)
      WinRTHost::s_cpu_thread_event_done.wait(lock, []() { return WinRTHost::s_blocking_cpu_events_pending == 0; });
}


void Host::RefreshGameListAsync(bool invalidate_cache)
{
    WinRTHost::StartAsyncOp(
      [invalidate_cache](ProgressCallback* progress) { GameList::Refresh(invalidate_cache, false, progress); });
}

void Host::CancelGameListRefresh()
{
    WinRTHost::CancelAsyncOp();
}

// Host impls
void Host::ReportFatalError(const std::string_view& title, const std::string_view& message)
{
  Log_ErrorPrintf("ReportFatalError: %.*s", static_cast<int>(message.size()), message.data());
  abort();
}

void Host::ReportErrorAsync(const std::string_view& title, const std::string_view& message)
{
  if (!title.empty() && !message.empty())
  {
    Log_ErrorPrintf("ReportErrorAsync: %.*s: %.*s", static_cast<int>(title.size()), title.data(),
                    static_cast<int>(message.size()), message.data());
  }
  else if (!message.empty())
  {
    Log_ErrorPrintf("ReportErrorAsync: %.*s", static_cast<int>(message.size()), message.data());
  }
}

bool Host::ConfirmMessage(const std::string_view& title, const std::string_view& message)
{
  if (!title.empty() && !message.empty())
  {
    Log_ErrorPrintf("ConfirmMessage: %.*s: %.*s", static_cast<int>(title.size()), title.data(),
                    static_cast<int>(message.size()), message.data());
  }
  else if (!message.empty())
  {
    Log_ErrorPrintf("ConfirmMessage: %.*s", static_cast<int>(message.size()), message.data());
  }

  return true;
}

void Host::ReportDebuggerMessage(const std::string_view& message)
{
  Log_ErrorPrintf("ReportDebuggerMessage: %.*s", static_cast<int>(message.size()), message.data());
}

std::span<const std::pair<const char*, const char*>> Host::GetAvailableLanguageList()
{
  return {};
}

bool Host::ChangeLanguage(const char* new_language)
{
  return false;
}

void Host::AddFixedInputBindings(SettingsInterface& si)
{
}

void Host::OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name)
{
  Host::AddKeyedOSDMessage(fmt::format("InputDeviceConnected-{}", identifier),
                           fmt::format("Input device {0} ({1}) connected.", device_name, identifier), 10.0f);
}

void Host::OnInputDeviceDisconnected(const std::string_view& identifier)
{
  Host::AddKeyedOSDMessage(fmt::format("InputDeviceConnected-{}", identifier),
                           fmt::format("Input device {} disconnected.", identifier), 10.0f);
}

s32 Host::Internal::GetTranslatedStringImpl(const std::string_view& context, const std::string_view& msg, char* tbuf,
                                            size_t tbuf_space)
{
  if (msg.size() > tbuf_space)
    return -1;
  else if (msg.empty())
    return 0;

  std::memcpy(tbuf, msg.data(), msg.size());
  return static_cast<s32>(msg.size());
}

std::optional<std::vector<u8>> Host::ReadResourceFile(std::string_view filename, bool allow_override)
{
  const std::string path = WinRTHost::GetResourcePath(filename, allow_override);
  std::optional<std::vector<u8>> ret(FileSystem::ReadBinaryFile(path.c_str()));
  if (!ret.has_value())
    Log_ErrorPrintf("Failed to read resource file '%s'", filename);
  return ret;
}

bool Host::ResourceFileExists(std::string_view filename, bool allow_override)
{
  const std::string path = WinRTHost::GetResourcePath(filename, allow_override);
  return FileSystem::FileExists(path.c_str());
}

std::optional<std::string> Host::ReadResourceFileToString(std::string_view filename, bool allow_override)
{
  const std::string path = WinRTHost::GetResourcePath(filename, allow_override);
  std::optional<std::string> ret(FileSystem::ReadFileToString(path.c_str()));
  if (!ret.has_value())
    Log_ErrorPrintf("Failed to read resource file to string '%s'", filename);
  return ret;
}

std::optional<std::time_t> Host::GetResourceFileTimestamp(std::string_view filename, bool allow_override)
{
  const std::string path = WinRTHost::GetResourcePath(filename, allow_override);
  FILESYSTEM_STAT_DATA sd;
  if (!FileSystem::StatFile(path.c_str(), &sd))
  {
    Log_ErrorPrintf("Failed to stat resource file '%s'", filename);
    return std::nullopt;
  }

  return sd.ModificationTime;
}

std::optional<WindowInfo> Host::AcquireRenderWindow(bool recreate_window)
{
  return WinRTHost::GetPlatformWindowInfo();
}

void Host::ReleaseRenderWindow()
{
    // No-op
}

void Host::OnSystemStarting()
{
  // TODO: Revisit
  //s_was_paused_by_focus_loss = false;
}

void Host::OnSystemStarted()
{
}

void Host::OnSystemPaused()
{
}

void Host::OnSystemResumed()
{
}

void Host::OnSystemDestroyed()
{
}

void Host::OnIdleStateChanged()
{
}

void Host::BeginPresentFrame()
{
}

void Host::RequestResizeHostDisplay(s32 width, s32 height)
{

}

void Host::OpenURL(const std::string_view& url)
{
  winrt::Windows::Foundation::Uri m_uri{winrt::to_hstring(url)};
  auto asyncOperation = winrt::Windows::System::Launcher::LaunchUriAsync(m_uri);
  asyncOperation.Completed([](winrt::Windows::Foundation::IAsyncOperation<bool> const& sender,
                              winrt::Windows::Foundation::AsyncStatus const asyncStatus) { return; });
}

bool Host::CopyTextToClipboard(const std::string_view& text)
{
  return false;
}

void Host::OnPerformanceCountersUpdated()
{
  // noop
}

void Host::OnGameChanged(const std::string& disc_path, const std::string& game_serial, const std::string& game_name)
{
  Log_VerbosePrintf("Host::OnGameChanged(\"%s\", \"%s\", \"%s\")", disc_path.c_str(), game_serial.c_str(),
                    game_name.c_str());
}

void Host::OnAchievementsLoginRequested(Achievements::LoginRequestReason reason)
{
  //ImGui::OpenPopup("Achievements Login");
  //FullscreenUI::DrawAchievementsLoginWindow();
}

void Host::OnAchievementsLoginSuccess(const char* username, u32 points, u32 sc_points, u32 unread_messages)
{
  // noop
}

void Host::OnAchievementsRefreshed()
{
  // noop
}

void Host::OnAchievementsHardcoreModeChanged(bool enabled)
{
  // noop
}

void Host::SetMouseMode(bool relative, bool hide_cursor)
{
  // noop
}

void Host::PumpMessagesOnCPUThread()
{
  WinRTHost::ProcessCPUThreadEvents(false);
  //NoGUIHost::ProcessCPUThreadPlatformMessages(); // TODO: May need something like this
}

bool Host::IsFullscreen()
{
  return true;
}

void Host::SetFullscreen(bool enabled)
{

}

void Host::LoadSettings(SettingsInterface& si, std::unique_lock<std::mutex>& lock)
{
}

void Host::CheckForSettingsChanges(const Settings& old_settings)
{
}

void Host::CommitBaseSettingChanges()
{
  WinRTHost::SaveSettings();
}

void Host::OnCoverDownloaderOpenRequested()
{
    // no-op
}

std::optional<WindowInfo> Host::GetTopLevelWindowInfo()
{
  return WinRTHost::GetPlatformWindowInfo();
}

void Host::RequestExit(bool allow_confirm)
{
  if (::System::IsValid())
  {
    Host::RunOnCPUThread([]() { ::System::ShutdownSystem(g_settings.save_state_on_exit); });
  }

  // clear the running flag, this'll break out of the main CPU loop once the VM is shutdown.
  WinRTHost::s_running.store(false, std::memory_order_release);
}

void Host::RequestSystemShutdown(bool allow_confirm, bool save_state)
{
  if (::System::IsValid())
  {
    Host::RunOnCPUThread([save_state]() { ::System::ShutdownSystem(save_state); });
  }
}

std::optional<u32> InputManager::ConvertHostKeyboardStringToCode(const std::string_view& str)
{
  std::optional<DWORD> converted(Win32KeyNames::GetKeyCodeForName(str));
  return converted.has_value() ? std::optional<u32>(static_cast<u32>(converted.value())) : std::nullopt;

}

std::optional<std::string> InputManager::ConvertHostKeyboardCodeToString(u32 code)
{
  const char* converted = Win32KeyNames::GetKeyName(code);
  return converted ? std::optional<std::string>(converted) : std::nullopt;
}

const char* InputManager::ConvertHostKeyboardCodeToIcon(u32 code)
{
  return nullptr;
}

WinRTHost::AsyncOpProgressCallback::AsyncOpProgressCallback(std::string name)
  : BaseProgressCallback(), m_name(std::move(name))
{
  ImGuiFullscreen::OpenBackgroundProgressDialog(m_name.c_str(), "", 0, 100, 0);
}

WinRTHost::AsyncOpProgressCallback::~AsyncOpProgressCallback()
{
  ImGuiFullscreen::CloseBackgroundProgressDialog(m_name.c_str());
}

void WinRTHost::AsyncOpProgressCallback::PushState()
{
  BaseProgressCallback::PushState();
}

void WinRTHost::AsyncOpProgressCallback::PopState()
{
  BaseProgressCallback::PopState();
  Redraw(true);
}

void WinRTHost::AsyncOpProgressCallback::SetCancellable(bool cancellable)
{
  BaseProgressCallback::SetCancellable(cancellable);
  Redraw(true);
}

void WinRTHost::AsyncOpProgressCallback::SetTitle(const char* title)
{
  // todo?
}

void WinRTHost::AsyncOpProgressCallback::SetStatusText(const char* text)
{
  BaseProgressCallback::SetStatusText(text);
  Redraw(true);
}

void WinRTHost::AsyncOpProgressCallback::SetProgressRange(u32 range)
{
  u32 last_range = m_progress_range;

  BaseProgressCallback::SetProgressRange(range);

  if (m_progress_range != last_range)
    Redraw(false);
}

void WinRTHost::AsyncOpProgressCallback::SetProgressValue(u32 value)
{
  u32 lastValue = m_progress_value;

  BaseProgressCallback::SetProgressValue(value);

  if (m_progress_value != lastValue)
    Redraw(false);
}

void WinRTHost::AsyncOpProgressCallback::Redraw(bool force)
{
  const int percent =
    static_cast<int>((static_cast<float>(m_progress_value) / static_cast<float>(m_progress_range)) * 100.0f);
  if (percent == m_last_progress_percent && !force)
    return;

  m_last_progress_percent = percent;
  ImGuiFullscreen::UpdateBackgroundProgressDialog(m_name.c_str(), m_status_text, 0, 100, percent);
}

void WinRTHost::AsyncOpProgressCallback::DisplayError(const char* message)
{
  Log_ErrorPrint(message);
  Host::ReportErrorAsync("Error", message);
}

void WinRTHost::AsyncOpProgressCallback::DisplayWarning(const char* message)
{
  Log_WarningPrint(message);
}

void WinRTHost::AsyncOpProgressCallback::DisplayInformation(const char* message)
{
  Log_InfoPrint(message);
}

void WinRTHost::AsyncOpProgressCallback::DisplayDebugMessage(const char* message)
{
  Log_DebugPrint(message);
}

void WinRTHost::AsyncOpProgressCallback::ModalError(const char* message)
{
  Log_ErrorPrint(message);
  Host::ReportErrorAsync("Error", message);
}

bool WinRTHost::AsyncOpProgressCallback::ModalConfirmation(const char* message)
{
  return false;
}

void WinRTHost::AsyncOpProgressCallback::ModalInformation(const char* message)
{
  Log_InfoPrint(message);
}

void WinRTHost::AsyncOpProgressCallback::SetCancelled()
{
  if (m_cancellable)
    m_cancelled = true;
}

BEGIN_HOTKEY_LIST(g_host_hotkeys)
END_HOTKEY_LIST()


struct App : implements<App, IFrameworkViewSource, IFrameworkView>
{
  std::thread m_cpu_thread;
  winrt::hstring m_launchOnExit;
  std::string m_bootPath;

  IFrameworkView CreateView() { return *this; }

  void Initialize(CoreApplicationView const& v)
  {
    v.Activated({this, &App::OnActivate});

    // Setup folders
    const std::string program_path = FileSystem::GetProgramPath();

    EmuFolders::AppRoot = Path::Canonicalize(Path::GetDirectory(program_path));
    EmuFolders::Resources = Path::Combine(EmuFolders::AppRoot, "resources");
    EmuFolders::DataRoot = UWP::GetLocalFolder();

    namespace WGI = winrt::Windows::Gaming::Input;
    try
    {
        WGI::RawGameController::RawGameControllerAdded([](auto&&, const WGI::RawGameController raw_game_controller) {
          Host::RunOnCPUThread([]() { InputManager::ReloadDevices(); });
        });

        WGI::RawGameController::RawGameControllerRemoved([](auto&&, const WGI::RawGameController raw_game_controller) {
          Host::RunOnCPUThread([]() { InputManager::ReloadDevices(); });
        });
    }
    catch (winrt::hresult_error)
    {
    }

  }

  void OnActivate(const winrt::Windows::ApplicationModel::Core::CoreApplicationView&,
                  const winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs& args)
  {
    std::stringstream filePath;

    if (args.Kind() == Windows::ApplicationModel::Activation::ActivationKind::Protocol)
    {
      auto protocolActivatedEventArgs{args.as<Windows::ApplicationModel::Activation::ProtocolActivatedEventArgs>()};
      auto query = protocolActivatedEventArgs.Uri().QueryParsed();

      for (uint32_t i = 0; i < query.Size(); i++)
      {
        auto arg = query.GetAt(i);

        // parse command line string
        if (arg.Name() == winrt::hstring(L"cmd"))
        {
          std::string argVal = winrt::to_string(arg.Value());

          // Strip the executable from the cmd argument
          if (argVal.rfind("duckstation.exe", 0) == 0)
          {
            argVal = argVal.substr(16, argVal.length());
          }

          std::istringstream iss(argVal);
          std::string s;

          // Maintain slashes while reading the quotes
          while (iss >> std::quoted(s, '"', (char)0))
          {
            filePath << s;
          }
        }
        else if (arg.Name() == winrt::hstring(L"launchOnExit"))
        {
          // For if we want to return to a frontend
          m_launchOnExit = arg.Value();
        }
      }
    }

    std::string gamePath = filePath.str();
    if (!gamePath.empty() && gamePath != "")
    {
      SystemBootParameters params;
      params.filename = gamePath;
      Host::RunOnCPUThread([params = std::move(params)]() { ::System::BootSystem(std::move(params)); });
    }
  }

  void Load(hstring const&) {}

  void Uninitialize() {}

  void Run() { 
    CoreWindow window = CoreWindow::GetForCurrentThread();
    WinRTHost::s_corewind = &window;
    window.Activate();

    auto navigation = winrt::Windows::UI::Core::SystemNavigationManager::GetForCurrentView();
    navigation.BackRequested([](const winrt::Windows::Foundation::IInspectable&,
                                const winrt::Windows::UI::Core::BackRequestedEventArgs& args) { args.Handled(true); });

    GAMING_DEVICE_MODEL_INFORMATION gaming_device_info = {};
    GetGamingDeviceModelInformation(&gaming_device_info);
    WinRTHost::is_running_on_xbox = (gaming_device_info.vendorId == GAMING_DEVICE_VENDOR_ID_MICROSOFT);

    CrashHandler::Install();

    WinRTHost::InitializeConfig();

    // the rest of initialization happens on the CPU thread.
    WinRTHost::StartCPUThread();

    // Refresh inputs
    window.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, []() {
      Sleep(500);
      InputManager::ReloadDevices();
    });


    while (WinRTHost::s_running.load())
    {
        window.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    WinRTHost::CancelAsyncOp();

	if (!m_launchOnExit.empty())
    {
      winrt::Windows::Foundation::Uri m_uri{m_launchOnExit};
      auto asyncOperation = winrt::Windows::System::Launcher::LaunchUriAsync(m_uri);
      asyncOperation.Completed([](winrt::Windows::Foundation::IAsyncOperation<bool> const& sender,
                                  winrt::Windows::Foundation::AsyncStatus const asyncStatus) {
        WinRTHost::StopCPUThread();
        CoreApplication::Exit();
        return;
      });
    }
    else
    {
      WinRTHost::StopCPUThread();
      CoreApplication::Exit();
    }

    // Ensure log is flushed.
    Log::SetFileOutputParams(false, nullptr);

    //s_base_settings_interface.reset();
  }

  void SetWindow(CoreWindow const& window) { window.CharacterReceived({this, &App::OnKeyInput}); }


  void OnKeyInput(const IInspectable&, const winrt::Windows::UI::Core::CharacterReceivedEventArgs& args)
  {
    Log_InfoPrintf("%d", args.KeyCode());
    ImGuiManager::AddCharacterInput(std::move(args.KeyCode()));
  }
};

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
  winrt::init_apartment();

  CoreApplication::Run(make<App>());

  winrt::uninit_apartment();
}