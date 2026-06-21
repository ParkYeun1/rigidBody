#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// USER INTERFACE GLOBALS
int LeftButtonDown = 0;    // MOUSE STUFF
int RightButtonDown = 0;

// settings
const unsigned int SCR_WIDTH = 1200;
const unsigned int SCR_HEIGHT = 1200;

// camera
Camera camera(glm::vec3(-1.0f, 1.0f, 6.0f));

//Camera camera(glm::vec3(0.0f, 1.5f, 2.5f), glm::vec3(0.0f, 1.0f, 0.0f), -90.f, -15.0f);
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

// light information
glm::vec3 lightColor = glm::vec3(0.48f, 0.68f, 1.0f);
// shader
Shader* lightingShader;
//Shader ourShader("7.4.camera.vs", "7.4.camera.fs");

// ObjectModel
Model* ourObjectModel;
//const char* ourObjectPath = "./teapot.obj";

// HOUSE KEEPING
void initGL(GLFWwindow** window);
void setupShader();
void destroyShader();
void createGLPrimitives();
void destroyGLPrimitives();

// CALLBACKS
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void processInput(GLFWwindow* window, int key, int scancode, int action, int mods);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);

bool hasTextures = false;
bool activateParent = true;   
bool activateSpotLight = false;

// Rigid-body simulation hooks
void InitRigidBodies();
void ResetRigidBodies();
void UpdateRigidBodies(float dt);
void DrawRigidBodies();
void DrawGroundPlane(glm::mat4 model);

void myDisplay()
{
	glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glm::mat4 model = glm::mat4(1.0f);
	DrawGroundPlane(model);

	if (activateParent)
	{
		UpdateRigidBodies(deltaTime);
	}
	DrawRigidBodies();
}

int main()
{
	GLFWwindow* window = NULL;

	initGL(&window);
	setupShader();
	createGLPrimitives();
	InitRigidBodies();

	glEnable(GL_DEPTH_TEST);
	// render loop

	while (!glfwWindowShouldClose(window))
	{
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		// light properties
		lightingShader->use();
		lightingShader->setVec3("light.position", camera.Position);
		lightingShader->setVec3("light.direction", camera.Front);
		lightingShader->setVec3("viewPos", camera.Position);
		lightingShader->setFloat("light.cutOff", glm::cos(glm::radians(30.0f)));
		lightingShader->setFloat("light.outerCutOff", glm::cos(glm::radians(50.0f)));
		if (activateSpotLight == true)
		{
			lightingShader->setFloat("activateSpotlight", true);
		}
		else
		{
			lightingShader->setFloat("activateSpotlight", false);

		}
		lightingShader->setVec3("light.ambient", 0.0f, 0.0f, 0.0f);

		lightingShader->setVec3("light.diffuse", 1.0f, 1.0f, 1.0f);
		lightingShader->setVec3("light.specular", 1.0f, 1.0f, 1.0f);
		lightingShader->setFloat("light.constant", 0.01f);
		lightingShader->setFloat("light.linear", 0.05f);
		lightingShader->setFloat("light.quadratic", 0.0009f);

		// material properties
		lightingShader->setFloat("material.shininess", 16.0f);

		// view/projection transformations
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		lightingShader->setMat4("projection", projection);
		lightingShader->setMat4("view", view);

		// render
		myDisplay();

		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	destroyGLPrimitives();
	destroyShader();

	// glfw: terminate, clearing all previously allocated GLFW resources.
	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void initGL(GLFWwindow** window)
{
	// glfw: initialize and configure
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X
#endif

	// glfw window creation
	* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "rigid body", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		exit(-1);
	}
	glfwMakeContextCurrent(*window);
	glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);
	glfwSetCursorPosCallback(*window, mouse_callback);
	glfwSetMouseButtonCallback(*window, mouse_button_callback);
	glfwSetScrollCallback(*window, scroll_callback);
	glfwSetKeyCallback(*window, processInput);

	// glad: load all OpenGL function pointers
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		exit(-1);
	}
}

void setupShader()
{
	// Light attributes
	lightingShader = new Shader("light_casters.vs", "light_casters.fs");
	lightingShader->use();
	lightingShader->setVec3("lightColor", lightColor);
}

