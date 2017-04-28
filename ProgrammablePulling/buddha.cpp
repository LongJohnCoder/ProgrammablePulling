/*
 * dragon.cpp
 *
 *  Created on: Sep 24, 2011
 *      Author: aqnuep
 */

#include <iostream>
#include <fstream>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "wavefront.h"
#include "buddha.h"

namespace buddha {
    
struct Transform {
	glm::mat4 ModelViewMatrix;		// modelview matrix of the transformation
	glm::mat4 ProjectionMatrix;		// projection matrix of the transformation
	glm::mat4 MVPMatrix;			// modelview-projection matrix
};

struct Camera {
	glm::vec3 position;				// camera position
	glm::vec3 rotation;				// camera rotation
};

struct Vertex {
	glm::vec3 position;				// vertex position
	glm::vec3 normal;				// vertex normal
};

struct DrawCommand {
	bool useIndices;				// specifies whether this is an indexed draw command
	GLenum prim_type;				// primitive type
	union {
		struct {
			GLuint indexOffset;	// offset into the index buffer
			GLuint indexCount;	// number of indices
		};
		struct {
			GLuint firstVertex;	// first vertex index
			GLuint vertexCount;	// number of vertices
		};
	};
};

class BuddhaDemo : public IBuddhaDemo {
protected:

    Camera camera;                          // camera data

    Transform transform;                    // transformation data
    GLuint transformUB;                     // uniform buffer for the transformation

    GLuint fragmentProg;                    // common fragment shader program
    GLuint vertexProg[NUMBER_OF_MODES];     // vertex shader programs for the three vertex pulling modes
    GLuint progPipeline[NUMBER_OF_MODES];   // program pipelines for the three vertex pulling modes

    GLuint indexBuffer;                     // index buffer for the mesh
    GLuint vertexBuffer;                    // vertex buffer for the mesh

    GLuint vertexArray;                     // vertex array for the three vertex pulling modes

    GLuint indexTexBuffer;                  // index buffer texture
    GLuint vertexTexBuffer;                 // vertex buffer texture

    GLuint timeElapsedQuery;                // query object for the time taken to render the scene

    DrawCommand drawCmd[NUMBER_OF_MODES];   // draw command for the three vertex pulling modes

    float cameraRotationFactor;             // camera rotation factor between [0,2*PI)

    void loadModels();
    void loadShaders();

    GLuint loadShaderProgramFromFile(const char* filename, GLenum shaderType);
    GLuint createProgramPipeline(GLuint vertexShader, GLuint geometryShader, GLuint fragmentShader);

public:
    BuddhaDemo();

