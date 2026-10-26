// Project-owned regression for I01: the Digitakt mk1's captured USB layout
// must remain directional. This reads the sanitized R02 endpoint artifact, so
// the test proves the fixture and the host codec agree without needing hardware.
//
//   c++ -std=c++17 -Wall -I deps/sidecar-firmware/teensy/libraries/RytmHost \
//       tests/test_digitakt_layout.cpp -o /tmp/test_digitakt_layout
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "ob_block.h"

static std::string json_field(const std::string& s, const std::string& key) {
  const std::string k = "\"" + key + "\"";
  auto p = s.find(k);
  if (p == std::string::npos) return {};
  p = s.find(':', p + k.size());
  if (p == std::string::npos) return {};
  p = s.find_first_not_of(" \t\r\n", p + 1);
  if (p == std::string::npos) return {};
  if (s[p] == '"') {
    const auto e = s.find('"', p + 1);
    return e == std::string::npos ? std::string() : s.substr(p + 1, e - p - 1);
  }
  const auto e = s.find_first_of(",}\r\n", p);
  return s.substr(p, e - p);
}

int main(int argc, char** argv) {
  const char* path = argc > 1 ? argv[1] : "evidence/r02-endpoints-2026-10-26.json";
  std::ifstream f(path);
  if (!f) { std::perror(path); return 2; }
  std::stringstream ss;
  ss << f.rdbuf();
  const std::string json = ss.str();

  bool found_in = false, found_out = false;
  int in_mps = 0, out_mps = 0;
  std::istringstream lines(json);
  std::string object;
  while (std::getline(lines, object, '}')) {
    if (json_field(object, "interface") != "1" || json_field(object, "alternate") != "3") {
      if (json_field(object, "interface") != "2" || json_field(object, "alternate") != "3") continue;
      out_mps = std::atoi(json_field(object, "wMaxPacketSize").c_str());
      found_out = true;
    } else {
      in_mps = std::atoi(json_field(object, "wMaxPacketSize").c_str());
      found_in = true;
    }
  }
  if (!found_in || !found_out) { std::fprintf(stderr, "R02 alt-3 endpoints not found in %s\n", path); return 1; }

  const ob::Direction in = ob::direction_for(in_mps);
  const ob::Direction out = ob::direction_for(out_mps);
  if (!in.valid() || !out.valid()) {
    std::fprintf(stderr, "invalid direction: IN %d/%d OUT %d/%d\n", in.channels, in.bytes, out.channels, out.bytes);
    return 1;
  }
  if (in.channels != 12 || in.bytes != 368 || out.channels != 2 || out.bytes != 88) {
    std::fprintf(stderr, "unexpected Digitakt layout: IN %d/%d OUT %d/%d\n", in.channels, in.bytes, out.channels, out.bytes);
    return 1;
  }
  if (ob::block_bytes(in.channels) != in_mps || ob::block_bytes(out.channels) != out_mps) {
    std::fprintf(stderr, "packet sizes do not match the fixture\n");
    return 1;
  }
  // A shared layout size would be the bug this test exists to prevent.
  if (in.bytes == out.bytes) {
    std::fprintf(stderr, "directions collapsed to a single size\n");
    return 1;
  }
  std::printf("Digitakt layout: IN %d ch / %d B, OUT %d ch / %d B\n", in.channels, in.bytes, out.channels, out.bytes);
  return 0;
}
