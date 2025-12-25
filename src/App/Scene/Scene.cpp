#include "Scene.h"
#include <tinygltf/tiny_gltf.h>
#include <QResource>
#include <QDebug>

Scene::Scene() = default;

void Mesh::createGLObjects(QOpenGLShaderProgram* shader)
{
	if (!shader) return;

	vao = std::make_unique<QOpenGLVertexArrayObject>();
	vao->create();
	vao->bind();

	vbo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::VertexBuffer);
	vbo->create();
	vbo->bind();
	vbo->setUsagePattern(QOpenGLBuffer::StaticDraw);
	vbo->allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(Vertex)));

	ibo = std::make_unique<QOpenGLBuffer>(QOpenGLBuffer::IndexBuffer);
	ibo->create();
	ibo->bind();
	ibo->setUsagePattern(QOpenGLBuffer::StaticDraw);
	ibo->allocate(indices.data(), static_cast<int>(indices.size() * sizeof(uint32_t)));

	shader->bind();
	shader->enableAttributeArray(0);
	shader->setAttributeBuffer(0, GL_FLOAT, offsetof(Vertex, position), 3, sizeof(Vertex));

	shader->enableAttributeArray(1);
	shader->setAttributeBuffer(1, GL_FLOAT, offsetof(Vertex, normal), 3, sizeof(Vertex));

	shader->enableAttributeArray(2);
	shader->setAttributeBuffer(2, GL_FLOAT, offsetof(Vertex, texCoord), 2, sizeof(Vertex));

	vao->release();
	vbo->release();
	ibo->release();
	shader->release();
}

void Mesh::destroyGLObjects()
{
	if (vbo) vbo->destroy();
	if (ibo) ibo->destroy();
	if (vao) vao->destroy();
}

void Scene::update(float dt)
{
	float forward = 0.0f;
	float right = 0.0f;

	if(moveForward)  forward += 1.0f;
	if(moveBackward) forward -= 1.0f;
	if(moveRight)	right += 1.0f;
	if(moveLeft)	 right -= 1.0f;

	camera_.move(forward, right, dt);
}

void Scene::applyLights(QOpenGLShaderProgram& shader)
{
	shader.setUniformValue("viewPos", camera_.position());

	shader.setUniformValue("dirLight.direction", dirLight_.direction);
	shader.setUniformValue("dirLight.color", dirLight_.color);
	shader.setUniformValue("dirLight.intensity", dirLight_.intensity);

	shader.setUniformValue("spotLight.position", spotLight_.position);
	shader.setUniformValue("spotLight.direction", spotLight_.direction);

	shader.setUniformValue("spotLight.innerCutoff", spotLight_.innerCutoff);
	shader.setUniformValue("spotLight.outerCutoff", spotLight_.outerCutoff);

	shader.setUniformValue("spotLight.color", spotLight_.color);

	shader.setUniformValue("spotLight.constant", spotLight_.constant);
	shader.setUniformValue("spotLight.linear", spotLight_.linear);
	shader.setUniformValue("spotLight.quadratic", spotLight_.quadratic);
}