    void renderScene(float dtsec, VertexPullingMode mode, uint64_t* elapsedNanoseconds);
};

std::shared_ptr<IBuddhaDemo> IBuddhaDemo::Create()
{
    return std::make_shared<BuddhaDemo>();
}

BuddhaDemo::BuddhaDemo() {

	std::cout << "> Initializing scene data..." << std::endl;

	// initialize camera data
	cameraRotationFactor = 0.f;
	transform.ProjectionMatrix = glm::perspective(glm::radians(45.0f), (float)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1f, 40.f);

	// create uniform buffer and store camera data
	glGenBuffers(1, &transformUB);
	glBindBuffer(GL_UNIFORM_BUFFER, transformUB);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(transform), &transform, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenQueries(1, &timeElapsedQuery);

	loadShaders();
	loadModels();

    std::cout << "> Done!" << std::endl;
}

void BuddhaDemo::loadModels() {

    std::cout << "> Loading models..." << std::endl;

	demo::WaveFrontObj buddhaObj("models/buddha.obj");

    std::cout << "> Uploading mesh data to GPU..." << std::endl;

	// create empty index buffer
	GLsizei indexBufferSize = (GLsizei)(buddhaObj.Indices.size() * sizeof(GLuint));
	glGenBuffers(1, &indexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, indexBuffer);
	glBufferData(GL_ARRAY_BUFFER, indexBufferSize, NULL, GL_STATIC_DRAW);

	// map index buffer and fill with data
    GLuint *index = (GLuint*)glMapBufferRange(GL_ARRAY_BUFFER, 0, indexBufferSize,
						GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

    for (size_t i = 0; i < buddhaObj.Indices.size(); i++)
		index[i] = buddhaObj.Indices[i];

	glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

	// create empty vertex buffer
    GLsizei vertexBufferSize = (GLsizei)(buddhaObj.Positions.size() * sizeof(Vertex));
	glGenBuffers(1, &vertexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glBufferData(GL_ARRAY_BUFFER, vertexBufferSize, NULL, GL_STATIC_DRAW);

	// map vertex buffer and fill with data
	Vertex *vertex = (Vertex*)glMapBufferRange(GL_ARRAY_BUFFER, 0, vertexBufferSize,
						GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

    for (size_t i = 0; i < buddhaObj.Positions.size(); i++) {
		vertex[i].position[0] = buddhaObj.Positions[i].x;
		vertex[i].position[1] = buddhaObj.Positions[i].y;
		vertex[i].position[2] = buddhaObj.Positions[i].z;
		vertex[i].normal[0] = buddhaObj.Normals[i].x;
		vertex[i].normal[1] = buddhaObj.Normals[i].y;
		vertex[i].normal[2] = buddhaObj.Normals[i].z;
	}

	glUnmapBuffer(GL_ARRAY_BUFFER);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

	// setup vertex array
	glGenVertexArrays(1, &vertexArray);
	glBindVertexArray(vertexArray);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glBindVertexArray(0);

	// setup draw command for fixed-function vertex pulling
	drawCmd[FIXED_FUNCTION_MODE].useIndices = true;
	drawCmd[FIXED_FUNCTION_MODE].prim_type = GL_TRIANGLES;
	drawCmd[FIXED_FUNCTION_MODE].indexOffset = 0;
	drawCmd[FIXED_FUNCTION_MODE].indexCount = (GLuint)buddhaObj.Indices.size();

	// setup draw command for programmable attribute fetching
	drawCmd[FETCHER_MODE].useIndices = true;
	drawCmd[FETCHER_MODE].prim_type = GL_TRIANGLES;
	drawCmd[FETCHER_MODE].indexOffset = 0;
	drawCmd[FETCHER_MODE].indexCount = (GLuint)buddhaObj.Indices.size();

    // setup draw command for programmable attribute fetching with an image
    drawCmd[FETCHER_IMAGE_AOS_MODE].useIndices = true;
    drawCmd[FETCHER_IMAGE_AOS_MODE].prim_type = GL_TRIANGLES;
    drawCmd[FETCHER_IMAGE_AOS_MODE].indexOffset = 0;
    drawCmd[FETCHER_IMAGE_AOS_MODE].indexCount = (GLuint)buddhaObj.Indices.size();

	// setup draw command for fully programmable vertex pulling
	drawCmd[PULLER_MODE].useIndices = false;
	drawCmd[PULLER_MODE].prim_type = GL_TRIANGLES;
	drawCmd[PULLER_MODE].firstVertex = 0;
	drawCmd[PULLER_MODE].vertexCount = (GLuint)buddhaObj.Indices.size();

	// create auxiliary texture buffers
	glGenTextures(1, &indexTexBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, indexTexBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_R32I, indexBuffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);

	glGenTextures(1, &vertexTexBuffer);
	glBindTexture(GL_TEXTURE_BUFFER, vertexTexBuffer);
	glTexBuffer(GL_TEXTURE_BUFFER, GL_R32F, vertexBuffer);
    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

GLuint BuddhaDemo::loadShaderProgramFromFile(const char* filename, GLenum shaderType) {

	std::ifstream file(filename);
	if (!file) {
		std::cerr << "Unable to open file: " << filename << std::endl;
		return 0;
	}

    // weird C++ magic to read the file in one go
    std::string source(
        std::istreambuf_iterator<char>{file},
        std::istreambuf_iterator<char>{});

	if (file.bad()) {
        std::cerr << "Error reading the file: " << filename << std::endl;
		return 0;
	}

    const GLchar* csource = source.c_str();
	GLuint program = glCreateShaderProgramv(shaderType, 1, (const GLchar**)&csource);

	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status != GL_TRUE) {
        std::cerr << "Failed to compile/link shader program:" << std::endl;
		GLchar log[10000];
		glGetProgramInfoLog(program, 10000, NULL, log);
        std::cerr << log << std::endl;
		exit(1);
	}

	return program;

}

GLuint BuddhaDemo::createProgramPipeline(GLuint vertexShader, GLuint geometryShader, GLuint fragmentShader) {

	GLuint pipeline;
	glGenProgramPipelines(1, &pipeline);

	if (vertexShader != 0) glUseProgramStages(pipeline, GL_VERTEX_SHADER_BIT, vertexShader);
	if (geometryShader != 0) glUseProgramStages(pipeline, GL_GEOMETRY_SHADER_BIT, geometryShader);
	if (fragmentShader != 0) glUseProgramStages(pipeline, GL_FRAGMENT_SHADER_BIT, fragmentShader);

	glValidateProgramPipeline(pipeline);

	GLint status;
	glGetProgramPipelineiv(pipeline, GL_VALIDATE_STATUS, &status);
	if (status != GL_TRUE) {
        std::cerr << "Failed to validate program pipeline:" << std::endl;
		GLchar log[10000];
		glGetProgramPipelineInfoLog(pipeline, 10000, NULL, log);
		std::cerr << log << std::endl;
		exit(1);
	}

	return pipeline;

}

void BuddhaDemo::loadShaders() {

    std::cout << "> Loading shaders..." << std::endl;

	// load common fragment shader
	fragmentProg = loadShaderProgramFromFile("shaders/common.frag", GL_FRAGMENT_SHADER);

	vertexProg[FIXED_FUNCTION_MODE] = loadShaderProgramFromFile("shaders/fixed.vert", GL_VERTEX_SHADER);
	progPipeline[FIXED_FUNCTION_MODE] = createProgramPipeline(vertexProg[FIXED_FUNCTION_MODE], 0, fragmentProg);

	vertexProg[FETCHER_MODE] = loadShaderProgramFromFile("shaders/fetcher.vert", GL_VERTEX_SHADER);
	progPipeline[FETCHER_MODE] = createProgramPipeline(vertexProg[FETCHER_MODE], 0, fragmentProg);

    vertexProg[FETCHER_IMAGE_AOS_MODE] = loadShaderProgramFromFile("shaders/fetcher_image_aos.vert", GL_VERTEX_SHADER);
    progPipeline[FETCHER_IMAGE_AOS_MODE] = createProgramPipeline(vertexProg[FETCHER_IMAGE_AOS_MODE], 0, fragmentProg);

    vertexProg[FETCHER_IMAGE_SOA_MODE] = loadShaderProgramFromFile("shaders/fetcher_image_soa.vert", GL_VERTEX_SHADER);
    progPipeline[FETCHER_IMAGE_SOA_MODE] = createProgramPipeline(vertexProg[FETCHER_IMAGE_SOA_MODE], 0, fragmentProg);

	vertexProg[PULLER_MODE] = loadShaderProgramFromFile("shaders/puller.vert", GL_VERTEX_SHADER);
	progPipeline[PULLER_MODE] = createProgramPipeline(vertexProg[PULLER_MODE], 0, fragmentProg);
}

void BuddhaDemo::renderScene(float dtsec, VertexPullingMode mode, uint64_t* elapsedNanoseconds)
{
	// update camera position
	cameraRotationFactor = fmodf(cameraRotationFactor + dtsec * 0.3f, 2.f * (float)M_PI);
	camera.position = glm::vec3(sin(cameraRotationFactor) * 5.f, 0.f, cos(cameraRotationFactor) * 5.f);
	camera.rotation = glm::vec3(0.f, -cameraRotationFactor, 0.f);

	// update camera data to uniform buffer
	transform.ModelViewMatrix = glm::mat4(1.0f);
	transform.ModelViewMatrix = glm::rotate(transform.ModelViewMatrix, camera.rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
	transform.ModelViewMatrix = glm::rotate(transform.ModelViewMatrix, camera.rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
	transform.ModelViewMatrix = glm::rotate(transform.ModelViewMatrix, camera.rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
	transform.ModelViewMatrix = translate(transform.ModelViewMatrix, -camera.position);
	transform.MVPMatrix = transform.ProjectionMatrix * transform.ModelViewMatrix;
	glBindBuffer(GL_UNIFORM_BUFFER, transformUB);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(transform), &transform, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    glClearDepth(1.f);
    glClearColor(0.f, 0.f, 0.f, 0.f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBeginQuery(GL_TIME_ELAPSED, timeElapsedQuery);

    // render scene
    glBindProgramPipeline(progPipeline[mode]);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_CULL_FACE);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, indexTexBuffer);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_BUFFER, vertexTexBuffer);

    glBindImageTexture(1, vertexTexBuffer, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R32F);

    glBindBufferBase(GL_UNIFORM_BUFFER, 0, transformUB);

    glBindVertexArray(vertexArray);

	if (drawCmd[mode].useIndices) {
		glDrawElements(drawCmd[mode].prim_type, drawCmd[mode].indexCount, GL_UNSIGNED_INT, (GLchar*)0 + drawCmd[mode].indexOffset);
	} else {
		glDrawArrays(drawCmd[mode].prim_type, drawCmd[mode].firstVertex, drawCmd[mode].vertexCount);
	}

    glBindVertexArray(0);

    glDisable(GL_CULL_FACE);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_DEPTH_TEST);
    glBindProgramPipeline(0);

    glEndQuery(GL_TIME_ELAPSED);

    if (elapsedNanoseconds)
        glGetQueryObjectui64v(timeElapsedQuery, GL_QUERY_RESULT, elapsedNanoseconds);
}

} /* namespace buddha */
