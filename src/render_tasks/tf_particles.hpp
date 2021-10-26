#pragma once

#include <mm/opengl/render_task.hpp>
#include <mm/services/opengl_renderer.hpp>

#include <mm/opengl/camera_3d.hpp>

//#include <glm/fwd.hpp>
#include <glm/vec4.hpp>

#include <mm/opengl/instance_buffer.hpp>

// fwd
namespace MM::OpenGL {
	class Shader;
	class Buffer;
	class VertexArrayObject;
}

namespace MM::OpenGL::RenderTasks {

	class TFParticles : public RenderTask {
		private:
			std::shared_ptr<Shader> _tf_shader;
			std::shared_ptr<Shader> _points_shader;

			// double buffered
			bool first_particles_buffer = true;
			std::unique_ptr<VertexArrayObject> _tf_vao[2];
			std::unique_ptr<VertexArrayObject> _render_vao[2];

			std::unique_ptr<MM::OpenGL::InstanceBuffer<glm::vec3>> _particles_pos_buffers[2];
			std::unique_ptr<MM::OpenGL::InstanceBuffer<glm::vec3>> _particles_vel_buffers[2];

			float time{0};

		public:
			glm::vec3 env_vec{0, 1, 0};
			float env_force{0.3};
			float noise_force{0.5};
			float dampening{0.99};
			float point_size{1};

			OpenGL::Camera3D default_cam;

			TFParticles(Engine& engine);
			~TFParticles(void);

			const char* name(void) override { return "TransformFeedbackParticles"; }

			void computeParticles(Services::OpenGLRenderer& rs, Engine& engine);
			void renderParticles(Services::OpenGLRenderer& rs, Engine& engine);
			void render(Services::OpenGLRenderer& rs, Engine& engine) override;

		public:
			const char* vertexPathTF = "shader/tf_particles/tf_vert.glsl";
			const char* fragmentPathTF = "shader/tf_particles/tf_frag.glsl"; // "empty" bc of webgl (es)

			const char* vertexPathPoints = "shader/tf_particles/point_vert.glsl";
			const char* fragmentPathPoints = "shader/tf_particles/point_frag.glsl";

			std::string target_fbo = "display";

		private:
			void setupShaderFiles(void);
	};

} // MM::OpenGL::RenderTasks