bool Scene::loadGLB(const QString& filename, QOpenGLShaderProgram* shader)
{
	qDebug() << "Loading GLB:" << filename;

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err, warn;

	QResource res(filename);
	if (!res.isValid()) {
		qWarning() << "Scene::loadGLB: invalid resource" << filename;
		return false;
	}

	QByteArray data(reinterpret_cast<const char*>(res.data()), res.size());
	qDebug() << "Resource size:" << data.size() << "bytes";

	bool ret = loader.LoadBinaryFromMemory(&model, &err, &warn,
										   reinterpret_cast<const unsigned char*>(data.data()),
										   static_cast<unsigned int>(data.size()), "");
	if (!warn.empty()) qDebug() << "WARN:" << QString::fromStdString(warn);
	if (!err.empty()) qDebug() << "ERR:" << QString::fromStdString(err);
	if (!ret) return false;

	qDebug() << "Loaded model:" << model.meshes.size() << "meshes," 
			 << model.nodes.size() << "nodes";

	meshes_.clear();

	std::vector<std::unique_ptr<QOpenGLTexture>> textures;
	for (size_t i = 0; i < model.images.size(); ++i)
	{
		const auto& image = model.images[i];
		QImage qimg;
		if (image.component == 3)
			qimg = QImage(image.image.data(), image.width, image.height, QImage::Format_RGB888);
		else if (image.component == 4)
			qimg = QImage(image.image.data(), image.width, image.height, QImage::Format_RGBA8888);
		else {
			qWarning() << "Unsupported image format for texture" << i;
			continue;
		}

		auto tex = std::make_unique<QOpenGLTexture>(qimg);
		tex->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
		tex->setWrapMode(QOpenGLTexture::Repeat);
		tex->generateMipMaps();

		qDebug() << "Loaded texture" << i << "size:" << qimg.width() << "x" << qimg.height();

		textures.push_back(std::move(tex));
	}

	for (size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex)
	{
		const auto& mesh = model.meshes[meshIndex];
		qDebug() << "Mesh" << meshIndex << "has" << mesh.primitives.size() << "primitives";

		for (size_t primIndex = 0; primIndex < mesh.primitives.size(); ++primIndex)
		{
			const auto& prim = mesh.primitives[primIndex];
			qDebug() << "Primitive" << primIndex;

			auto m = std::make_unique<Mesh>();

			auto posIt = prim.attributes.find("POSITION");
			if (posIt == prim.attributes.end()) {
				qWarning() << "Primitive has no POSITION attribute, skipping";
				continue;
			}
			const auto& posAccessor = model.accessors[posIt->second];
			const auto& posBufferView = model.bufferViews[posAccessor.bufferView];
			const auto& posBuffer = model.buffers[posBufferView.buffer];
			const float* positions = reinterpret_cast<const float*>(posBuffer.data.data() + posBufferView.byteOffset + posAccessor.byteOffset);

			size_t vertexCount = posAccessor.count;
			m->vertices.resize(vertexCount);

			for (size_t i = 0; i < vertexCount; ++i) {
				m->vertices[i].position[0] = positions[i*3 + 0];
				m->vertices[i].position[1] = positions[i*3 + 1];
				m->vertices[i].position[2] = positions[i*3 + 2];
			}
			qDebug() << "Vertex count:" << vertexCount;

			QVector3D min, max;
			for (auto& v : m->vertices)
			{
				min.setX(std::min(min.x(), v.position[0]));
				min.setY(std::min(min.y(), v.position[1]));
				min.setZ(std::min(min.z(), v.position[2]));

				max.setX(std::max(max.x(), v.position[0]));
				max.setY(std::max(max.y(), v.position[1]));
				max.setZ(std::max(max.z(), v.position[2]));
			}

			m->center = (min + max) * 0.5f;
			m->radius = (max - m->center).length();

			auto normIt = prim.attributes.find("NORMAL");
			if (normIt != prim.attributes.end()) {
				const auto& normAccessor = model.accessors[normIt->second];
				const auto& normBufferView = model.bufferViews[normAccessor.bufferView];
				const auto& normBuffer = model.buffers[normBufferView.buffer];
				const float* normals = reinterpret_cast<const float*>(normBuffer.data.data() + normBufferView.byteOffset + normAccessor.byteOffset);
				for (size_t i = 0; i < vertexCount; ++i) {
					m->vertices[i].normal[0] = normals[i*3 + 0];
					m->vertices[i].normal[1] = normals[i*3 + 1];
					m->vertices[i].normal[2] = normals[i*3 + 2];
				}
			}

			auto uvIt = prim.attributes.find("TEXCOORD_0");
			if (uvIt != prim.attributes.end()) {
				const auto& uvAccessor = model.accessors[uvIt->second];
				const auto& uvBufferView = model.bufferViews[uvAccessor.bufferView];
				const auto& uvBuffer = model.buffers[uvBufferView.buffer];
				const float* uvs = reinterpret_cast<const float*>(uvBuffer.data.data() + uvBufferView.byteOffset + uvAccessor.byteOffset);
				for (size_t i = 0; i < vertexCount; ++i) {
					m->vertices[i].texCoord[0] = uvs[i*2 + 0];
					m->vertices[i].texCoord[1] = uvs[i*2 + 1];
				}
			}

			if (prim.indices >= 0) {
				const auto& idxAccessor = model.accessors[prim.indices];
				const auto& idxBufferView = model.bufferViews[idxAccessor.bufferView];
				const auto& idxBuffer = model.buffers[idxBufferView.buffer];

				m->indices.resize(idxAccessor.count);
				if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
					qDebug() << "Primitive has short indices";
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(idxBuffer.data.data() + idxBufferView.byteOffset + idxAccessor.byteOffset);
					for (size_t i=0; i<idxAccessor.count; ++i) m->indices[i] = buf[i];
				} else if (idxAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
					qDebug() << "Primitive has unsigned int indices";
					const uint32_t* buf = reinterpret_cast<const uint32_t*>(idxBuffer.data.data() + idxBufferView.byteOffset + idxAccessor.byteOffset);
					for (size_t i=0; i<idxAccessor.count; ++i) m->indices[i] = buf[i];
				} else {
					qWarning() << "Unsupported index component type";
					continue;
				}
			} else {
				m->indices.resize(vertexCount);
				for (size_t i = 0; i < vertexCount; ++i) m->indices[i] = static_cast<uint32_t>(i);
				qDebug() << "Primitive has no indices, generated default indices";
			}

			if (prim.material >= 0 && prim.material < static_cast<int>(model.materials.size())) {
				const auto& material = model.materials[prim.material];
				if (material.pbrMetallicRoughness.baseColorTexture.index >= 0) {
					int texIndex = material.pbrMetallicRoughness.baseColorTexture.index;
					if (texIndex < static_cast<int>(textures.size())) {
						m->texture = std::move(textures[texIndex]);
						qDebug() << "Assigned texture" << texIndex << "to mesh" << meshIndex;
					} else {
						qWarning() << "Texture index out of range for mesh" << meshIndex;
					}
				}
			}

			m->createGLObjects(shader);
			meshes_.push_back(std::move(m));
		}
	}

	qDebug() << "Total meshes loaded:" << meshes_.size();
	return true;
}
