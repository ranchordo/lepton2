#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>

#include "../vulkancore/ObjectData.h"

namespace lepton2::utils {

namespace vkc = lepton2::vulkancore;

typedef std::chrono::steady_clock::time_point lepton2_time_point;

extern lepton2_time_point startTiming();
extern double getElapsedSeconds(lepton2_time_point time_point);
extern std::filesystem::path getExecutableLocation(const char* argv0, bool force_absolute);
extern std::vector<char> readFile(const std::string& filename);
extern vkc::HostObjectData* loadObjFile(vkc::VulkanContext* ctx, const char* filename);

} // namespace lepton2::utils