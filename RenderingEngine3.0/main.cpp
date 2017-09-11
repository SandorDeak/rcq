#include "engine.h"
#include "scene.h"
#include "structs.h"
#include "entity.h"

#include <iostream>

int main()
{
	int k;
	std::cout << "changes for git tutorial" << std::endl;
	try
	{ 
		rcq::engine::init();
		scene sc;
		GLFWwindow* window=rcq::engine::instance()->get_window();
		auto time = std::chrono::high_resolution_clock::now();
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			//render should work on a different thread
			auto now = std::chrono::high_resolution_clock::now();
			float dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count()/1000.f;
			time = now;
			sc.update(dt);
		}
		rcq::engine::destroy();
		return EXIT_SUCCESS;
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

}