#include "mitu.hpp"
#include <iomanip>
#include <iostream>
#include <string>
#include <chrono>
#include <format>
#include <memory>
#include <string_view>
#include <cctype>
#include <cstring>
#include <unordered_map>
#include <limits>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace mitus;

enum class TimeFormat { H12, H24 };

class MappedFile {
    void* addr_{nullptr};
     size_t size_{0};
public:
    explicit MappedFile(const std::string& path) {
        #ifdef _WIN32
        HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER fs;
            if (GetFileSizeEx(hFile, &fs)) {
                size_ = static_cast<size_t>(fs.QuadPart);
                HANDLE hMap = CreateFileMappingA(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
                if (hMap) {
                    addr_ = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
                    CloseHandle(hMap);
                }
            }
            CloseHandle(hFile);
        }
        #else
        int fd = open(path.c_str(), O_RDONLY);
        if (fd >= 0) {
            struct stat st;
            if (fstat(fd, &st) == 0) {
                size_ = static_cast<size_t>(st.st_size);
                addr_ = mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd, 0);
            }
            close(fd);
        }
        #endif
    }
    ~MappedFile() { 
        #ifdef _WIN32
        if (addr_) UnmapViewOfFile(addr_);
        #else
        if (addr_ && addr_ != MAP_FAILED) munmap(addr_, size_); 
        #endif
    }
    

    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    [[nodiscard]] const void* data() const noexcept { return addr_; }
    [[nodiscard]] bool valid() const noexcept {
        #ifdef _WIN32
        return addr_ != nullptr;
        #else
        return addr_ && addr_ != MAP_FAILED; 
        #endif
    }
    [[nodiscard]] size_t size() const noexcept { return size_; }
};

class mituEngine {
    std::unique_ptr<MappedFile> file_;
    const StaticNode* nodes_{nullptr};
    const MetadataRecord* recs_{nullptr};
    const char* pool_{nullptr};

    uint32_t node_count_{0};
    uint32_t record_count_{0};
    size_t pool_size_{0};

    TimeFormat time_format_{TimeFormat::H24}; // default to 24h
    bool measure_performance_{true}; // output operation time in ms for each lookup

    #ifdef _WIN32
    LARGE_INTEGER qpc_freq_{};
    #endif

    std::unordered_map<std::string_view, const std::chrono::time_zone*> tz_cache_;

    std::string_view get_s(int32_t off) const {
        if (off == -1 || static_cast<size_t>(off) >= pool_size_) return "Unknown";
        const char* start = pool_ + off;
        const char* end = static_cast<const char*>(memchr(start, '\0', pool_size_ - off));
        return (!end) ? "Unknown" : std::string_view(start, static_cast<size_t>(end - start));
    }

public:
    void setTimeFormat(TimeFormat fmt) { time_format_ = fmt; }
    void setMeasurePerformance(bool measure) { measure_performance_ = measure; }

