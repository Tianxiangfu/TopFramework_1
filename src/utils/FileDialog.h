#pragma once
#include <string>

struct GLFWwindow;

namespace TopOpt {

class FileDialog {
public:
    static std::string openFile(GLFWwindow* parentWindow,
                                const char* title  = "Open Project",
                                const char* filter = "TopOpt Project (*.topopt)\0*.topopt\0All Files (*.*)\0*.*\0");

    static std::string saveFile(GLFWwindow* parentWindow,
                                const char* title      = "Save Project",
                                const char* filter     = "TopOpt Project (*.topopt)\0*.topopt\0All Files (*.*)\0*.*\0",
                                const char* defaultExt = "topopt");
};

} // namespace TopOpt
