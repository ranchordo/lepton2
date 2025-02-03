#pragma once

#include "../vulkancore/VulkanContext.h"

#define INPUT_HANDLER_BITMAP_SIZE ((GLFW_KEY_LAST + 7) / 8)

namespace lepton2::utils {

struct MousePosition {
    double x;
    double y;
};

class InputHandler {
   public:
    InputHandler(GLFWwindow* win) {
        this->win = win;
        memset(this->bitmap, 0x00, INPUT_HANDLER_BITMAP_SIZE);
    }

    bool handleEdge(int key, bool state) {
        int byteidx = key / 8;
        if (byteidx >= INPUT_HANDLER_BITMAP_SIZE) byteidx = INPUT_HANDLER_BITMAP_SIZE - 1;
        bool bit = (bitmap[byteidx] & (1 << (key & 7))) > 0;
        bool ret = (state && !bit);
        if (bit != state) {
            bitmap[byteidx] ^= 1 << (key & 7); // Flip bit
        }
        return ret;
    }

    bool keyPressed(int key) { return glfwGetKey(this->win, key); }
    bool keyTyped(int key) { return this->handleEdge(key, this->keyPressed(key)); }
    bool mousePressed(int key) { return glfwGetMouseButton(this->win, key); }
    bool mouseClicked(int key) { return this->handleEdge(key, this->mousePressed(key)); }

    MousePosition getMousePosition() {
        MousePosition ret;
        glfwGetCursorPos(this->win, &ret.x, &ret.y);
        return ret;
    }

   private:
    GLFWwindow* win;
    uint8_t bitmap[INPUT_HANDLER_BITMAP_SIZE];
};

}  // namespace lepton2::utils