void destroyShader()
{
	delete lightingShader;
}

void processInput(GLFWwindow* window, int key, int scancode, int action, int mods)
{

	if (key == GLFW_KEY_SPACE && action == GLFW_PRESS) {
		if (activateParent == false) activateParent = true;
		else activateParent = false;
	}
	if (key == GLFW_KEY_R && action == GLFW_PRESS) {
		ResetRigidBodies();
		activateParent = true;
	}
	float cameraSpeed = 2.5f * deltaTime;
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = (float)xpos;
		lastY = (float)ypos;
		firstMouse = false;
	}

	float xoffset = (float)(xpos - lastX) / SCR_WIDTH;
	float yoffset = (float)(lastY - ypos) / SCR_HEIGHT; 

	lastX = (float)xpos;
	lastY = (float)ypos;
	if (RightButtonDown)
	{
		camera.ProcessMouseMovement(xoffset * 200, yoffset * 200);
	}
	if (LeftButtonDown)
	{

	}
	if (LeftButtonDown && activateParent == true)
	{

	}

}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		LeftButtonDown = 1;
	}
	else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
	{
		LeftButtonDown = 0;
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		RightButtonDown = 1;
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
	{
		RightButtonDown = 0;
	}
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
}

class State {
public:
	State() {

	}
	~State() {

	}
protected:
	glm::vec3 mass = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 x = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 v = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 r = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 p = glm::vec3(0.0f, 0.0f, 0.0f);
	glm::vec3 w = glm::vec3(0.0f, 0.0f, 0.0f);
};

class Primitive {
public:
	Primitive() {
		glGenVertexArrays(1, &VAO);
		glGenBuffers(1, &vbo);
		glGenBuffers(1, &ebo);
	}
	~Primitive() {
		if (ebo) glDeleteBuffers(1, &ebo);
		if (vbo) glDeleteBuffers(1, &vbo);
		if (VAO) glDeleteVertexArrays(1, &VAO);
	}
	void Draw() {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLE_STRIP, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}

protected:
	unsigned int VAO = 0, vbo = 0, ebo = 0;
	unsigned int IndexCount = 0;
	float height = 1.0f;
	float radius[2] = { 1.0f, 1.0f };
};

class Cylinder : public Primitive {
public:
	Cylinder(float bottomRadius = 0.5f, float topRadius = 0.5f, int NumSegs = 16);
};

class Box : public Primitive {
public:
	Box();
	void Draw() {
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
};

class Sphere : public Primitive {
public:
	Sphere(int NumSegs = 16);
};

class Plane : public Primitive {
public:
	Plane();
	void Draw() {
		glBindVertexArray(VAO);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, floorTexture);
		glDrawElements(GL_TRIANGLE_STRIP, IndexCount, GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
private:
	unsigned int floorTexture;
};

Sphere* unitSphere;
Plane* groundPlane;
Cylinder* unitCylinder;
Cylinder* unitCone;
Box* unitBox;

void createGLPrimitives()
{
	unitSphere = new Sphere();
	groundPlane = new Plane();
	unitCylinder = new Cylinder();
	unitCone = new Cylinder(0.5, 0);
	unitBox = new Box();

}
void destroyGLPrimitives()
{
	delete unitSphere;
	delete groundPlane;
	delete unitCylinder;
	delete unitCone;
	delete unitBox;

	delete ourObjectModel;
}

void DrawGroundPlane(glm::mat4 model)
{
	lightingShader->use();
	lightingShader->setMat4("model", model);
	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	lightingShader->setInt("hasTextures", true);
	groundPlane->Draw();

}

void DrawJoint(glm::mat4 model)
{
	glm::mat4 Mat1 = glm::scale(glm::mat4(1.0f), glm::vec3(0.15f, 0.15f, 0.12f));
	Mat1 = glm::rotate(Mat1, glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f));

