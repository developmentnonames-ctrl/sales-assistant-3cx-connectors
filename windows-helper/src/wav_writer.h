// Diagnostics build only (--record): writes what the helper hears to a 16 kHz mono WAV file, for
// checking the capture on a new PC. The program agents get never writes audio to disk.
#ifndef SA3CX_WAV_WRITER_H_
#define SA3CX_WAV_WRITER_H_

#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace sa3cx {

class WavWriter {
 public:
  explicit WavWriter(const std::wstring& path);
  ~WavWriter();  // finishes the header
  WavWriter(const WavWriter&) = delete;
  WavWriter& operator=(const WavWriter&) = delete;

  bool ok() const { return file_ != nullptr; }
  void Write(const std::vector<int16_t>& samples);
  uint32_t bytes_written() const { return data_bytes_; }

 private:
  void WriteHeader();

  std::mutex mutex_;
  FILE* file_ = nullptr;
  uint32_t data_bytes_ = 0;
};

}  // namespace sa3cx

#endif  // SA3CX_WAV_WRITER_H_
