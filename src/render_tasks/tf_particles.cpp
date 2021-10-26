#include "./tf_particles.hpp"

#include <memory>

#include <mm/opengl/shader.hpp>
#include <mm/opengl/shader_builder.hpp>
#include <mm/opengl/buffer.hpp>
#include <mm/opengl/vertex_array_object.hpp>

#include <mm/fs_const_archiver.hpp>

#include <mm/engine.hpp>

#include <mm/services/scene_service_interface.hpp>
#include <entt/entity/registry.hpp>

#include <mm/services/opengl_renderer.hpp>

#include <mm/random/srng.hpp>

#include <glm/geometric.hpp>
#include <imgui/imgui.h>

#include <tracy/Tracy.hpp>
#ifndef MM_OPENGL_3_GLES
	#include <tracy/TracyOpenGL.hpp>
#else
	#define TracyGpuContext
	#define TracyGpuCollect
	#define TracyGpuZone(...)
#endif

#include <mm/logger.hpp>

namespace MM::OpenGL::RenderTasks {

static const size_t __num_part = 1'000'000;
//static const size_t __num_part = 1'000;

static void __set_pos(const std::unique_ptr<InstanceBuffer<glm::vec3>>& buffer) {
	auto* data = buffer->map(__num_part, GL_STATIC_COPY);
	MM::Random::SRNG rng{1337u, 0};
	for (size_t i = 0; i < __num_part; i++) {
		data[i] = glm::vec3(rng.negOneToOne() * 50, rng.negOneToOne() * 50 * (9.f/16.f), 0);
	}
	buffer->unmap();
}

static void __set_vel(const std::unique_ptr<InstanceBuffer<glm::vec3>>& buffer) {
	auto* data = buffer->map(__num_part, GL_STATIC_COPY);
	for (size_t i = 0; i < __num_part; i++) {
		data[i] = glm::vec3(0, 0, 0);
	}
	buffer->unmap();
}

TFParticles::TFParticles(Engine& engine) {
	default_cam.setOrthographic();
	default_cam.updateView();

	_particles_pos_buffers[0] = std::make_unique<InstanceBuffer<glm::vec3>>();
	_particles_pos_buffers[1] = std::make_unique<InstanceBuffer<glm::vec3>>();
	_particles_vel_buffers[0] = std::make_unique<InstanceBuffer<glm::vec3>>();
	_particles_vel_buffers[1] = std::make_unique<InstanceBuffer<glm::vec3>>();

	__set_pos(_particles_pos_buffers[0]);
	__set_pos(_particles_pos_buffers[1]);
	__set_vel(_particles_vel_buffers[0]);
	__set_vel(_particles_vel_buffers[1]);

	_tf_vao[0] = std::make_unique<VertexArrayObject>();
	_tf_vao[1] = std::make_unique<VertexArrayObject>();

	{// setup buffer 0
		_tf_vao[0]->bind();
		_particles_pos_buffers[0]->bind();
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_pos_buffers[0]->unbind();

		_particles_vel_buffers[0]->bind();
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_vel_buffers[0]->unbind();

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		_tf_vao[0]->unbind();
	}

	{// setup buffer 1
		_tf_vao[1]->bind();
		_particles_pos_buffers[1]->bind();
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_pos_buffers[1]->unbind();

		_particles_vel_buffers[1]->bind();
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_vel_buffers[1]->unbind();

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		_tf_vao[1]->unbind();
	}

	// rendering

	_render_vao[0] = std::make_unique<VertexArrayObject>();
	_render_vao[1] = std::make_unique<VertexArrayObject>();

	{// setup vao 0
		_render_vao[0]->bind();
		// bc we computed into the other one 0->1
		_particles_pos_buffers[1]->bind();
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_pos_buffers[1]->unbind();

		_particles_vel_buffers[1]->bind();
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_vel_buffers[1]->unbind();

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		_render_vao[0]->unbind();
	}

	{// setup vao 1
		_render_vao[1]->bind();
		// bc we computed into the other one 1->0
		_particles_pos_buffers[0]->bind();
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_pos_buffers[0]->unbind();

		_particles_vel_buffers[0]->bind();
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		_particles_vel_buffers[0]->unbind();

		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);

		_render_vao[1]->unbind();
	}

	setupShaderFiles();

	_tf_shader = ShaderBuilder::start()
		.addStageVertexF(engine, vertexPathTF)
		.addTransformFeedbackVarying("_out_pos")
		.addTransformFeedbackVarying("_out_vel")
		//.addTransformFeedbackVarying("_out_color")
		.addStageFragmentF(engine, fragmentPathTF) // empty stage
		.finish();
	assert(static_cast<bool>(_tf_shader));

