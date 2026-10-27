#include "audio_capture.h"

#include <algorithm>
#include <cmath>
#include <memory>

#include "win32.h"

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <mmreg.h>
#include <wrl/client.h>
// The GUIDs and property keys below are defined in this file only.
#include <initguid.h>
#include <functiondiscoverykeys_devpkey.h>
#include <ksmedia.h>

#include "config.h"
#include "handle.h"
#include "log.h"
#include "process_info.h"
#include "resampler.h"

// MinGW-w64 declares this interface but does not define it (Windows SDK: endpointvolume.h).
struct IAudioMeterInformation : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE GetPeakValue(float* peak) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetMeteringChannelCount(UINT* count) = 0;
  virtual HRESULT STDMETHODCALLTYPE GetChannelsPeakValues(UINT32 count, float* peaks) = 0;
  virtual HRESULT STDMETHODCALLTYPE QueryHardwareSupport(DWORD* mask) = 0;
};
__CRT_UUID_DECL(IAudioMeterInformation, 0xC02216F6, 0x8C67, 0x4B5B, 0x9D, 0x00, 0xD0, 0x08, 0xE7,
                0x3E, 0x00, 0x64)

namespace sa3cx {

namespace {

using Microsoft::WRL::ComPtr;

constexpr REFERENCE_TIME kBufferDuration = 2000000;  // 200 ms, in 100 ns units
constexpr DWORD kPollMs = 10;
constexpr DWORD kDeviceCheckMs = 2000;    // has 3CX moved device? new sessions?
constexpr DWORD kKeepAfterSoundMs = 300;  // filter hold time, keeps word endings
constexpr float kSoundPeak = 0.0005f;     // a 3CX session peak above this is "making sound"
constexpr float kOtherAppPeak = 0.001f;

std::wstring EndpointId(IMMDevice* device) {
  LPWSTR id = nullptr;
  std::wstring result;
  if (SUCCEEDED(device->GetId(&id)) && id != nullptr) {
    result = id;
    CoTaskMemFree(id);
  }
  return result;
}

std::wstring EndpointName(IMMDevice* device) {
  ComPtr<IPropertyStore> properties;
  std::wstring name = L"?";
  if (FAILED(device->OpenPropertyStore(STGM_READ, &properties))) return name;
  PROPVARIANT value;
  PropVariantInit(&value);
  if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR) {
    name = value.pwszVal;
  }
  PropVariantClear(&value);
  return name;
}

// Calls `visit(session, process_id)` for every audio session on a device.
template <typename Visit>
void ForEachSession(IMMDevice* device, Visit visit) {
  ComPtr<IAudioSessionManager2> manager;
  if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &manager))) {
    return;
  }
  ComPtr<IAudioSessionEnumerator> sessions;
  if (FAILED(manager->GetSessionEnumerator(&sessions))) return;
  int count = 0;
  sessions->GetCount(&count);
  for (int i = 0; i < count; ++i) {
    ComPtr<IAudioSessionControl> session;
    if (FAILED(sessions->GetSession(i, &session))) continue;
    ComPtr<IAudioSessionControl2> session2;
    DWORD process_id = 0;
    if (SUCCEEDED(session.As(&session2))) session2->GetProcessId(&process_id);
    visit(session.Get(), process_id);
  }
}

// The device the process plays to: where it has an active session, else any session, else the
// default communications device.
ComPtr<IMMDevice> DeviceOfProcess(IMMDeviceEnumerator* enumerator, uint32_t process_id) {
  ComPtr<IMMDeviceCollection> devices;
  ComPtr<IMMDevice> with_session;
  if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devices))) {
    UINT count = 0;
    devices->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
      ComPtr<IMMDevice> device;
      if (FAILED(devices->Item(i, &device))) continue;
      bool has_session = false;
      bool active = false;
      ForEachSession(device.Get(), [&](IAudioSessionControl* session, DWORD owner) {
        if (owner != process_id) return;
        has_session = true;
        AudioSessionState session_state;
        if (SUCCEEDED(session->GetState(&session_state)) &&
            session_state == AudioSessionStateActive) {
          active = true;
        }
      });
      if (active) return device;
      if (has_session && !with_session) with_session = device;
    }
  }
  if (with_session) return with_session;
  ComPtr<IMMDevice> fallback;
  if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eCommunications, &fallback))) {
    enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &fallback);
  }
  return fallback;
}

// On one device: the meters of the target process's sessions, and the names of other apps
// playing sound there right now.
struct SessionScan {
  std::vector<ComPtr<IAudioMeterInformation>> target_meters;
  std::wstring other_apps;
};