	lightingShader->use();
	Mat1 = model * Mat1;
	lightingShader->setMat4("model", Mat1);

	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	lightingShader->setInt("hasTextures", false);
	unitCylinder->Draw();
}
void DrawBase(glm::mat4 model)
{
	glm::mat4 Base = glm::scale(glm::mat4(1.0f), glm::vec3(0.2f, 0.025f, 0.2f));
	glm::mat4 InBase = glm::inverse(Base);

	lightingShader->use();
	Base = model * Base;
	lightingShader->setMat4("model", Base);
	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	lightingShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.2f, 0.0f));
	Mat1 = glm::scale(Mat1, glm::vec3(0.1f, 0.4f, 0.1f));

	Mat1 = Base * Mat1;
	lightingShader->setMat4("model", Mat1);
	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	unitCylinder->Draw();

	glm::mat4 Mat2 = glm::translate(InBase, glm::vec3(0.0f, 0.4f, 0.0f));
	Mat2 = Base * Mat2;
	lightingShader->setMat4("model", Mat2);
	DrawJoint(Mat2);
}
void DrawArmSegment(glm::mat4 model)
{
	glm::mat4 Base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.2f, 0.0f));
	Base = glm::scale(Base, glm::vec3(0.1f, 0.5f, 0.1f));
	glm::mat4 InBase = glm::inverse(Base);

	lightingShader->use();
	Base = model * Base;
	lightingShader->setMat4("model", Base);
	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	lightingShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.5f, 0.0f));;
	Mat1 = Base * Mat1;
	lightingShader->setMat4("model", Mat1);
	DrawJoint(Mat1);
}
void DrawWrist(glm::mat4 model)
{
	glm::mat4 Base = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.1f, 0.0f));
	Base = glm::scale(Base, glm::vec3(0.08f, 0.2f, 0.08f));
	glm::mat4 InBase = glm::inverse(Base);

	lightingShader->use();
	Base = model * Base;
	lightingShader->setMat4("model", Base);
	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	lightingShader->setInt("hasTextures", false);
	unitCylinder->Draw();

	glm::mat4 Mat1 = glm::translate(InBase, glm::vec3(0.0f, 0.2f, 0.0f));
	Mat1 = glm::scale(Mat1, glm::vec3(0.06f, 0.06f, 0.06f));

	Mat1 = Base * Mat1;
	lightingShader->setMat4("model", Mat1);
	lightingShader->setVec3("ObjColor", glm::vec3(0.8f, 0.8f, 0.8f));
	unitSphere->Draw();
}


struct RigidBody
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 velocity = glm::vec3(0.0f);
	glm::quat orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
	glm::vec3 angularVelocity = glm::vec3(0.0f);
	glm::vec3 halfSize = glm::vec3(0.5f);
	glm::vec3 color = glm::vec3(0.8f);
	glm::vec3 invInertiaLocal = glm::vec3(1.0f);
	float mass = 1.0f;
	float invMass = 1.0f;
	float restitution = 0.35f;
	float friction = 0.65f;
};

const int NUM_RIGID_BODIES = 3;
RigidBody bodies[NUM_RIGID_BODIES];
const glm::vec3 GRAVITY(0.0f, -9.8f, 0.0f);

void SetBoxMassAndInertia(RigidBody& b, float mass)
{
	b.mass = mass;
	b.invMass = 1.0f / mass;

	glm::vec3 size = 2.0f * b.halfSize;
	float ix = (mass / 12.0f) * (size.y * size.y + size.z * size.z);
	float iy = (mass / 12.0f) * (size.x * size.x + size.z * size.z);
	float iz = (mass / 12.0f) * (size.x * size.x + size.y * size.y);
	b.invInertiaLocal = glm::vec3(1.0f / ix, 1.0f / iy, 1.0f / iz);
}