	//_points_shader = Shader::createF(engine, vertexPathPoints, fragmentPathPoints);
	_points_shader = ShaderBuilder::start()
		.addStageVertexF(engine, vertexPathPoints)
		.addStageFragmentF(engine, fragmentPathPoints)
		.finish();
	assert(static_cast<bool>(_points_shader));
}

TFParticles::~TFParticles(void) {
}

void TFParticles::computeParticles(Services::OpenGLRenderer&, Engine&) {
	_tf_shader->bind();

	float time_delta = (1/144.f) * 0.2f;
	time += time_delta;
	_tf_shader->setUniform1f("_time_delta", time_delta);
	_tf_shader->setUniform1f("_time", time);
	_tf_shader->setUniform3f("_env_vec", env_vec * env_force);
	_tf_shader->setUniform1f("_noise_force", noise_force);
	_tf_shader->setUniform1f("_dampening", dampening);

	const size_t curr_index = first_particles_buffer ? 0 : 1;
	const size_t next_index = first_particles_buffer ? 1 : 0;

	// bind in particles
	_tf_vao[curr_index]->bind();

	// bind out particles
	// the order is the same as given to the ShaderBuilder
	_particles_pos_buffers[next_index]->bindBase(0);
	_particles_vel_buffers[next_index]->bindBase(1);

	glEnable(GL_RASTERIZER_DISCARD); // compute only rn

	glBeginTransformFeedback(GL_POINTS);
	glDrawArrays(GL_POINTS, 0, _particles_pos_buffers[curr_index]->getSize());
	glEndTransformFeedback();

	glDisable(GL_RASTERIZER_DISCARD);

	// TODO: move this
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 0, 0);
	glBindBufferBase(GL_TRANSFORM_FEEDBACK_BUFFER, 1, 0);

	_tf_vao[curr_index]->unbind();
	_tf_shader->unbind();

	// TODO: do i need this??
	glFlush();

	first_particles_buffer = !first_particles_buffer;
}

void TFParticles::renderParticles(Services::OpenGLRenderer& rs, Engine& engine) {
	// for speed
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);

#ifndef MM_OPENGL_3_GLES
	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_VERTEX_PROGRAM_POINT_SIZE);
#endif

	rs.targets[target_fbo]->bind(FrameBufferObject::W);

	_points_shader->bind();

	auto vp = default_cam.getViewProjection();

	_points_shader->setUniformMat4f("_vp", vp);
	_points_shader->setUniform1f("_point_size", point_size);

	const size_t curr_index = first_particles_buffer; // 0->1 / 1->0 bc compute phase also switches
	const size_t next_index = !first_particles_buffer;
	_render_vao[curr_index]->bind();

	glDrawArrays(GL_POINTS, 0, _particles_pos_buffers[next_index]->getSize());
	//glDrawArrays(GL_LINES, 0, _particles_pos_buffers[next_index]->getSize()/2);

	_render_vao[curr_index]->unbind();
	_points_shader->unbind();
}

void TFParticles::render(Services::OpenGLRenderer& rs, Engine& engine) {
	ZoneScopedN("MM::OpenGL::RenderTasks::TFParticles::render");

	// imgui
	if (ImGui::Begin("particles")) {
		if (ImGui::InputFloat3("env_vec", &env_vec.x)) {
			env_vec = glm::normalize(env_vec);
		}
		ImGui::DragFloat("env_force", &env_force, 0.01f);
		ImGui::DragFloat("noise_force", &noise_force, 0.01f);
		ImGui::InputFloat("dampening", &dampening);
		ImGui::DragFloat("point_size", &point_size, 0.01f);
	}
	ImGui::End();

	computeParticles(rs, engine);

	renderParticles(rs, engine);
}

void TFParticles::setupShaderFiles(void) {
	FS_CONST_MOUNT_FILE(vertexPathTF,
GLSL_VERSION_STRING
R"(
#ifdef GL_ES
	precision mediump float;
#endif

//uniform mat4 _VP;
uniform float _time_delta;
uniform float _time;

uniform vec3 _env_vec; // premultiplied
uniform float _noise_force;
uniform float _dampening;

layout(location = 0) in vec3 _in_pos;
layout(location = 1) in vec3 _in_vel;

out vec3 _out_pos;
out vec3 _out_vel;

// https://www.shadertoy.com/view/4dS3Wd
// By Morgan McGuire @morgan3d, http://graphicscodex.com
// Reuse permitted under the BSD license.

// All noise functions are designed for values on integer scale.
// They are tuned to avoid visible periodicity for both positive and
// negative coordinates within a few orders of magnitude.

// Precision-adjusted variations of https://www.shadertoy.com/view/4djSRW
float hash(float p) { p = fract(p * 0.011); p *= p + 7.5; p *= p + p; return fract(p); }
float hash(vec2 p) {vec3 p3 = fract(vec3(p.xyx) * 0.13); p3 += dot(p3, p3.yzx + 3.333); return fract((p3.x + p3.y) * p3.z); }

float noise(float x) {
	float i = floor(x);
	float f = fract(x);
	float u = f * f * (3.0 - 2.0 * f);
	return mix(hash(i), hash(i + 1.0), u);
}


float noise(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);

	// Four corners in 2D of a tile
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));

	// Simple 2D lerp using smoothstep envelope between the values.
	// return vec3(mix(mix(a, b, smoothstep(0.0, 1.0, f.x)),
	//			mix(c, d, smoothstep(0.0, 1.0, f.x)),
	//			smoothstep(0.0, 1.0, f.y)));

	// Same code, with the clamps in smoothstep and common subexpressions
	// optimized away.
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}


