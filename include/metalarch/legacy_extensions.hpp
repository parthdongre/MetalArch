#pragma once
#include "metalarch/core.hpp"

namespace ma {
// Audited, window-bounded C++20 adaptations of eight ASEP2 analytical methods.
// Values are raw mathematical statistics, not legacy 0-100 opinion scores.
std::vector<Descriptor> make_legacy_extensions(const Key& primary);
}