void ResetRigidBodies()
{
	bodies[0].position = glm::vec3(-0.85f, 2.60f, 0.00f);
	bodies[0].velocity = glm::vec3(1.20f, 0.0f, 0.20f);
	bodies[0].orientation = glm::angleAxis(glm::radians(18.0f), glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f)));
	bodies[0].angularVelocity = glm::vec3(1.6f, 0.5f, -0.9f);
	bodies[0].halfSize = glm::vec3(0.55f, 0.18f, 0.25f);
	bodies[0].color = glm::vec3(0.95f, 0.35f, 0.25f);
	bodies[0].restitution = 0.40f;
	bodies[0].friction = 0.65f;
	SetBoxMassAndInertia(bodies[0], 1.2f);

	bodies[1].position = glm::vec3(0.15f, 4.10f, 0.05f);
	bodies[1].velocity = glm::vec3(-0.35f, 0.0f, 0.10f);
	bodies[1].orientation = glm::angleAxis(glm::radians(-25.0f), glm::normalize(glm::vec3(1.0f, 0.3f, 0.5f)));
	bodies[1].angularVelocity = glm::vec3(-1.0f, 0.7f, 1.3f);
	bodies[1].halfSize = glm::vec3(0.22f, 0.62f, 0.22f);
	bodies[1].color = glm::vec3(0.25f, 0.65f, 0.95f);
	bodies[1].restitution = 0.30f;
	bodies[1].friction = 0.70f;
	SetBoxMassAndInertia(bodies[1], 1.0f);

	bodies[2].position = glm::vec3(0.95f, 5.40f, -0.25f);
	bodies[2].velocity = glm::vec3(-0.85f, 0.0f, 0.30f);
	bodies[2].orientation = glm::angleAxis(glm::radians(35.0f), glm::normalize(glm::vec3(0.2f, 0.5f, 1.0f)));
	bodies[2].angularVelocity = glm::vec3(0.6f, -1.2f, 0.8f);
	bodies[2].halfSize = glm::vec3(0.36f, 0.25f, 0.70f);
	bodies[2].color = glm::vec3(0.55f, 0.90f, 0.35f);
	bodies[2].restitution = 0.35f;
	bodies[2].friction = 0.60f;
	SetBoxMassAndInertia(bodies[2], 1.4f);
}

void InitRigidBodies()
{
	ResetRigidBodies();
}

glm::mat3 InverseInertiaWorld(const RigidBody& b)
{
	glm::mat3 R = glm::mat3_cast(b.orientation);
	glm::mat3 IinvLocal(0.0f);
	IinvLocal[0][0] = b.invInertiaLocal.x;
	IinvLocal[1][1] = b.invInertiaLocal.y;
	IinvLocal[2][2] = b.invInertiaLocal.z;
	return R * IinvLocal * glm::transpose(R);
}

std::vector<glm::vec3> GetBoxCorners(const RigidBody& b)
{
	std::vector<glm::vec3> corners;
	corners.reserve(8);
	glm::mat3 R = glm::mat3_cast(b.orientation);
	for (int x = -1; x <= 1; x += 2)
	{
		for (int y = -1; y <= 1; y += 2)
		{
			for (int z = -1; z <= 1; z += 2)
			{
				glm::vec3 local = glm::vec3((float)x * b.halfSize.x, (float)y * b.halfSize.y, (float)z * b.halfSize.z);
				corners.push_back(b.position + R * local);
			}
		}
	}
	return corners;
}

void IntegrateRigidBody(RigidBody& b, float h)
{
	b.velocity += GRAVITY * h;
	b.position += b.velocity * h;

	glm::quat spin(0.0f, b.angularVelocity.x, b.angularVelocity.y, b.angularVelocity.z);
	glm::quat qdot = spin * b.orientation;
	b.orientation.w += 0.5f * h * qdot.w;
	b.orientation.x += 0.5f * h * qdot.x;
	b.orientation.y += 0.5f * h * qdot.y;
	b.orientation.z += 0.5f * h * qdot.z;
	b.orientation = glm::normalize(b.orientation);

	// Mild damping keeps the demo stable without hiding collision behavior.
	b.velocity *= 0.999f;
	b.angularVelocity *= 0.995f;
}

