cmake_minimum_required(VERSION 3.2 FATAL_ERROR)
project(mm_ogl_tf CXX)

add_executable(mm_ogl_tf_flow
	./render_tasks/tf_particles.hpp
	./render_tasks/tf_particles.cpp

	./flow.cpp
)

target_link_libraries(mm_ogl_tf_flow
	engine
	random

	opengl_renderer_s
	imgui_service

	imgui_render_task
	imgui_tools

	imgui_opengl_renderer_tools
)

if(EMSCRIPTEN)
	set_target_properties(mm_ogl_tf_flow PROPERTIES COMPILE_FLAGS "-s USE_SDL=2")
	set_target_properties(mm_ogl_tf_flow PROPERTIES LINK_FLAGS "-s USE_SDL=2 -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s FULL_ES2=1 -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1")
endif()

