#pragma once

#include <GLFW/glfw3.h>
#include <webgpu/webgpu.h>

class Application
{
public:
	bool Initialize();
	void Terminate();
	void MainLoop();
	bool IsRunning();
private:
	GLFWwindow* m_window = nullptr;
};