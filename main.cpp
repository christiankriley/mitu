#include "mitu.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <format>
#include <memory>
#include <string_view>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace mitus;

enum class TimeFormat { H12, H24 };

class MappedFile {
    void* addr_{nullptr};
    size_t size_{0};
public:
    explicit MappedFile(const std::string& path) {
        int fd = open(path.c_str(), O_RDONLY);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0) {
                size_ = static_cast<size_t>(st.st_size);
                addr_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
            }
            close(fd);
        }
    }
    ~MappedFile() { if (addr_ && addr_ != MAP_FAILED) munmap(addr_, size_); }
    

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    [[nodiscard]] const void* data() const noexcept { return addr_; }
    [[nodiscard]] bool valid() const noexcept { return addr_ && addr_ != MAP_FAILED; }
};

class mituEngine {
    std::unique_ptr<MappedFile> file_;
    const StaticNode* nodes_{nullptr};
    const MetadataRecord* recs_{nullptr};
    const char* pool_{nullptr};
    TimeFormat time_format_{TimeFormat::H24}; // default to 24h

public:
    void setTimeFormat(TimeFormat fmt) { time_format_ = fmt; }

    bool init(const std::string& path) {
        file_ = std::make_unique<MappedFile>(path);
        if (!file_->valid()) return false;
        
        const auto* h = static_cast<const FileHeader*>(file_->data());
        
        nodes_ = reinterpret_cast<const StaticNode*>(h + 1);
        recs_ = reinterpret_cast<const MetadataRecord*>(nodes_ + h->node_count);
        pool_ = reinterpret_cast<const char*>(recs_ + h->record_count);
        return true;
    }

    void lookup(std::string_view num) const {
        // we are starting w/ sanitized num, E.164 max length is 15 digits
        if (num.length() > 15) {
            std::cout << "Invalid number: Too long.\n";
            return;
        }
        
        int32_t curr_node_idx = 0; 
        const MetadataRecord* last_loc_rec = nullptr;
        const MetadataRecord* last_tz_rec = nullptr;

        for (const char c : num) {
            const int digit = c - '0';
            const int32_t next_node_idx = nodes_[curr_node_idx].children[digit];
            
            if (next_node_idx == -1) break;
            
            curr_node_idx = next_node_idx;
            
            if (nodes_[curr_node_idx].record_idx != -1) {
                const auto* current_rec = &recs_[nodes_[curr_node_idx].record_idx];
                
                if (current_rec->city_off != -1 || current_rec->state_off != -1) {
                    last_loc_rec = current_rec;
                }
                
                if (current_rec->tz_off != -1) {
                    last_tz_rec = current_rec;
                }
            }
        }

        if (!last_loc_rec && !last_tz_rec) {
            std::cout << "No data found for this number.\n";
            return;
        }

        auto get_s = [&](int32_t off) -> std::string_view {
            return (off == -1) ? "Unknown" : std::string_view(pool_ + off);
        };

        std::cout << "(o>\n";
        if (last_loc_rec) {
            std::cout << "Location: " << get_s(last_loc_rec->city_off) 
                      << ", " << get_s(last_loc_rec->state_off) << "\n";
        }

        if (last_tz_rec) {
            const auto tz_name = get_s(last_tz_rec->tz_off);
            std::cout << "Timezone: " << tz_name << "\n";
            
            try {
                const auto now = std::chrono::system_clock::now();
                const auto zoned = std::chrono::zoned_time{std::string(tz_name), now};
                
                if (time_format_ == TimeFormat::H24) {
                    std::cout << "Local Time: " << std::format("{:%H:%M}", zoned) << "\n";
                } else {
                    std::cout << "Local Time: " << std::format("{:%I:%M %p}", zoned) << "\n";
                }

            } catch (...) {
                std::cout << "Local Time: Calculation error (Check system tzdata)\n";
            }
        } else {
            std::cout << "Timezone: N/A\n";
        }
    }
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cout << "Usage: ./mitu <phone_number>\n";
        return 1;
    }

    std::string sanitized;
    bool leading_zeros = true; // assume there may be leading zeros

    for (int i = 1; i < argc; ++i) {
        for (char c : std::string_view(argv[i])) {
            // skip non-digit chars
            if (std::isdigit(static_cast<unsigned char>(c))) {
                // skip leading zeros
                if (leading_zeros && c == '0') {
                    continue;
                }
                leading_zeros = false;
                sanitized += c;
            }
        }
    }

    mituEngine engine;
    engine.setTimeFormat(TimeFormat::H12);
    if (engine.init("mitu.db")) {
        engine.lookup(sanitized);
    } else {
        std::cerr << "Error: Could not initialize mitu.db\n";
        return 1;
    }

    return 0;
}