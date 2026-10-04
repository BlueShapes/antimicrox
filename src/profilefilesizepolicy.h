#ifndef PROFILEFILESIZEPOLICY_H
#define PROFILEFILESIZEPOLICY_H

#include <cstdint>

/**
 * Limits profile input before XML parsing or migration. Profiles are active
 * content and should remain small enough to prevent avoidable memory pressure.
 */
class ProfileFileSizePolicy final
{
  public:
    static constexpr std::int64_t maximumBytes = 16LL * 1024 * 1024;

    [[nodiscard]] static constexpr bool isAllowed(std::int64_t size) noexcept { return size >= 0 && size <= maximumBytes; }
};

#endif // PROFILEFILESIZEPOLICY_H
