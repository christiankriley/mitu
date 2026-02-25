#ifndef MITU_HPP
#define MITU_HPP

#include <cstdint>
#include <array>
#include <cstddef>
#include <string_view>

#ifndef APP_VERSION
#define APP_VERSION "0.0.0"
#endif

#ifndef SCHEMA_VERSION
#define SCHEMA_VERSION 0
#endif

#pragma pack(push, 1)

namespace mitus {

inline constexpr std::string_view VERSION = APP_VERSION;
inline constexpr uint32_t S_VERSION = SCHEMA_VERSION;

inline uint32_t calculate_crc32(const void* data, size_t length, uint32_t crc = 0xFFFFFFFF) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(data);
    while (length--) {
        crc ^= *p++;
        for (int i = 0; i < 8; i++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
    }
    return crc;
}

struct MetadataRecord {
    int32_t city_off{-1};
    int32_t state_off{-1};
    int32_t tz_off{-1};
};

struct StaticNode {
    std::array<int32_t, 10> children;
    int32_t record_idx{-1};

    StaticNode() {
        children.fill(-1);
    }
};

struct FileHeader {
    uint32_t magic{0x4D495455}; // MITU
    uint32_t version{1}; // update if data structure changes
    uint32_t node_count{0};
    uint32_t record_count{0};
    uint32_t checksum{0};
};

static_assert(sizeof(MetadataRecord) == 12, "MetadataRecord size mismatch");
static_assert(sizeof(StaticNode) == 44, "StaticNode size mismatch");
static_assert(sizeof(FileHeader) == 20, "FileHeader size mismatch");
static_assert(std::is_standard_layout_v<MetadataRecord>, "MetadataRecord must be standard layout");
static_assert(std::is_standard_layout_v<StaticNode>, "StaticNode must be standard layout");
static_assert(std::is_trivially_copyable_v<MetadataRecord>, "MetadataRecord must be trivially copyable");
static_assert(std::is_trivially_copyable_v<StaticNode>, "StaticNode must be trivially copyable");

#pragma pack(pop)

} // namespace mitus

#endif // MITU_HPP