void ResolveGroundCollision(RigidBody& b)
{
	std::vector<glm::vec3> corners = GetBoxCorners(b);
	float minY = corners[0].y;
	for (const glm::vec3& c : corners)
	{
		minY = std::min(minY, c.y);
	}

	if (minY < 0.0f)
	{
		// Positional correction
		b.position.y += -minY + 0.001f;
	}

	glm::mat3 IinvWorld = InverseInertiaWorld(b);
	glm::vec3 n(0.0f, 1.0f, 0.0f);

	for (const glm::vec3& c : corners)
	{
		if (c.y > 0.02f) continue;

		glm::vec3 r = c - b.position;
		glm::vec3 pointVelocity = b.velocity + glm::cross(b.angularVelocity, r);
		float vn = glm::dot(pointVelocity, n);

		if (vn < 0.0f)
		{
			glm::vec3 rn = glm::cross(r, n);
			float denom = b.invMass + glm::dot(n, glm::cross(IinvWorld * rn, r));
			if (denom > 0.0001f)
			{
				float j = -(1.0f + b.restitution) * vn / denom;
				j /= 4.0f; // several corners may contact at once
				glm::vec3 impulse = j * n;
				b.velocity += impulse * b.invMass;
				b.angularVelocity += IinvWorld * glm::cross(r, impulse);

				// friction impulse.
				glm::vec3 tangent = pointVelocity - vn * n;
				float tangentLen = glm::length(tangent);
				if (tangentLen > 0.0001f)
				{
					tangent /= tangentLen;
					glm::vec3 rt = glm::cross(r, tangent);
					float denomT = b.invMass + glm::dot(tangent, glm::cross(IinvWorld * rt, r));
					float jt = -glm::dot(pointVelocity, tangent) / std::max(denomT, 0.0001f);
					float maxFriction = b.friction * j;
					jt = glm::clamp(jt, -maxFriction, maxFriction);
					glm::vec3 frictionImpulse = jt * tangent;
					b.velocity += frictionImpulse * b.invMass;
					b.angularVelocity += IinvWorld * glm::cross(r, frictionImpulse);
				}
			}
		}
	}
}

void GetAABB(const RigidBody& b, glm::vec3& minP, glm::vec3& maxP)
{
	std::vector<glm::vec3> corners = GetBoxCorners(b);
	minP = corners[0];
	maxP = corners[0];
	for (const glm::vec3& c : corners)
	{
		minP = glm::min(minP, c);
		maxP = glm::max(maxP, c);
	}
}

void ResolvePairCollision(RigidBody& a, RigidBody& b)
{
	glm::vec3 minA, maxA, minB, maxB;
	GetAABB(a, minA, maxA);
	GetAABB(b, minB, maxB);

	float overlapX = std::min(maxA.x, maxB.x) - std::max(minA.x, minB.x);
	float overlapY = std::min(maxA.y, maxB.y) - std::max(minA.y, minB.y);
	float overlapZ = std::min(maxA.z, maxB.z) - std::max(minA.z, minB.z);
	if (overlapX <= 0.0f || overlapY <= 0.0f || overlapZ <= 0.0f) return;

	glm::vec3 n(0.0f);
	float penetration = overlapX;
	n.x = (a.position.x < b.position.x) ? -1.0f : 1.0f;
	if (overlapY < penetration)
	{
		penetration = overlapY;
		n = glm::vec3(0.0f, (a.position.y < b.position.y) ? -1.0f : 1.0f, 0.0f);
	}
	if (overlapZ < penetration)
	{
		penetration = overlapZ;
		n = glm::vec3(0.0f, 0.0f, (a.position.z < b.position.z) ? -1.0f : 1.0f);
	}

	float totalInvMass = a.invMass + b.invMass;
	if (totalInvMass <= 0.0f) return;

	// Positional correction.
	glm::vec3 correction = (penetration / totalInvMass) * n * 0.55f;
	a.position += correction * a.invMass;
	b.position -= correction * b.invMass;

	glm::vec3 relativeVelocity = a.velocity - b.velocity;
	float relN = glm::dot(relativeVelocity, n);
	if (relN < 0.0f)
	{
		float e = std::min(a.restitution, b.restitution);
		float j = -(1.0f + e) * relN / totalInvMass;
		glm::vec3 impulse = j * n;
		a.velocity += impulse * a.invMass;
		b.velocity -= impulse * b.invMass;

		a.angularVelocity += 0.08f * glm::cross(n, relativeVelocity);
		b.angularVelocity -= 0.08f * glm::cross(n, relativeVelocity);
	}
}

void UpdateRigidBodies(float dt)
{
	if (dt <= 0.0f) return;
	dt = std::min(dt, 1.0f / 30.0f);

	const int subSteps = 6;
	float h = dt / (float)subSteps;
	for (int s = 0; s < subSteps; ++s)
	{
		for (int i = 0; i < NUM_RIGID_BODIES; ++i)
		{
			IntegrateRigidBody(bodies[i], h);
			ResolveGroundCollision(bodies[i]);
		}
		for (int i = 0; i < NUM_RIGID_BODIES; ++i)
		{
			for (int j = i + 1; j < NUM_RIGID_BODIES; ++j)
			{
				ResolvePairCollision(bodies[i], bodies[j]);
			}
		}
	}
}