    bool init(const std::string& path) {
        file_ = std::make_unique<MappedFile>(path);
        if (!file_->valid()) return false;

        if (reinterpret_cast<uintptr_t>(file_->data()) % alignof(int32_t) != 0) {
            return false;
        }

        const size_t fileSize = file_->size();
        if (fileSize < sizeof(FileHeader)) return false;

        FileHeader h;
        std::memcpy(&h, file_->data(), sizeof(FileHeader));

        // INTEGRITY CHECK START

        if (h.magic != 0x4D495455) {
            std::cerr << "Not a valid MITU DB file (bad magic)\n";
            return false;
        }

        if (h.version != S_VERSION) {
            std::cerr << std::format("Unsupported MITU DB version: {} (Expected {})\n", h.version, S_VERSION);
            return false;
        }

        const uint8_t* data_ptr = static_cast<const uint8_t*>(file_->data()) + sizeof(FileHeader);
        size_t data_len = file_->size() - sizeof(FileHeader);
        uint32_t actual_crc = calculate_crc32(data_ptr, data_len);

        if (h.checksum != ~actual_crc) {
            std::cerr << "Checksum mismatch! DB may be corrupted.\n";
            return false;
        }

        // INTEGRITY CHECK END

        node_count_ = h.node_count;
        record_count_ = h.record_count;

        if (node_count_ > 0 && sizeof(StaticNode) > std::numeric_limits<size_t>::max() / node_count_) {
            return false;
        }

        if (record_count_ > 0 && sizeof(MetadataRecord) > std::numeric_limits<size_t>::max() / record_count_) {
            return false;
        } 

        const size_t nodes_size = static_cast<size_t>(node_count_) * sizeof(StaticNode);
        const size_t recs_size = static_cast<size_t>(record_count_) * sizeof(MetadataRecord);
        
        if (std::numeric_limits<size_t>::max() - sizeof(FileHeader) < nodes_size ||
        std::numeric_limits<size_t>::max() - (sizeof(FileHeader) + nodes_size) < recs_size) {
            return false;
        }

        const size_t required_min = sizeof(FileHeader) + nodes_size + recs_size;

        if (required_min > fileSize || node_count_ == 0) return false;

        const char* base = static_cast<const char*>(file_->data());
        nodes_ = reinterpret_cast<const StaticNode*>(base + sizeof(FileHeader));
        recs_ = reinterpret_cast<const MetadataRecord*>(base + sizeof(FileHeader) + nodes_size);
        pool_ = base + required_min;
        pool_size_ = fileSize - required_min;

        #ifdef _WIN32
        QueryPerformanceFrequency(&qpc_freq_);
        const auto now = std::chrono::system_clock::now();
        #endif

        // pre-cache timezone pointers for faster lookups
        for (uint32_t i = 0; i < record_count_; ++i) {
            const auto& rec = recs_[i];
            if (rec.tz_off != -1) {
                std::string_view full_tz = get_s(rec.tz_off);
                std::string_view tz_name = full_tz;
                if (auto pos = full_tz.find('&'); pos != std::string_view::npos) {
                    tz_name = full_tz.substr(0, pos);
                }
                if (tz_cache_.find(tz_name) == tz_cache_.end()) {
                    try {
                        auto* tz = std::chrono::locate_zone(std::string(tz_name));
                        #ifdef _WIN32
                        (void)tz->get_info(now);
                        #endif
                        tz_cache_[tz_name] = tz;
                    } catch (...) {
                        tz_cache_[tz_name] = nullptr; // mark as invalid
                    }
                }
            }
        }
        return true;
    }

