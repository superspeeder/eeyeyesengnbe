
#include <spdlog/spdlog.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <array>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <tuple>
#include <iostream>

struct Vertex {
	glm::vec4 position;
	glm::vec4 color;
	glm::vec2 textureCoords;
	glm::vec4 normals;
};

enum class PrimitiveFormat {
	Triangles = GL_TRIANGLES,
	TriangleFan = GL_TRIANGLE_FAN,
	TriangleStrip = GL_TRIANGLE_STRIP,
	Lines = GL_LINES,
	LineStrip = GL_LINE_STRIP
};

struct Mesh {
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	PrimitiveFormat primitiveFormat;

	uint32_t vao, vbo, ibo;
};

struct Texture {
	uint32_t handle;

	glm::ivec2 size;

};

struct ShaderProgram {
	uint32_t handle;
};

std::string readFile(std::string name) {
	std::ifstream f(name);
	std::string str((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	return str;
}

constexpr std::string s2lower(std::string str) {
	std::transform(str.begin(), str.end(), str.begin(),
		[](unsigned char c) { return std::tolower(c); });
	return str;
}

uint32_t sTypeFromName(std::string name) {
	std::string ln = s2lower(name);
	if (ln == "vertex") return GL_VERTEX_SHADER;
	if (ln == "fragment") return GL_FRAGMENT_SHADER;
	if (ln == "compute") return GL_COMPUTE_SHADER;
	if (ln == "geometry") return GL_GEOMETRY_SHADER;
	if (ln == "tess-control") return GL_TESS_CONTROL_SHADER;
	if (ln == "tess-evaluation") return GL_TESS_EVALUATION_SHADER;
	spdlog::error("Unknown shader type {}", name);
	throw std::runtime_error("Unknown shader type " + name);
}

bool isWhiteSpace(char c) {
	if (c == ' ' || c == '\n' || c == '\r' || c == '\t') return true;
	return false;
}

std::string lStripNewlines(std::string text) {
	size_t i = 0;
	char c = text[0];
	while (isWhiteSpace(c)) {
		c = text[++i];
		if (i >= text.size()) return "";
	}

	return text.substr(i);
}

std::tuple<std::string, std::string> splitAt(std::string text, size_t i) {
	if (i == 0) return std::make_tuple("", text.substr(1));
	if (i == std::string::npos) return std::make_tuple(text, "");
	if (i + 1 == text.size()) return std::make_tuple(text.substr(0, i), "");
	if (i + 1 > text.size()) return std::make_tuple(text, "");
	return std::make_tuple(text.substr(0, i), text.substr(i + 1));
}

std::tuple<std::string, std::string> splitOnFirst(std::string text, char c) {
	size_t i = text.find_first_of(c);
	return splitAt(text, i);
}

std::tuple<std::string, std::string> splitFirstLine(std::string text) {
	return splitOnFirst(text, '\n');
}

std::string stripWhitespace(std::string str) {
	size_t start = 0;
	char c = str[start];
	while (isWhiteSpace(c)) {
		c = str[++start];
		if (start >= str.size()) return "";
	}
	std::string rem = str.substr(start);
	size_t end = rem.size() - 1;
	c = rem[end];
	while (isWhiteSpace(c)) {
		c = rem[--end];
		if (end == 0 || end >= rem.size()) return "";
	}

	return rem.substr(0, end + 1);
}

uint32_t createShader(std::string path) {
	std::string srcuf = lStripNewlines(readFile(path));
	/* expect that the first non-blank line is in format
	#type <name>
	*/


	std::string l, src;
	std::tie(l, src) = splitFirstLine(srcuf);

	if (!l.starts_with("#type ")) { spdlog::error("Failed to read shader file {}. Missing a type header.", path); throw std::runtime_error("Failed to read shader " + path); }

	std::string type = stripWhitespace(l.substr(6));
	uint32_t stype = sTypeFromName(type);

	uint32_t sh = glCreateShader(stype);
	const char* ssrc_czs = src.c_str();
	glShaderSource(sh, 1, &ssrc_czs, nullptr);
	
	glCompileShader(sh);

	int i;
	glGetShaderiv(sh, GL_COMPILE_STATUS, &i);
	if (!i) {
		glGetShaderiv(sh, GL_INFO_LOG_LENGTH, &i);
		char* ilog = new char[i];

		glGetShaderInfoLog(sh, i, &i, ilog);
		spdlog::error("Failed to compile shader {}!", path);
		std::cerr << ilog;

		delete[i] ilog;
		throw std::runtime_error("Failed to compile shader " + path);
	}

	return sh;
}

ShaderProgram* createShaderProgram(std::initializer_list<std::string> files) {
	std::vector<uint32_t> shaders;
	for (std::string p : files) {
		shaders.push_back(createShader(p));
	}
	uint32_t pr = glCreateProgram();
	for (uint32_t s : shaders) {
		glAttachShader(pr, s);
	}

	glLinkProgram(pr);
	int i;
	glGetProgramiv(pr, GL_LINK_STATUS, &i);
	if (!i) {
		glGetProgramiv(pr, GL_INFO_LOG_LENGTH, &i);
		char* ilog = new char[i];

		glGetProgramInfoLog(pr, i, &i, ilog);
		spdlog::error("Failed to link program");
		std::cerr << ilog;

		delete[i] ilog;
		throw std::runtime_error("Failed to link program");
	}
	
	for (uint32_t s : shaders) {
		glDeleteShader(s);
	}

	return new ShaderProgram{pr};
}

struct Material {
	ShaderProgram* shader;
	std::array<Texture*, 32> textures;
	glm::vec4 color;
};

struct MeshRenderer {
	Mesh* mesh;
	Material* material;
};

struct Transform {
	glm::vec3 position{0.0f, 0.0f, 0.0f};
	glm::fquat rotation = glm::quat_identity<float, glm::packed_highp>();
	glm::vec3 scale{1.0f, 1.0f, 1.0f};
};

struct GameObject {
	MeshRenderer meshRenderer;
	Transform transform;
};

GameObject* createGameObject(Mesh* mesh, Material* material) {
	GameObject* go = new GameObject();

	go->meshRenderer.mesh = mesh;
	go->meshRenderer.material = material;
	return go;
}

glm::mat4 rotate_m4(glm::mat4 mat, glm::fquat quat) {
	return glm::toMat4(quat) * mat;
}

glm::mat4 createModelMatrix(const Transform& transform) {
	return glm::translate(rotate_m4(glm::scale(glm::identity<glm::mat4>(), transform.scale), transform.rotation), transform.position);
}

glm::mat4 createModelMatrix(const Transform* transform) {
	if (transform != nullptr) {
		return createModelMatrix(*transform);
	}

	return glm::identity<glm::mat4>();
}

Mesh* createMesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, PrimitiveFormat fmt) {

	Mesh* mesh = new Mesh();
	glCreateVertexArrays(1, &mesh->vao);
	glCreateBuffers(2, &mesh->vbo); // assume that the struct is contiguous

	glBindBuffer(GL_ARRAY_BUFFER, mesh->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->ibo);

	glNamedBufferData(mesh->vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
	glNamedBufferData(mesh->ibo, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

	glVertexArrayAttribBinding(mesh->vao, 0, 0);
	glVertexArrayAttribBinding(mesh->vao, 1, 0);
	glVertexArrayAttribBinding(mesh->vao, 2, 0);
	glVertexArrayAttribBinding(mesh->vao, 3, 0);
	glVertexArrayAttribFormat(mesh->vao, 0, 4, GL_FLOAT, false, 0);
	glVertexArrayAttribFormat(mesh->vao, 1, 4, GL_FLOAT, false, 4 * sizeof(float));
	glVertexArrayAttribFormat(mesh->vao, 2, 2, GL_FLOAT, false, 8 * sizeof(float));
	glVertexArrayAttribFormat(mesh->vao, 3, 4, GL_FLOAT, false, 10 * sizeof(float));
	glVertexArrayVertexBuffer(mesh->vao, 0, mesh->vbo, 0, sizeof(Vertex));
	glVertexArrayElementBuffer(mesh->vao, mesh->ibo);
	glEnableVertexArrayAttrib(mesh->vao, 0);
	glEnableVertexArrayAttrib(mesh->vao, 1);
	glEnableVertexArrayAttrib(mesh->vao, 2);
	glEnableVertexArrayAttrib(mesh->vao, 3);
	

	mesh->vertices = vertices;
	mesh->indices = indices;
	mesh->primitiveFormat = fmt;
	return mesh;
}

glm::mat4 pMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);

struct Camera {
	glm::vec3 position;
	glm::fquat lookDirection = glm::angleAxis(0.f, glm::vec3(0.f,0.f,1.f));
};

glm::mat4 calcVPMatrix(Camera* camera) {
	return pMatrix * glm::translate(glm::toMat4(camera->lookDirection), -camera->position);
}

void applyMaterial(Material* mat) {
	glUseProgram(mat->shader->handle);
	glUniform4fv(glGetUniformLocation(mat->shader->handle, "uColor"), 1, reinterpret_cast<const float*>(&mat->color));
}

void applyTransform(GameObject* go) {
	const float* m = glm::value_ptr(createModelMatrix(go->transform));
	glUniformMatrix4fv(glGetUniformLocation(go->meshRenderer.material->shader->handle, "uModel"), 1, false, m);
}

void applyCamera(Camera* camera, GameObject* go) {
	const float* m = glm::value_ptr(calcVPMatrix(camera));
	glUniformMatrix4fv(glGetUniformLocation(go->meshRenderer.material->shader->handle, "uViewProjection"), 1, false, m);
}

void renderGameObject(Camera* camera, GameObject* go) {
	glBindVertexArray(go->meshRenderer.mesh->vao);
	applyMaterial(go->meshRenderer.material);
	applyTransform(go);
	applyCamera(camera, go);
	
	glDrawElements(static_cast<GLenum>(go->meshRenderer.mesh->primitiveFormat), go->meshRenderer.mesh->indices.size(), GL_UNSIGNED_INT, 0);
}

void updateCamera(GLFWwindow* win, Camera* camera) {
	glm::vec3 cameraForward = glm::rotate(glm::inverse(camera->lookDirection), { 0,0,-1,1 });
	glm::vec3 cameraRight = glm::rotate(glm::inverse(camera->lookDirection), { 1,0,0,1 });
	glm::vec3 cameraUp = glm::rotate(glm::inverse(camera->lookDirection), { 0,1,0,1 });

	if (glfwGetKey(win, GLFW_KEY_W)) camera->position += 0.05f * cameraForward;
	if (glfwGetKey(win, GLFW_KEY_A)) camera->position -= 0.05f * glm::vec3(cameraRight.x, 0, cameraRight.z);
	if (glfwGetKey(win, GLFW_KEY_S)) camera->position -= 0.05f * cameraForward;
	if (glfwGetKey(win, GLFW_KEY_D)) camera->position += 0.05f * cameraRight;
	if (glfwGetKey(win, GLFW_KEY_Q)) camera->position += 0.05f * cameraUp;
	if (glfwGetKey(win, GLFW_KEY_E)) camera->position -= 0.05f * cameraUp;

	if (glfwGetKey(win, GLFW_KEY_LEFT)) camera->lookDirection *= glm::angleAxis(glm::radians(-0.5f), glm::vec3(0.f, 1.f, 0.f));
	if (glfwGetKey(win, GLFW_KEY_RIGHT)) camera->lookDirection *= glm::angleAxis(glm::radians(0.5f), glm::vec3(0.f, 1.f, 0.f));

	if (glfwGetKey(win, GLFW_KEY_UP)) camera->lookDirection *= glm::angleAxis(glm::radians(-0.5f), cameraRight);
	if (glfwGetKey(win, GLFW_KEY_DOWN)) camera->lookDirection *= glm::angleAxis(glm::radians(0.5f), cameraRight);

}

GameObject* clone(GameObject* go) {
	GameObject *go2 = new GameObject(*go);
	return go2;
}

std::vector<GameObject*> createGameObjects(GameObject* prefab, size_t count) {
	std::vector<GameObject*> gos(count);
	for (int i = 0; i < count; i++) {
		gos[i] = clone(prefab);
	}
	return gos;
}

void renderGameObjects(Camera* camera, std::vector<GameObject*> objs) {
	for (auto* go : objs) {
		renderGameObject(camera, go);
	}
}

void APIENTRY glDebugOutput(GLenum source,
	GLenum type,
	unsigned int id,
	GLenum severity,
	GLsizei length,
	const char* message,
	const void* userParam)
{
	// ignore non-significant error/warning codes
	if (id == 131169 || id == 131185 || id == 131218 || id == 131204) return;

	std::cout << "---------------" << std::endl;
	std::cout << "Debug message (" << id << "): " << message << std::endl;

	switch (source)
	{
	case GL_DEBUG_SOURCE_API:             std::cout << "Source: API"; break;
	case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   std::cout << "Source: Window System"; break;
	case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cout << "Source: Shader Compiler"; break;
	case GL_DEBUG_SOURCE_THIRD_PARTY:     std::cout << "Source: Third Party"; break;
	case GL_DEBUG_SOURCE_APPLICATION:     std::cout << "Source: Application"; break;
	case GL_DEBUG_SOURCE_OTHER:           std::cout << "Source: Other"; break;
	} std::cout << std::endl;

	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:               std::cout << "Type: Error"; break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cout << "Type: Deprecated Behaviour"; break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  std::cout << "Type: Undefined Behaviour"; break;
	case GL_DEBUG_TYPE_PORTABILITY:         std::cout << "Type: Portability"; break;
	case GL_DEBUG_TYPE_PERFORMANCE:         std::cout << "Type: Performance"; break;
	case GL_DEBUG_TYPE_MARKER:              std::cout << "Type: Marker"; break;
	case GL_DEBUG_TYPE_PUSH_GROUP:          std::cout << "Type: Push Group"; break;
	case GL_DEBUG_TYPE_POP_GROUP:           std::cout << "Type: Pop Group"; break;
	case GL_DEBUG_TYPE_OTHER:               std::cout << "Type: Other"; break;
	} std::cout << std::endl;

	switch (severity)
	{
	case GL_DEBUG_SEVERITY_HIGH:         std::cout << "Severity: high"; break;
	case GL_DEBUG_SEVERITY_MEDIUM:       std::cout << "Severity: medium"; break;
	case GL_DEBUG_SEVERITY_LOW:          std::cout << "Severity: low"; break;
	case GL_DEBUG_SEVERITY_NOTIFICATION: std::cout << "Severity: notification"; break;
	} std::cout << std::endl;
	std::cout << std::endl;
}

int main() {

	glfwInit();
	glfwDefaultWindowHints();
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
//	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

	GLFWwindow* win = glfwCreateWindow(800, 800, "Window", NULL, NULL);
	glfwMakeContextCurrent(win);

	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);



	int flags; glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
	if (flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glDebugMessageCallback(glDebugOutput, nullptr);
		glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	}

	glClearColor(0, 1.0f, 0, 1.0f);

 	glEnable(GL_DEPTH_TEST);
 	glEnable(GL_BLEND);
 	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
 	glBlendEquation(GL_FUNC_ADD);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);
	glFrontFace(GL_CCW);

	ShaderProgram* sp = createShaderProgram({ "test.glsl", "testvert.glsl" });

	Material* mat = new Material();
	mat->color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
	mat->shader = sp;

	std::vector<Vertex> vertices = {
		{ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
		{ { 1.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
		{ { 1.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
		{ { 0.0f, 1.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},

		{ { 0.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
		{ { 1.0f, 0.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
		{ { 1.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
		{ { 0.0f, 1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 1.0f }},
	};

	std::vector<uint32_t> indices = {
		0, 2, 3,
		0, 1, 2,

		4, 7, 6,
		4, 6, 5,

		1, 5, 6,
		1, 6, 2,

		4, 3, 7,
		4, 0, 3,

		4, 1, 0,
		4, 5, 1,

		3, 6, 7,
		3, 2, 6,
	};

	Mesh* mesh = createMesh(vertices, indices, PrimitiveFormat::Triangles);

	mat->color.a = 0.5f;

	GameObject* prefab = createGameObject(mesh, mat);

	std::vector<GameObject*> testObjs = createGameObjects(prefab, 10);

//	GameObject* testObj = createGameObject(mesh, mat);
//	GameObject* testObj2 = createGameObject(mesh, mat);
//	GameObject* testObj3 = createGameObject(mesh, mat);
//	GameObject* testObj4 = createGameObject(mesh, mat);
//	GameObject* testObj5 = createGameObject(mesh, mat);

//	GameObject* testObj6 = createGameObject(mesh, mat);
//	GameObject* testObj7 = createGameObject(mesh, mat);
//	GameObject* testObj8 = createGameObject(mesh, mat);
//	GameObject* testObj9 = createGameObject(mesh, mat);
//	GameObject* testObj10 = createGameObject(mesh, mat);

	testObjs[1]->transform.position.x = 2;
	testObjs[2]->transform.position.x = -2;
	testObjs[3]->transform.position.y = 2;
	testObjs[4]->transform.position.y = -2;

	testObjs[6]->transform.position.x = 2;
	testObjs[7]->transform.position.x = -2;
	testObjs[8]->transform.position.y = 2;
	testObjs[9]->transform.position.y = -2;

	testObjs[5]->transform.position.z = 2;
	testObjs[6]->transform.position.z = 2;
	testObjs[7]->transform.position.z = 2;
	testObjs[8]->transform.position.z = 2;
	testObjs[9]->transform.position.z = 2;


	Camera* camera = new Camera();

	spdlog::info("Hello!");



	while (!glfwWindowShouldClose(win)) {
		glfwPollEvents();


		updateCamera(win, camera);


		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

		renderGameObjects(camera, testObjs);


		glfwSwapBuffers(win);
	}
}