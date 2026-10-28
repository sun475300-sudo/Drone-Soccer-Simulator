#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

// Canvas coordinates are logical HUD units. Keep measuring and drawing in the
// same units while imposing a floor in actual viewport pixels, not Windows DPI.
namespace ImperialHUDReadability
{
inline float LogicalTextSize(float Requested, float Scale)
{
    const float SafeScale = std::isfinite(Scale) && Scale > 0.f ? Scale : 1.f;
    const float SafeSize = std::isfinite(Requested) && Requested > 0.f ? Requested : 10.f;
    return std::max(SafeSize, 10.f / SafeScale);
}
template<class Character>
std::size_t PreviousCodePointLength(const Character* Value, std::size_t Length)
{
    if (!Length) return 0;
    std::size_t Next = Length - 1;
    const unsigned Last = static_cast<unsigned>(Value[Next]);
    if (Next && Last >= 0xdc00u && Last <= 0xdfffu)
    {
        const unsigned Before = static_cast<unsigned>(Value[Next - 1]);
        if (Before >= 0xd800u && Before <= 0xdbffu) --Next;
    }
    return Next;
}
}
