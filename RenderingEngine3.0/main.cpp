#include "engine.h"
#include "scene.h"
#include "structs.h"
#include "entity.h"

#include <iostream>

int main()
{
	/*try
	{ */
		rcq::engine::init();
		GLFWwindow* window = rcq::engine::instance()->get_window();
		auto sc = new scene(window, rcq::engine::instance()->get_window_size());
		auto time = std::chrono::high_resolution_clock::now();
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			auto now = std::chrono::high_resolution_clock::now();
			float dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - time).count()/1000.f;
			std::cout << dt << std::endl;
			time = now;
			sc->update(dt);
		}
		delete sc;
		rcq::engine::destroy();
		return EXIT_SUCCESS;
	/*}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}*/

}