SessionScan ScanSessions(IMMDevice* device, uint32_t process_id) {
  SessionScan scan;
  ForEachSession(device, [&](IAudioSessionControl* session, DWORD owner) {
    ComPtr<IAudioMeterInformation> meter;
    session->QueryInterface(__uuidof(IAudioMeterInformation), &meter);
    if (owner == process_id) {
      if (meter) scan.target_meters.push_back(meter);
      return;
    }
    float peak = 0;
    if (meter) meter->GetPeakValue(&peak);
    AudioSessionState session_state;
    const bool playing = SUCCEEDED(session->GetState(&session_state)) &&
                         session_state == AudioSessionStateActive && peak > kOtherAppPeak;
    if (!playing || owner == GetCurrentProcessId()) return;
    const std::wstring name = owner != 0 ? ProcessName(owner) : L"Windows sounds";
    if (scan.other_apps.find(name) == std::wstring::npos) {
      if (!scan.other_apps.empty()) scan.other_apps += L", ";
      scan.other_apps += name;
    }
  });
  return scan;
}

float LoudestPeak(const std::vector<ComPtr<IAudioMeterInformation>>& meters) {
  float loudest = 0;
  for (const auto& meter : meters) {
    float peak = 0;
    if (SUCCEEDED(meter->GetPeakValue(&peak))) loudest = std::max(loudest, peak);
  }
  return loudest;
}

bool IsFloatFormat(const WAVEFORMATEX* format) {
  if (format->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) return true;
  if (format->wFormatTag != WAVE_FORMAT_EXTENSIBLE || format->wBitsPerSample != 32) return false;
  const auto* extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format);
  return IsEqualGUID(extensible->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
}

// Down-mixes one packet to mono 16-bit.
void ToMono(const BYTE* data, UINT32 frames, int channels, bool is_float, int bits,
            std::vector<int16_t>& mono) {
  mono.assign(frames, 0);
  for (UINT32 f = 0; f < frames; ++f) {
    double sum = 0;
    for (int ch = 0; ch < channels; ++ch) {
      const std::size_t index = static_cast<std::size_t>(f) * channels + ch;
      if (is_float) {
        sum += reinterpret_cast<const float*>(data)[index] * 32767.0;
      } else if (bits == 16) {
        sum += reinterpret_cast<const int16_t*>(data)[index];
      }
    }
    mono[f] = static_cast<int16_t>(std::clamp(sum / channels, -32768.0, 32767.0));
  }
}

// CoTaskMemFree on scope exit.
struct CoTaskMemDeleter {
  void operator()(void* p) const { CoTaskMemFree(p); }
};

}  // namespace

AudioCapture::AudioCapture(AppState& state, CaptureOptions options, FrameSink sink,
                           StateChanged on_changed)
    : state_(state),
      options_(std::move(options)),
      sink_(std::move(sink)),
      on_changed_(std::move(on_changed)) {}

void AudioCapture::Run() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  bool said_waiting = false;
  while (state_.running) {
    const uint32_t process_id = FindTarget();
    if (process_id != state_.app_process_id) {
      state_.app_process_id = process_id;
      on_changed_();
    }
    state_.restart_capture = false;
    if (process_id == 0) {
      if (!said_waiting) {
        log::Write("waiting for the 3CX app (%ls) to start", options_.process_name.c_str());
        said_waiting = true;
      }
      for (int i = 0; i < 20 && state_.running; ++i) Sleep(100);
      continue;
    }
    said_waiting = false;
    if (!CaptureFrom(process_id)) {
      for (int i = 0; i < 50 && state_.running; ++i) Sleep(100);  // try again in 5 s
    }
    state_.app_process_id = 0;
  }
  CoUninitialize();
}

uint32_t AudioCapture::FindTarget() const {
  if (options_.fixed_process_id != 0) {
    return ProcessExists(options_.fixed_process_id) ? options_.fixed_process_id : 0;
  }
  return FindProcessByName(options_.process_name);
}

