#ifndef MITU_HPP
#define MITU_HPP

#include <cstdint>
#include <array>

namespace mitus {

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
    uint32_t node_count{0};
    uint32_t record_count{0};
};

} // namespace mitus

#endif // MITU_HPP