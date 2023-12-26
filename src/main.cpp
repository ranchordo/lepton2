#include "lepton2/vulkancore/VulkanUtils.h"
#include "lepton2/vulkancore/VulkanMemory.h"
#include "lepton2/vulkancore/VulkanContext.h"
#include "lepton2/vulkancore/Shader.h"

using namespace lepton2::vulkancore;

VulkanContext* vkctx;

int main() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed GLFW initialization.");
    }
    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan main", nullptr, nullptr);
    VkApplicationInfo appInfo {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan test of some sort";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "None yet";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;
    vkctx = new VulkanContext(true, true, appInfo, window);

    Shader shader(vkctx, "shader");

    vkctx->destroy(vkctx);
    delete vkctx;
    return 0;
}