void DrawRigidBox(const RigidBody& b)
{
	glm::mat4 model = glm::translate(glm::mat4(1.0f), b.position);
	model *= glm::toMat4(b.orientation);
	model = glm::scale(model, 2.0f * b.halfSize);

	lightingShader->use();
	lightingShader->setMat4("model", model);
	lightingShader->setVec3("ObjColor", b.color);
	lightingShader->setInt("hasTextures", false);
	unitBox->Draw();
}

void DrawRigidBodies()
{
	for (int i = 0; i < NUM_RIGID_BODIES; ++i)
	{
		DrawRigidBox(bodies[i]);
	}
}

/////////////////////////////////////////////////////////////////////////
///// References https://learnopengl.com/Getting-started/Shaders
/////		     https://learnopengl.com/Lighting/Basic-Lighting
/////			 http://www.songho.ca/opengl/gl_cylinder.html
/////////////////////////////////////////////////////////////////////////


Box::Box()
{
	float data[] = {
		// positions          // normals
		// front (+Z)
		-0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		 0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		 0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		-0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
		// back (-Z)
		 0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		-0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		 0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
		 // left (-X)
		 -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
		 -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
		 // right (+X)
		  0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
		  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
		  // top (+Y)
		  -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		   0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
		   0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		  -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
		  // bottom (-Y)
		  -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		   0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
		   0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
		  -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f
	};

	unsigned int indices[] = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19,
		20, 21, 22, 20, 22, 23
	};

	IndexCount = sizeof(indices) / sizeof(unsigned int);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
	glBindVertexArray(0);
}

Sphere::Sphere(int NumSegs)
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	const unsigned int X_SEGMENTS = NumSegs;
	const unsigned int Y_SEGMENTS = NumSegs;
	const float PI = (float)3.14159265359;

	for (unsigned int y = 0; y <= Y_SEGMENTS; ++y)
	{
		for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
		{
			float xSegment = (float)x / (float)X_SEGMENTS;
			float ySegment = (float)y / (float)Y_SEGMENTS;
			float xPos = std::cos(xSegment * 2.0f * PI) * std::sin(ySegment * PI);
			float yPos = std::cos(ySegment * PI);
			float zPos = std::sin(xSegment * 2.0f * PI) * std::sin(ySegment * PI);

			positions.push_back(glm::vec3(xPos, yPos, zPos));
			normals.push_back(glm::vec3(xPos, yPos, zPos));
		}
	}

	bool oddRow = false;
	for (unsigned int y = 0; y < Y_SEGMENTS; ++y)
	{
		if (!oddRow)
		{
			for (unsigned int x = 0; x <= X_SEGMENTS; ++x)
			{
				indices.push_back(y * (X_SEGMENTS + 1) + x);
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
			}
		}
		else
		{
			for (int x = X_SEGMENTS; x >= 0; --x)
			{
				indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
				indices.push_back(y * (X_SEGMENTS + 1) + x);
			}
		}
		oddRow = !oddRow;
	}

	IndexCount = (unsigned int)indices.size();

	std::vector<float> data;
	for (int i = 0; i < positions.size(); ++i)
	{
		data.push_back(positions[i].x);
		data.push_back(positions[i].y);
		data.push_back(positions[i].z);
		if (normals.size() > 0)
		{
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
		}
	}

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

}


// utility function for loading a 2D texture from file
unsigned int loadTexture(char const* path)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum format;
		if (nrComponents == 1)
			format = GL_RED;
		else if (nrComponents == 3)
			format = GL_RGB;
		else if (nrComponents == 4)
			format = GL_RGBA;

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); 
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		std::cout << "Texture failed to load at path: " << path << std::endl;
		stbi_image_free(data);
	}

	return textureID;
}