    void lookup(std::string_view num) const {
        // PERFORMANCE MEASUREMENT START
        #ifdef _WIN32
        LARGE_INTEGER start_qpc;
        QueryPerformanceCounter(&start_qpc);
        #else
        const auto start_time = std::chrono::steady_clock::now();
        #endif

        auto finalize_lookup = [&](const std::string& output) {
            #ifdef _WIN32
            LARGE_INTEGER end_qpc;
            QueryPerformanceCounter(&end_qpc);
            double duration_ms = static_cast<double>(end_qpc.QuadPart - start_qpc.QuadPart) * 1000.0 / qpc_freq_.QuadPart;
            #else
            const auto end_time = std::chrono::steady_clock::now();
            const auto duration_us = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
            const double duration_ms = duration_us / 1000.0;
            #endif

            std::cout << output;

            if (measure_performance_) {
                std::cout << "Returned in " << std::fixed << std::setprecision(4) << duration_ms << " ms\n";
            }
        };

        // PERFORMANCE MEASUREMENT END

        // we are starting w/ sanitized num, E.164 max length is 15 digits
        if (num.length() > 15) {
            std::cout << "Invalid number: Too long.\n";
            return;
        }
        
        int32_t curr_node_idx = 0; 
        const MetadataRecord* last_loc_rec = nullptr;
        const MetadataRecord* last_tz_rec = nullptr;
        int32_t current_city_off = -1;
        int32_t current_state_off = -1;

        for (const char c : num) {
            const int digit = c - '0';
            if (digit < 0 || digit > 9) break; // sanity check
            
            const int32_t next_node_idx = nodes_[curr_node_idx].children[digit];
            
            if (next_node_idx == -1 || static_cast<uint32_t>(next_node_idx) >= node_count_) break;
            
            curr_node_idx = next_node_idx;
            const int32_t rec_idx = nodes_[curr_node_idx].record_idx;

            if (rec_idx != -1 && static_cast<uint32_t>(rec_idx) < record_count_) {
                const auto* current_rec = &recs_[rec_idx];
                
                if (current_rec->city_off != -1) current_city_off = current_rec->city_off;
                if (current_rec->state_off != -1) current_state_off = current_rec->state_off;
                
                if (current_rec->tz_off != -1) {
                    last_tz_rec = current_rec;
                }
            }
        }

        if (!last_loc_rec && !last_tz_rec) {
            std::cout << "No data found for this number.\n";
            return;
        }

        std::string output;
        output.reserve(256);
        output += "(o>\n";

        if (current_city_off != -1 || current_state_off != -1) {
            std::string_view city = get_s(current_city_off);
            std::string_view state = get_s(current_state_off);

            #ifdef _WIN32
            output.append("Location: ");
            if (current_city_off != -1 && current_state_off != -1) {
                output.append(city).append(", ").append(state);
            } else {
                output.append(current_state_off != -1 ? state : city);
            }
            output.append("\n");
            #else
            if (current_city_off != -1 && current_state_off != -1) {
                output += std::format("Location: {}, {}\n", city, state);
            } else {
                output += std::format("Location: {}\n", (current_state_off != -1) ? state : city);
            }
            #endif
        }

        if (last_tz_rec) {
            const std::string_view tz_full = get_s(last_tz_rec->tz_off);
            std::string_view tz_name = tz_full;
            // only display one timezone name
            if (auto pos = tz_full.find('&'); pos != std::string_view::npos) {
                tz_name = tz_full.substr(0, pos);
            }

            #ifdef _WIN32
            output.append("Timezone: ").append(tz_name).append("\n");
            #else
            output += std::format("Timezone: {}\n", tz_name);
            #endif

            auto it = tz_cache_.find(tz_name);

            if (it != tz_cache_.end() && it->second != nullptr) {
                try {
                    const auto now = std::chrono::system_clock::now();
                    const auto zoned = std::chrono::zoned_time{it->second, now};
    
                    #ifdef _WIN32
                    const auto local_time = zoned.get_local_time();
                    const auto days = std::chrono::floor<std::chrono::days>(local_time);
                    const std::chrono::hh_mm_ss hms{local_time - days};

                    if (time_format_ == TimeFormat::H24) {
                        output += std::format("Local Time: {:02}:{:02}\n", hms.hours().count(), hms.minutes().count());
                    } else {
                        int h = hms.hours().count();
                        const char* suffix = (h >= 12) ? "PM" : "AM";
                        h = (h % 12 == 0) ? 12 : h % 12;
                        output += std::format("Local Time: {:02}:{:02} {}\n", h, hms.minutes().count(), suffix);
                    }

                    #else
                    if (time_format_ == TimeFormat::H24) {
                        output += std::format("Local Time: {:%H:%M}\n", zoned);
                    } else {
                        output += std::format("Local Time: {:%I:%M %p}\n", zoned);
                    }
                    #endif
                } catch (...) {
                    output += "Local Time: Calculation error (Check system tzdata)\n";
                }
            } else {
                output += "Timezone: N/A\n";
            }
        }
        finalize_lookup(output);
    }
};

int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(NULL);

    if (argc < 2) {
        std::cout << "Usage: ./mitu <phone_number> or --version\n";
        return 1;
    }

    std::string_view arg = argv[1];
    if (arg == "--version" || arg == "-v") {
        std::cout << "mitu v" << VERSION << "\n";
        std::cout << "db schema v" << S_VERSION << "\n";
        std::cout << "offline phone number info lookup tool\n";
        return 0;
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
    
    bool measurePerformance = true;
    engine.setTimeFormat(TimeFormat::H12);
    engine.setMeasurePerformance(measurePerformance);

    if (engine.init("mitu.db")) {
        engine.lookup(sanitized);
    } else {
        std::cerr << "Error: Could not initialize mitu.db\n";
        return 1;
    }

    return 0;
}