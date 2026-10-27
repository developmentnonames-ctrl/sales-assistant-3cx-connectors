#include "wav_writer.h"

#include "config.h"

namespace sa3cx {

WavWriter::WavWriter(const std::wstring& path) {
  file_ = _wfopen(path.c_str(), L"wb+");
  if (file_ != nullptr) WriteHeader();
}

WavWriter::~WavWriter() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_ == nullptr) return;
  WriteHeader();
  fclose(file_);
}

void WavWriter::Write(const std::vector<int16_t>& samples) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (file_ == nullptr) return;
  fwrite(samples.data(), sizeof(int16_t), samples.size(), file_);
  data_bytes_ += static_cast<uint32_t>(samples.size() * sizeof(int16_t));
}

void WavWriter::WriteHeader() {
  const uint32_t riff_size = 36 + data_bytes_;
  const uint32_t format_size = 16;
  const uint16_t pcm = 1;
  const uint16_t channels = 1;
  const uint32_t rate = kTargetSampleRate;
  const uint32_t byte_rate = kTargetSampleRate * 2;
  const uint16_t block_align = 2;
  const uint16_t bits = 16;
  fseek(file_, 0, SEEK_SET);
  fwrite("RIFF", 1, 4, file_);
  fwrite(&riff_size, 4, 1, file_);
  fwrite("WAVEfmt ", 1, 8, file_);
  fwrite(&format_size, 4, 1, file_);
  fwrite(&pcm, 2, 1, file_);
  fwrite(&channels, 2, 1, file_);
  fwrite(&rate, 4, 1, file_);
  fwrite(&byte_rate, 4, 1, file_);
  fwrite(&block_align, 2, 1, file_);
  fwrite(&bits, 2, 1, file_);
  fwrite("data", 1, 4, file_);
  fwrite(&data_bytes_, 4, 1, file_);
  fseek(file_, 0, SEEK_END);
}

}  // namespace sa3cx
