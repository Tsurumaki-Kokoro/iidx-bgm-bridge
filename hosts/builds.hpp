#pragma once
#include <array>
#include <cstdint>
namespace bgmhost {
struct Spec {
    const char* id;
    const char* family;
    const char* version;
    const char* sha256;
    uintptr_t advance,target,lookup,playing,play,custom,customSecondary,volume;
    const char* avsOpen="XCgsqzn000004e";
    const char* avsStat="XCgsqzn0000063";
    std::array<int,4> soundIds{0x2550,0x25A0,0x25F0,0x2640};
};
inline constexpr Spec iidx34{
    "iidx34-20260916","iidx34","2026091600",
    "3755587f44785effe5008f74cfe7dccbdf423ad09feead48e00a90f78a42fe83",
    0x95ACA0,0x95A8B0,0xA801F0,0x5B80F0,0x5B83B0,0x95AB60,0x95AC00,0x5B7CC0
};
inline constexpr Spec iidx33{
    "iidx33-20260819","iidx33","2026081900",
    "073493c6cf06fa4f0d197c60f9ffb9d70bf1ec52828018f1c06bdc24a27bb795",
    0x973B00,0x973710,0xA91A90,0x5C5E00,0x5C60C0,0x9739C0,0x973A60,0x5C59D0
};
inline constexpr std::array<Spec,2> builds{iidx34,iidx33};
} // namespace bgmhost