float noise(vec3 x) {
	const vec3 step = vec3(110, 241, 171);

	vec3 i = floor(x);
	vec3 f = fract(x);

	// For performance, compute the base input to a 1D hash from the integer part of the argument and the
	// incremental change to the 1D based on the 3D -> 1D wrapping
	float n = dot(i, step);

	vec3 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
				   mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
			   mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
				   mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

#define NUM_NOISE_OCTAVES 4

float fbm(float x) {
	float v = 0.0;
	float a = 0.5;
	float shift = float(100);
	for (int i = 0; i < NUM_NOISE_OCTAVES; ++i) {
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}


float fbm(vec2 x) {
	float v = 0.0;
	float a = 0.5;
	vec2 shift = vec2(100);
	// Rotate to reduce axial bias
	mat2 rot = mat2(cos(0.5), sin(0.5), -sin(0.5), cos(0.50));
	for (int i = 0; i < NUM_NOISE_OCTAVES; ++i) {
		v += a * noise(x);
		x = rot * x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}


float fbm(vec3 x) {
	float v = 0.0;
	float a = 0.5;
	vec3 shift = vec3(100);
	for (int i = 0; i < NUM_NOISE_OCTAVES; ++i) {
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

void main() {
	// TODO: just use noise as component, instead of angle
	float noise_value1 = noise(vec3(_in_pos.xy, _time));
	float noise_value2 = noise(vec3(_in_pos.xy*7.88822129, _time));

	float noise_dir = noise_value1 * 3.14159 * 2.0;
	float noise_dir2 = noise_value2 * 3.14159 * 2.0;

	vec3 noise_dir_vec = normalize(vec3(
		cos(noise_dir) + -cos(noise_dir2),
		sin(noise_dir)*0.5 + sin(noise_dir2)*0.5,
		0.0
	));

	_out_vel =
		_in_vel +
		(
			noise_dir_vec * _noise_force +
			_env_vec
		) * _time_delta;

	_out_vel *= _dampening;

	_out_pos = _in_pos + _out_vel;

	// wrap hack
	const vec2 extent = vec2(50.0, 50.0 * (9.0/16.0));

	// right
	_out_pos.x = _out_pos.x > extent.x ? _out_pos.x - extent.x*2.0 : _out_pos.x;

	// left
	_out_pos.x = _out_pos.x < -extent.x ? _out_pos.x + extent.x*2.0 : _out_pos.x;

	// top
	_out_pos.y = _out_pos.y > extent.y ? _out_pos.y - extent.y*2.0 : _out_pos.y;

	// bottom
	_out_pos.y = _out_pos.y < -extent.y ? _out_pos.y + extent.y*2.0 : _out_pos.y;
})")

	FS_CONST_MOUNT_FILE(fragmentPathTF,
GLSL_VERSION_STRING
R"(
#ifdef GL_ES
	precision mediump float;
#endif

out vec4 _out_color;

void main() {
	_out_color = vec4(1.0);
})")

	FS_CONST_MOUNT_FILE(vertexPathPoints,
GLSL_VERSION_STRING
R"(
#ifdef GL_ES
	precision mediump float;
#endif

uniform mat4 _vp;
uniform float _point_size;

layout(location = 0) in vec3 _in_pos;
layout(location = 1) in vec3 _in_vel;

out vec3 _frag_color;

void main() {
	gl_Position = _vp * vec4(_in_pos, 1.0);

	gl_Position.z = -1.0; // hack for ortho

	gl_PointSize = _point_size;

	//float speed_f = clamp(length(_in_vel)*14.0, 0.0, 1.0);
	//_frag_color = vec3(0.9, speed_f, 0.2);
	_frag_color = vec3(normalize(_in_vel.xy)/2 + 0.5, 0.5);
})")

	FS_CONST_MOUNT_FILE(fragmentPathPoints,
GLSL_VERSION_STRING
R"(
#ifdef GL_ES
	precision mediump float;
#endif

in vec3 _frag_color;

out vec4 _out_color;

void main() {
	_out_color = vec4(_frag_color, 1.0);
})")

}

} // MM::OpenGL::RenderTasks

