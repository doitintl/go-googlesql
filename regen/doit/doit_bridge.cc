// doit_bridge.cc: hand-written bridge entry points for the DoiT fork of
// go-googlesql (github.com/doitintl/go-googlesql).
//
// wasmify compiles and links this file next to the generated
// bridge/api_bridge.cc (wasmify.json, bridge.CustomBridgeSources). The
// generated bridge numbers its services from 0 in declaration order and
// has about 1,500 of them; service 9000 is reserved for DoiT additions so
// a regeneration can never collide with it. wasm2go turns every export
// named w_<svc>_<mid> into a Go entry point Inv_<svc>_<mid>, which the
// hand-written Go side in go-googlesql (analyzer_options_doit.go) calls.
//
// Wire format, identical to api_bridge.cc: proto3 varints and
// length-delimited fields; a handle is a submessage {1: uint64 ptr};
// an error is a string in field 15 of the response.
//
// Why these exist: wasmify cannot carry absl::TimeZone across the Wasm
// boundary (it is an external type with no serializable fields), so the
// generated AnalyzerOptions.SetDefaultTimeZone(*TimeZone) sets a
// default-constructed zone and DefaultTimeZone() returns nothing. These
// two entry points take and return the zone by name instead, through
// googlesql::FindTimeZoneByName (absl::LoadTimeZone) and
// absl::TimeZone::name().

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "absl/time/time.h"
#include "googlesql/public/analyzer_options.h"
#include "googlesql/public/time_zone_util.h"

#define DOIT_WASM_EXPORT(name) __attribute__((export_name(#name)))

namespace doit_bridge {

class Reader {
 public:
  Reader(const void* data, int32_t len)
      : pos_(static_cast<const uint8_t*>(data)), end_(pos_ + len) {}

  bool next() {
    if (pos_ >= end_) return false;
    uint64_t tag = 0;
    if (!varint(&tag)) return false;
    field_ = static_cast<uint32_t>(tag >> 3);
    wire_ = static_cast<uint32_t>(tag & 7);
    return true;
  }
  uint32_t field() const { return field_; }

  uint64_t read_uint64() {
    uint64_t v = 0;
    varint(&v);
    return v;
  }

  std::string read_string() {
    uint64_t len = read_uint64();
    if (pos_ + len > end_) {
      pos_ = end_;
      return "";
    }
    std::string s(reinterpret_cast<const char*>(pos_), static_cast<size_t>(len));
    pos_ += len;
    return s;
  }

  // A handle is a submessage whose field 1 is the uint64 pointer.
  uint64_t read_handle() {
    uint64_t len = read_uint64();
    if (pos_ + len > end_) {
      pos_ = end_;
      return 0;
    }
    Reader sub(pos_, static_cast<int32_t>(len));
    pos_ += len;
    uint64_t ptr = 0;
    while (sub.next()) {
      if (sub.field() == 1) {
        ptr = sub.read_uint64();
      } else {
        sub.skip();
      }
    }
    return ptr;
  }

  void skip() {
    switch (wire_) {
      case 0: read_uint64(); break;
      case 1: pos_ += 8; break;
      case 2: { uint64_t len = read_uint64(); pos_ += len; } break;
      case 5: pos_ += 4; break;
      default: pos_ = end_; break;
    }
    if (pos_ > end_) pos_ = end_;
  }

 private:
  bool varint(uint64_t* val) {
    *val = 0;
    for (int shift = 0; shift < 64 && pos_ < end_; shift += 7) {
      uint8_t byte = *pos_++;
      *val |= static_cast<uint64_t>(byte & 0x7F) << shift;
      if ((byte & 0x80) == 0) return true;
    }
    return false;
  }

  const uint8_t* pos_;
  const uint8_t* end_;
  uint32_t field_ = 0;
  uint32_t wire_ = 0;
};

// Writer builds the response in a malloc'd buffer that finish() hands to
// the host, which releases it with wasm_free (free) after reading.
class Writer {
 public:
  Writer() = default;
  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;
  ~Writer() { free(data_); }

  void write_string(uint32_t field, const std::string& s) {
    if (s.empty()) return;
    write_varint((static_cast<uint64_t>(field) << 3) | 2);
    write_varint(s.size());
    write_raw(s.data(), static_cast<int32_t>(s.size()));
  }

  void write_error(const std::string& msg) { write_string(15, msg); }

  int64_t finish() {
    if (size_ == 0) return 0;
    uint8_t* released = data_;
    int32_t released_size = size_;
    data_ = nullptr;
    size_ = 0;
    cap_ = 0;
    return (static_cast<int64_t>(reinterpret_cast<uintptr_t>(released)) << 32) |
           static_cast<uint32_t>(released_size);
  }

 private:
  void ensure(int32_t need) {
    if (data_ == nullptr) {
      cap_ = need > 128 ? need : 128;
      data_ = static_cast<uint8_t*>(malloc(cap_));
      return;
    }
    while (size_ + need > cap_) {
      cap_ *= 2;
      data_ = static_cast<uint8_t*>(realloc(data_, cap_));
    }
  }
  void write_raw(const void* src, int32_t len) {
    ensure(len);
    memcpy(data_ + size_, src, len);
    size_ += len;
  }
  void write_varint(uint64_t val) {
    ensure(10);
    while (val > 0x7F) {
      data_[size_++] = static_cast<uint8_t>((val & 0x7F) | 0x80);
      val >>= 7;
    }
    data_[size_++] = static_cast<uint8_t>(val);
  }

  uint8_t* data_ = nullptr;
  int32_t size_ = 0;
  int32_t cap_ = 0;
};

}  // namespace doit_bridge

extern "C" {

// w_9000_0: AnalyzerOptions::SetDefaultTimeZoneName.
// Request: {1: handle AnalyzerOptions, 2: string name}. Response: empty, or
// {15: error} when FindTimeZoneByName rejects the name.
DOIT_WASM_EXPORT(w_9000_0)
int64_t w_9000_0(void* req, int32_t req_len) {
  uint64_t handle = 0;
  std::string name;
  doit_bridge::Reader reader(req, req_len);
  while (reader.next()) {
    switch (reader.field()) {
      case 1: handle = reader.read_handle(); break;
      case 2: name = reader.read_string(); break;
      default: reader.skip(); break;
    }
  }
  doit_bridge::Writer w;
  if (handle == 0) {
    w.write_error("null handle");
    return w.finish();
  }
  absl::TimeZone tz;
  absl::Status st = googlesql::FindTimeZoneByName(name, &tz);
  if (!st.ok()) {
    w.write_error(std::string(st.message()));
    return w.finish();
  }
  reinterpret_cast<googlesql::AnalyzerOptions*>(handle)->set_default_time_zone(tz);
  return w.finish();
}

// w_9000_1: AnalyzerOptions::DefaultTimeZoneName.
// Request: {1: handle AnalyzerOptions}. Response: {2: string name}, the
// absl::TimeZone name ("UTC", "America/Los_Angeles").
DOIT_WASM_EXPORT(w_9000_1)
int64_t w_9000_1(void* req, int32_t req_len) {
  uint64_t handle = 0;
  doit_bridge::Reader reader(req, req_len);
  while (reader.next()) {
    switch (reader.field()) {
      case 1: handle = reader.read_handle(); break;
      default: reader.skip(); break;
    }
  }
  doit_bridge::Writer w;
  if (handle == 0) {
    w.write_error("null handle");
    return w.finish();
  }
  const auto* opts = reinterpret_cast<const googlesql::AnalyzerOptions*>(handle);
  w.write_string(2, opts->default_time_zone().name());
  return w.finish();
}

}  // extern "C"
