#include "scene.h"

#include <iostream>

int main()
{
	rcq_user::init();

	GLFWwindow* window = rcq_user::get_window();
	auto sc = new scene(window, rcq_user::get_window_size());
	rcq_user::timer t;
	t.start();
	while (!glfwWindowShouldClose(window))
	{
		t.stop();
		float dt = t.get();
		//std::cout << dt << std::endl; 
		t.start();
		glfwPollEvents();
		sc->update(dt);
	}
	delete sc;

	rcq_user::destroy();
	return 0;
}