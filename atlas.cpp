#include "mitu.h"
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <map>
#include <memory>
#include <algorithm>

using namespace mitus;

// use a trie to follow digits so we don't have to look at everything
struct LiveNode {
    std::map<char, std::unique_ptr<LiveNode>> children;
    MetadataRecord record;
    int32_t id{-1};
};

class MapBuilder {
    // avoid costly string objects by using a single pool
    std::string string_pool;

    int32_t add_to_pool(std::string_view s) {
        if (s.empty()) return -1;
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
        for (std::string line; std::getline(file, line); ) {
            if (line.empty() || line[0] == '#') continue;
            if (const auto p = line.find('|'); p != std::string::npos) {
                auto* node = get_or_create(root, line.substr(0, p));
                auto val = line.substr(p + 1);
                if (is_tz) {
                    node->record.tz_off = add_to_pool(val);
                } else {
                    if (const auto c = val.find(','); c != std::string::npos) {
                        node->record.city_off = add_to_pool(val.substr(0, c));
                        node->record.state_off = add_to_pool(val.substr(c + 2));
                    } else {
                        node->record.state_off = add_to_pool(val);
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
            if (l->record.city_off != -1 || l->record.tz_off != -1) {
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

        std::ofstream out(out_path, std::ios::binary);
        FileHeader head{(uint32_t)flat_nodes.size(), (uint32_t)flat_records.size()};
        out.write(reinterpret_cast<const char*>(&head), sizeof(head));
        out.write(reinterpret_cast<const char*>(flat_nodes.data()), flat_nodes.size() * sizeof(StaticNode));
        out.write(reinterpret_cast<const char*>(flat_records.data()), flat_records.size() * sizeof(MetadataRecord));
        out.write(string_pool.data(), string_pool.size());
    }
};

int main() {
    LiveNode root; MapBuilder builder;
    builder.load_data(root, "resources/geocoding/en/1.txt", false);
    builder.load_data(root, "resources/geocoding/en/custom.txt", false);
    builder.load_data(root, "resources/timezones/map_data.txt", true);
    builder.load_data(root, "resources/timezones/custom_tz.txt", true);
    builder.flatten(root, "mitu.db");
    return 0;
}