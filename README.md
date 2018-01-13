# RCQ Engine 

This real-time outdoor graphics engine uses the [Vulkan API](https://vulkan.lunarg.com/) to render 

* physically based materials,

* sky with dynamic day and night cycle featuring Rayleigh and Mie scattering and ozone absorption, 

* ocean surface using offline generated noise and real-time Fast Fourier Transform,

* terrain with precomputed height map, gradient map and texture mask using virtual texturing. 

The engine is written in C and low-level C++17 using Visual Studio 2017, has its own polymorphic memory resources and containers. The shaders are written in GLSL. Other libraries the engine uses are [stb\_image](https://github.com/nothings/stb/blob/master/stb_image.h) and [glm](https://glm.g-truc.net/0.9.8/index.html).

## Concepts, architecture

This engine is the result of a self-learning process I have started in August, 2017 in which I’ve learned (and still learning) the basics of real-time graphics programming, deepened my knowledge in low level programming in C/C++17 and learned to use the Vulkan API effectively. For learning reasons I tried to make the engine as low level as possible (within reasonable bounds).

The engine is of course still under development, some parts have to be improved (e.g. the shaders), and new features have to be added. However, despite the limitations I think it is a good starting point for me to dive into graphics programming.

For consistent memory management I avoided the usage of STL containers, wrote my own polymorphic memory resources and containers. (During the development of the engine the std::pmr namespace was not available with the MSVC compiler.) There are memory resources for host and device memory separately, although the containers support only host memory resources. The containers are low level in a sense that they handle only the underlying memory of the object, they don’t call the constructors, destructor or operator= at all. The user can call the constructors and destructor manually of course, and internally the containers use memcpy for copying. The containers support move constructor and move assignment.

The rendering involves two types of objects: *resources *and* renderables*. The *resources* contain the actual data (or rather the handles to them) used for rendering. For example meshes, opaque materials, transforms, precomputed sky maps are *resources*. The *renderables* are "abstract" objects which participate in the rendering. They contain handles for data available through *resources*; e.g. an opaque object renderable contains data handles available through a mesh, an opaque material and a transform resources, a sky renderable contains data handles available through a sky resource. For obvious reasons, the engine can have only one sky, ocean and terrain *renderables *at a time, but can have many sky, ocean and terrain *resources*.

The engine consist of four singleton classes: *base*, *resource_manager, engine *and *terrain_manager*. The *base* is responsible for initializing window and the basic Vulkan objects e.g. instance, device, queues, swapchain. The *resource_manager* is responsible for building and destroying the *resources*. It has two threads, one for building and one for destroying. The *engine* is responsible for the actual rendering, it creates uses and destroys the object necessary for rendering (e.g. render passes, graphics/compute pipelines, G-buffer, shadow map image). The *terrain_manager* handles the virtual texture system of the terrain texture. It has a thread which processes the load and delete requests coming from the GPU.

The interface of the engine is quite simple, the user is able to build, destroy and update resources and renderables.

*(Note: beside the engine, I made another program for producing the necessary precomputed data for the engine, namely the sky map, the terrain map and the noise for the ocean.)*

## Rendering

For designing the rendering pipeline I used some idea from [this GTA V graphics study](http://www.adriancourreges.com/blog/2015/11/02/gta-v-graphics-study/). Of course, this part of the engine changes the most frequently, I add new features, try to improve existing ones. In the followings I will describe the process of rendering very briefly. I won’t include features that I’m currently working on since they are not entirely parts of the pipeline yet.

In each frame the rendering pipeline does the followings:

1. A data buffer (containing data such as projection matrix, light direction, wind speed etc) is updated.

2. The two compute pipelines regarding the terrain and the ocean are executed.

3. Four shadow maps are generated. (Cascade shadow mapping.)

4. The opaque objects and the terrain are rendered to the G-buffer.

5. The screen-space shadow map and the SSAO map are generated and then blurred.

6. The images of the G-buffer (and the sky map for reflections) are combined.

7. The sky is drawn.

8. The sun is drawn.

9. The refraction image is generated.

10. The ocean is drawn.

11. The bloom effect is added.

12. Tone mapping is applied.

## Graphics features

In the rest of this document I briefly describe some (the most interesting) features of the engine or still under development and indicate the relevant sources (web pages, articles, books). 

### Physically based material

The engine uses PBR to render opaque materials mainly based on the PBR section at [learnopengl](https://learnopengl.com/). Some parameters (base color, metalness, roughness) can be provided by scalars as well as by textures. Some keywords are normal mapping, Fresnel-Schlick approximation, Cook-Torrance BRDF, parallax occlusion mapping. I mainly learned the basic concepts of BRDF theory from the book [Real-Time Rendering](http://www.realtimerendering.com/).

### Physically based sky

For sky rendering I use a precomputed 3D sky map (more precisely, two sky maps, one for Rayleigh and one for Mie scattering) and a 2D transmittance map. The precomputation is implemented based on the ideas in [this](https://media.contentapi.ea.com/content/dam/eacom/frostbite/files/s2016-pbs-frostbite-sky-clouds-new.pdf), [this](http://old.cescg.org/CESCG-2009/papers/PragueCUNI-Elek-Oskar09.pdf), and [this](https://software.intel.com/sites/default/files/blog/473591/outdoor-light-scattering-update_1.pdf) document . For integration over the sphere I use the [Lebedev quadrature](https://en.wikipedia.org/wiki/Lebedev_quadrature) method, the file containing the grid points can be found [here](http://people.sc.fsu.edu/~jburkardt/datasets/sphere_lebedev_rule/sphere_lebedev_rule.html).

### Cascaded shadow mapping

This is a well-known technique to implement shadows. I didn’t use any specific source for it.

### Real-time ocean rendering with Fast-Fourier Transform

The water rendering system consist of two pipelines. The first one is compute pipeline based on [Tessendorf's work](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.161.9102&rep=rep1&type=pdf) which executes the FFT, i.e. computes the height and the gradient map from a noise texture. The second one is a graphics pipeline which executes the drawing using refraction map and single scattering method.

### Terrain rendering with virtual texturing

Since the Vulkan API supports sparse images, virtual texturing can be implemented conveniently with it. The engine’s virtual texturing system uses a precomputed terrain map which contains information about the height, gradient and texture mask. It is generated using Perlin-noise. The terrain map also have (precomputed) mip levels. There is two pipeline associated with terrain rendering. The first one is a compute pipeline which requests the load or deletion of the appropriate mip level for a given tile (then the requests are processed by the terrain manager). The second pipeline is responsible for the drawing of the terrain. Beside the virtual texture of the terrain it also uses four opaque materials for texturing.

### Aerial perspective effect

This effect is added during the combination of the G-buffer and the shading of the ocean. The transmittance coefficient is computed in the same way as in the computation of the sky map, but in this case the give integral can be computed explicitly since because of the smaller scales we can assume that the height function is the *y coordinate*. The volume scattering part of the effect is computed from the sky map.

### Environment cubemap rendering *(under development)*

For realistic reflections it’s good to use environment maps (possibly with distance maps). The engine has a graphics pipeline which can generate environment map centered at the viewpoint in real-time, but currently it’s not used and hasn’t been tested.

### Screen space reflection *(under development)*

For all pixel the SSR pipeline of the engine draws a ray starting from the appropriate position and uses the rasterizer to find the first intersection. It is being tested currently but it seems to have serious impact on the performance.

### Planar reflection *(under development)*

One of the best ways for water reflection is using planar reflection. However rendering a screen two times can be performance heavy, so some "hacks" will probably be necessary.

