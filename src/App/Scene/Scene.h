#pragma once

#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include "Camera.h"
#include "Lihgt.h"

struct Vertex
{
	float position[3]{0,0,0};
	float normal[3]{0,0,0};
	float texCoord[2]{0,0};
};

struct Mesh
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	QVector3D position{0,0,-2};

	QVector3D center;
	float radius;

	std::unique_ptr<QOpenGLVertexArrayObject> vao;
	std::unique_ptr<QOpenGLBuffer> vbo;
	std::unique_ptr<QOpenGLBuffer> ibo;

	std::unique_ptr<QOpenGLTexture> texture;

	void createGLObjects(QOpenGLShaderProgram* shader);
	void destroyGLObjects();
};

class Scene
{
public:
	Scene();

	void update(float dt);

	Camera& camera() { return camera_; }
	const Camera& camera() const { return camera_; }

	const std::vector<std::unique_ptr<Mesh>>& meshes() const { return meshes_; }
	std::vector<std::unique_ptr<Mesh>>& meshes() { return meshes_; }

	void applyLights(QOpenGLShaderProgram& shader);
	bool loadGLB(const QString& filename, QOpenGLShaderProgram* shader);

	bool moveForward{false};
	bool moveBackward{false};
	bool moveLeft{false};
	bool moveRight{false};

	//tempurarily public, change tp getters/setters later
	DirectionalLight dirLight_;
	SpotLight spotLight_;

private:
	Camera camera_;
	std::vector<std::unique_ptr<Mesh>> meshes_;
};
