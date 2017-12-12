#include "rcq_engine.h"
#include "scene.h"
#include "structs.h"
#include "entity.h"

#include <iostream>

int main()
{
	try
	{ 
		rcq_user::init();

		GLFWwindow* window = rcq_user::get_window();
		//auto sc = new scene(window, rcq::engine::instance()->get_window_size());
		//rcq::timer t;
		//t.start();
		while (!glfwWindowShouldClose(window))
		{
			//t.stop();
			//float dt = t.get();
			//std::cout << dt << std::endl; 
			//t.start();
			glfwPollEvents();
			//sc->update(dt);
		}
		//delete sc;
		rcq_user::destroy();
  		return EXIT_SUCCESS;
	}
	catch (const std::runtime_error& e) 
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

}