Plane::Plane()
{
	float data[] = {
		// positions           // normals          // texcoords
		-10.0f, 0.0f, -10.0f,  0.0f, 1.0f,  0.0f,  0.0f,  0.0f,
		 10.0f, 0.0f, -10.0f,  0.0f, 1.0f,  0.0f,  10.0f, 0.0f,
		 10.0f, 0.0f,  10.0f,  0.0f, 1.0f,  0.0f,  10.0f, 10.0f,
		-10.0f, 0.0f,  10.0f,  0.0f, 1.0f,  0.0f,  0.0f,  10.0f
	};
	unsigned int indices[] = { 0, 1, 3, 2 };

	IndexCount = sizeof(indices) / sizeof(unsigned int);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	GLsizei stride = (3 + 3 + 2) * sizeof(float);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

	glEnableVertexAttribArray(2);
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(6 * sizeof(float)));

	glBindVertexArray(0);

	floorTexture = loadTexture("./wood.png");
	lightingShader->use();
	lightingShader->setInt("texture1", floorTexture);

}

Cylinder::Cylinder(float bottomRadius, float topRadius, int NumSegs)
{
	radius[0] = bottomRadius; radius[1] = topRadius;

	std::vector<glm::vec3> base;
	std::vector<glm::vec3> positions;
	std::vector<glm::vec3> normals;
	std::vector<unsigned int> indices;

	//a circle
	const float PI = (float)3.14159265359;
	float sectorStep = 2 * PI / NumSegs;				// Angle increasing
	float sectorAngle;									// radian

	for (int i = 0; i <= NumSegs; ++i)
	{
		sectorAngle = i * sectorStep;
		float xPos = std::sin(sectorAngle);
		float yPos = 0;
		float zPos = std::cos(sectorAngle);

		base.push_back(glm::vec3(xPos, yPos, zPos));
	}

	//put side of cylinder
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;			// height from -h/2 to h/2   

		for (int j = 0; j <= NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(base[j].x, h, base[j].z));
		}
	}

	int baseCenterIndex = (int)positions.size();
	int topCenterIndex = baseCenterIndex + NumSegs + 1; // include center vertex

	//put base and top circles
	for (int i = 0; i < 2; ++i)
	{
		float h = -height / 2.0f + i * height;
		float ny = (float)-1 + i * 2;

		// center point
		positions.push_back(glm::vec3(0, h, 0));		// height from -h/2 to h/2
		normals.push_back(glm::vec3(0, ny, 0));			// z value of normal; -1 to 1

		for (int j = 0; j < NumSegs; ++j)
		{
			positions.push_back(glm::vec3(base[j].x * radius[i], h, base[j].z * radius[i]));
			normals.push_back(glm::vec3(0, ny, 0));
		}
	}

	//Indexing
	int k1 = 0;											// 1st vertex index at base
	int k2 = NumSegs + 1;								// 1st vertex index at top

	// indices for the side surface
	for (int i = 0; i < NumSegs; ++i, ++k1, ++k2)
	{
		// 2 triangles per sector
		// k1 => k1+1 => k2
		indices.push_back(k1);
		indices.push_back(k1 + 1);
		indices.push_back(k2);

		// k2 => k1+1 => k2+1
		indices.push_back(k2);
		indices.push_back(k1 + 1);
		indices.push_back(k2 + 1);
	}

	//indices for the base surface
	for (int i = 0, k = baseCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(k + 1);
			indices.push_back(k);
		}
		else // last triangle
		{
			indices.push_back(baseCenterIndex);
			indices.push_back(baseCenterIndex + 1);
			indices.push_back(k);
		}
	}

	// indices for the top surface
	for (int i = 0, k = topCenterIndex + 1; i < NumSegs; ++i, ++k)
	{
		if (i < NumSegs - 1)
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(k + 1);
		}
		else // last triangle
		{
			indices.push_back(topCenterIndex);
			indices.push_back(k);
			indices.push_back(topCenterIndex + 1);
		}
	}
	IndexCount = (unsigned int)indices.size();

	std::vector<float> data;
	for (int i = 0; i < positions.size(); ++i)
	{
		data.push_back(positions[i].x);
		data.push_back(positions[i].y);
		data.push_back(positions[i].z);

		if (normals.size() > 0)
		{
			data.push_back(normals[i].x);
			data.push_back(normals[i].y);
			data.push_back(normals[i].z);
		}
	}
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), &data[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
	GLsizei stride = (3 + 3) * sizeof(float);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));
}

