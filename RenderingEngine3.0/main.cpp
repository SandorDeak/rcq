#include "rcq_engine.h"
//#include "scene.h"
//#include "structs.h"
//#include "entity.h"

#include <iostream>

int main()
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
	return 0;
}