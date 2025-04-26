#pragma once
#include <iostream>
#include <string_view>

constexpr uint32_t hash(const std::string_view str, uint32_t h = 2166136261u) {
  for (char c : str) {
      h ^= static_cast<uint32_t>(c);
      h *= 16777619u;
  }
  return h;
}