bool AudioCapture::CaptureFrom(uint32_t process_id) {
  ComPtr<IMMDeviceEnumerator> enumerator;
  if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                              IID_PPV_ARGS(&enumerator)))) {
    log::Write("could not list the sound devices");
    return false;
  }
  ComPtr<IMMDevice> device = DeviceOfProcess(enumerator.Get(), process_id);
  if (!device) {
    log::Write("no sound output device found");
    return false;
  }
  const std::wstring device_id = EndpointId(device.Get());
  const std::wstring device_name = EndpointName(device.Get());

  ComPtr<IAudioClient> client;
  WAVEFORMATEX* raw_format = nullptr;
  HRESULT hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, &client);
  if (SUCCEEDED(hr)) hr = client->GetMixFormat(&raw_format);
  std::unique_ptr<WAVEFORMATEX, CoTaskMemDeleter> format(raw_format);
  if (SUCCEEDED(hr)) {
    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                            kBufferDuration, 0, format.get(), nullptr);
  }
  ComPtr<IAudioCaptureClient> capture;
  if (SUCCEEDED(hr)) hr = client->GetService(IID_PPV_ARGS(&capture));
  if (SUCCEEDED(hr)) hr = client->Start();
  if (FAILED(hr)) {
    log::Write("could not record '%ls' (0x%08lx)", device_name.c_str(), static_cast<long>(hr));
    return false;
  }

  const bool is_float = IsFloatFormat(format.get());
  const int channels = format->nChannels;
  const int rate = static_cast<int>(format->nSamplesPerSec);
  const int bits = format->wBitsPerSample;
  log::Write("listening to '%ls' (where the 3CX app plays, process %lu): %d Hz, %d ch, %s",
             device_name.c_str(), static_cast<unsigned long>(process_id), rate, channels,
             is_float ? "float" : (bits == 16 ? "16-bit" : "other"));

  UniqueHandle process(OpenProcess(SYNCHRONIZE, FALSE, process_id));
  state_.capturing = true;
  on_changed_();

  Resampler resampler(rate, kTargetSampleRate, kFrameSamples, sink_);
  SessionScan scan = ScanSessions(device.Get(), process_id);
  DWORD last_target_sound = 0;
  std::vector<int16_t> mono;
  const DWORD start = GetTickCount();
  DWORD last_device_check = start;
  long long produced = 0;  // input samples handed on, sound or filled silence

  while (state_.running && !state_.restart_capture) {
    if (process.valid() && WaitForSingleObject(process.get(), 0) == WAIT_OBJECT_0) break;  // closed
    Sleep(kPollMs);

    UINT32 packet = 0;
    while (SUCCEEDED(capture->GetNextPacketSize(&packet)) && packet > 0) {
      BYTE* data = nullptr;
      UINT32 frames = 0;
      DWORD flags = 0;
      if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
      if (LoudestPeak(scan.target_meters) > kSoundPeak) last_target_sound = GetTickCount();
      const bool from_target =
          last_target_sound != 0 && GetTickCount() - last_target_sound < kKeepAfterSoundMs;
      if ((flags & AUDCLNT_BUFFERFLAGS_SILENT) == 0 && from_target) {
        ToMono(data, frames, channels, is_float, bits, mono);
      } else {
        mono.assign(frames, 0);  // 3CX quiet: other apps' sound is not passed on
      }
      resampler.Push(mono.data(), mono.size());
      produced += frames;
      capture->ReleaseBuffer(frames);
    }

    // A device that plays nothing delivers nothing: fill the gap with silence so the page keeps
    // receiving 10 frames a second. Never more than 1 s at once (e.g. after the PC slept).
    const long long due = static_cast<long long>(GetTickCount() - start) * rate / 1000;
    if (due - produced > rate / 8) {
      long long gap = due - produced - rate / 50;
      if (gap > rate) {
        produced += gap - rate;
        gap = rate;
      }
      mono.assign(static_cast<std::size_t>(gap), 0);
      resampler.Push(mono.data(), mono.size());
      produced += gap;
    }

    if (GetTickCount() - last_device_check > kDeviceCheckMs) {
      last_device_check = GetTickCount();
      ComPtr<IMMDevice> now = DeviceOfProcess(enumerator.Get(), process_id);
      if (now && EndpointId(now.Get()) != device_id) {
        log::Write("the 3CX app moved to another sound device");
        break;
      }
      const std::wstring before = scan.other_apps;
      scan = ScanSessions(device.Get(), process_id);  // a call opens a new 3CX session
      if (scan.other_apps != before) {
        state_.SetOtherApps(scan.other_apps);
        if (!scan.other_apps.empty()) {
          log::Write("also playing on '%ls': %ls", device_name.c_str(), scan.other_apps.c_str());
        }
        state_.NotifyStatusChanged();
      }
    }
  }

  client->Stop();
  state_.SetOtherApps(L"");
  state_.capturing = false;
  on_changed_();
  log::Write("stopped listening to '%ls'", device_name.c_str());
  return true;
}

}  // namespace sa3cx
