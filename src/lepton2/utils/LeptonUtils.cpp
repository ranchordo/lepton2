#include "LeptonUtils.h"

namespace lepton2::utils {

lepton2_time_point startTiming() {
    return std::chrono::steady_clock::now();
}

double getElapsedSeconds(lepton2_time_point time_point) {
    lepton2_time_point now = startTiming();
    std::chrono::duration<double> interval = now - time_point;
    return interval.count();
}

std::filesystem::path getExecutableLocation(const char* argv0, bool force_absolute) {
    std::filesystem::path relative_path = std::filesystem::path(argv0).parent_path();
    if (!force_absolute) {
        return relative_path;
    }
    return std::filesystem::absolute(relative_path);
}

std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        printf("Attempted to open file %s\n", filename.c_str());
        throw std::runtime_error("Could not open miscellaneous file for reading.");
    }
    size_t fsize = (size_t)file.tellg();
    std::vector<char> buffer(fsize);
    file.seekg(0);
    file.read(buffer.data(), fsize);
    file.close();
    return buffer;
}

} // namespace lepton2::utils