#include <mm/engine.hpp>

#include <mm/services/filesystem.hpp>
#include <mm/services/sdl_service.hpp>
#include <mm/services/opengl_renderer.hpp>
#include <mm/services/imgui_s.hpp>

#include <mm/services/imgui_menu_bar.hpp>
#include <mm/services/engine_tools.hpp>
#include <mm/services/opengl_renderer_tools.hpp>

#include "./render_tasks/tf_particles.hpp"
#include <mm/opengl/render_tasks/imgui.hpp>

#include <mm/logger.hpp>

#define ENABLE_BAIL(x) if (!x) return false;
bool setup(MM::Engine& engine, const char* argv_0) {
	auto& sdl_ss = engine.addService<MM::Services::SDLService>(
		SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS
	);
	ENABLE_BAIL(engine.enableService<MM::Services::SDLService>());

	sdl_ss.createGLWindow("OpenGL TransformFeedback Particles flow", 1280, 720, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	engine.addService<MM::Services::FilesystemService>(argv_0, "mm_ogl_tf_flow");
	ENABLE_BAIL(engine.enableService<MM::Services::FilesystemService>());

	engine.addService<MM::Services::ImGuiService>();
	ENABLE_BAIL(engine.enableService<MM::Services::ImGuiService>());

	engine.addService<MM::Services::ImGuiMenuBar>();
	ENABLE_BAIL(engine.enableService<MM::Services::ImGuiMenuBar>());

	engine.addService<MM::Services::ImGuiEngineTools>();
	ENABLE_BAIL(engine.enableService<MM::Services::ImGuiEngineTools>());

	engine.addService<MM::Services::ImGuiOpenGLRendererTools>();
	ENABLE_BAIL(engine.enableService<MM::Services::ImGuiOpenGLRendererTools>());

	auto& rs = engine.addService<MM::Services::OpenGLRenderer>();
	ENABLE_BAIL(engine.enableService<MM::Services::OpenGLRenderer>());

	rs.addRenderTask<MM::OpenGL::RenderTasks::TFParticles>(engine);
	rs.addRenderTask<MM::OpenGL::RenderTasks::ImGuiRT>(engine);

	return true;
}

static MM::Engine engine;

int main(int argc, char** argv) {
	assert(argc > 0);
	if (!setup(engine, argv[0])) {
		SPDLOG_ERROR("setting up the engine");
		return 1;
	}

	engine.run();

	engine.cleanup();
	return 0;
}

