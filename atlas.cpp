#include "mitu.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <memory>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <set>
#include <filesystem>

using namespace mitus;
namespace fs = std::filesystem;

// use a trie to follow digits so we don't have to look at everything
struct LiveNode {
    std::map<char, std::unique_ptr<LiveNode>> children;
    MetadataRecord record;
    int32_t id{-1};
};

class MapBuilder {
    // avoid costly string objects by using a single pool
    std::string string_pool;
    std::set<std::string> country_prefixes;

    int32_t add_to_pool(std::string_view s) {
        if (s.empty()) return -1;

        if (string_pool.size() + s.size() + 1 > static_cast<size_t>(std::numeric_limits<int32_t>::max())) {
            throw std::runtime_error("String pool overflow");
        }

        const auto off = static_cast<int32_t>(string_pool.size());
        string_pool.append(s);
        string_pool.push_back('\0');
        return off;
    }

    LiveNode* get_or_create(LiveNode& root, std::string_view prefix) {
        LiveNode* curr = &root;
        for (const char ch : prefix) {
            if (!std::isdigit(static_cast<unsigned char>(ch))) continue;
            auto& child = curr->children[ch];
            if (!child) child = std::make_unique<LiveNode>();
            curr = child.get();
        }
        return curr;
    }

public:
// parse geo+tz info from dataset
    void load_data(LiveNode& root, const std::string& path, bool is_tz) {
        std::ifstream file(path);
        if (!file) return;

        bool is_masterlist = (path.find("masterlist.txt") != std::string::npos); // known country codes

        for (std::string line; std::getline(file, line); ) {
            if (line.empty() || line[0] == '#') continue;
            if (const auto p = line.find('|'); p != std::string::npos) {
                std::string prefix_str = line.substr(0, p);
                std::string_view prefix = prefix_str;
                auto* node = get_or_create(root, prefix);
                auto val = line.substr(p + 1);
                if (is_tz) {
                    node->record.tz_off = add_to_pool(val);
                } else {
                    if (const auto c = val.find(','); c != std::string::npos) {
                        // city, state (nanp) format
                        node->record.city_off = add_to_pool(val.substr(0, c));
                        std::string_view state = (val.size() > c + 2) ? val.substr(c + 2) : "";
                        node->record.state_off = add_to_pool(state);
                    } else {
                        // append country to city for non-nanp entries
                        bool is_city = false;
                        if (!is_masterlist) {
                            if (!prefix.starts_with('1')) {
                                for (size_t len = 1; len < prefix.size(); ++len) {
                                    if (country_prefixes.count(std::string(prefix.substr(0, len)))) {
                                        is_city = true;
                                        break;
                                    }
                                }
                            }
                        }
                    if (is_city) {
                        node->record.city_off = add_to_pool(val);
                    } else {
                        node->record.state_off = add_to_pool(val);
                        if (is_masterlist) country_prefixes.insert(prefix_str);
                    }
                }
            }
        }
    }
}
    // flatten to a binary blob for fast memory-mapped lookups
    void flatten(LiveNode& root, const std::string& out_path) {
        std::vector<StaticNode> flat_nodes;
        std::vector<MetadataRecord> flat_records;
        std::vector<LiveNode*> queue = { &root };

        for (size_t i = 0; i < queue.size(); ++i) {
            LiveNode* l = queue[i];
            l->id = static_cast<int32_t>(flat_nodes.size());
            StaticNode sn;
            if (l->record.city_off != -1 || l->record.state_off != -1 || l->record.tz_off != -1) {
                sn.record_idx = static_cast<int32_t>(flat_records.size());
                flat_records.push_back(l->record);
            }
            flat_nodes.push_back(sn);
            for (auto const& [ch, child] : l->children) queue.push_back(child.get());
        }

        for (size_t i = 0; i < queue.size(); ++i) {
            for (auto const& [ch, child] : queue[i]->children)
                flat_nodes[i].children[ch - '0'] = child->id;
        }

        uint32_t crc = 0xFFFFFFFF;
        crc = calculate_crc32(flat_nodes.data(), flat_nodes.size() * sizeof(StaticNode), crc);
        crc = calculate_crc32(flat_records.data(), flat_records.size() * sizeof(MetadataRecord), crc);
        crc = calculate_crc32(string_pool.data(), string_pool.size(), crc);
        
        FileHeader head{};
        head.magic = 0x4D495455; // MITU
        head.version = S_VERSION;
        head.node_count = static_cast<uint32_t>(flat_nodes.size());
        head.record_count = static_cast<uint32_t>(flat_records.size());
        head.checksum = ~crc;

        std::ofstream out(out_path, std::ios::binary);
        out.write(reinterpret_cast<const char*>(&head), sizeof(head));
        out.write(reinterpret_cast<const char*>(flat_nodes.data()), flat_nodes.size() * sizeof(StaticNode));
        out.write(reinterpret_cast<const char*>(flat_records.data()), flat_records.size() * sizeof(MetadataRecord));
        out.write(string_pool.data(), string_pool.size());
    }
};

int main() {
    LiveNode root; 
    MapBuilder builder;
    const std::string geocode_path = "resources/geocoding/en/";

    builder.load_data(root, geocode_path + "masterlist.txt", false);
    // builder.load_data(root, geocode_path + "us-canada.txt", false);

    try {
        for (const auto& entry : fs::directory_iterator(geocode_path)) {
            if (!entry.is_regular_file()) continue;
            std::string filename = entry.path().stem().string();
            
            bool is_numeric = !filename.empty() && std::all_of(filename.begin(), filename.end(), ::isdigit);
            if (is_numeric) {
                builder.load_data(root, entry.path().string(), false);
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error reading geocoding directory: " << e.what() << "\n";
        return 1;
    }

    builder.load_data(root, "resources/geocoding/en/custom.txt", false);

    builder.load_data(root, "resources/timezones/map_data.txt", true);
    builder.load_data(root, "resources/timezones/custom_tz.txt", true);

    builder.flatten(root, "mitu.db");
    return 0;
}