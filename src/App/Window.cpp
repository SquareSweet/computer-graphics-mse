#include "Window.h"

#include <QMouseEvent>
#include <QLabel>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVBoxLayout>
#include <QScreen>
#include <QSlider>
#include <QGroupBox>
#include <QColorDialog>

#include <array>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tinygltf/tiny_gltf.h>

Window::Window() noexcept
{
	const auto formatFPS = [](const auto value) {
		return QString("FPS: %1").arg(QString::number(value));
	};

	auto fps = new QLabel(formatFPS(0), this);
	fps->setStyleSheet("QLabel { color : white; }");

	auto controlsLabel = new QLabel(
		"Controls:\n"
		"  - WASD: Move camera\n"
		"  - Mouse Drag (LMB): Rotate camera\n"
		"  - L: Snap spotlight to camera\n"
		"  - F: Free camera mode", 
		this
	);
	controlsLabel->setStyleSheet("color: white;");
	controlsLabel->setWordWrap(true);
	controlsLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

	auto layout = new QVBoxLayout();
	layout->addWidget(fps, 0);
	layout->addWidget(controlsLabel, 0);
	layout->addStretch(1);

	createControlPanel();
	layout->addWidget(controlPanel_, 0);

	setLayout(layout);

	timer_.start();
	fpsTimer_.start();

	connect(this, &Window::updateUI, [=] {
		fps->setText(formatFPS(ui_.fps));
	});

	setMouseTracking(true);
}

Window::~Window()
{
	{
		// Free resources with context bounded.
		const auto guard = bindContext();
		texture_.reset();
		program_.reset();
	}
}

void Window::onInit()
{
	program_ = std::make_unique<QOpenGLShaderProgram>(this);
	program_->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/Shaders/model.vs");
	program_->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/Shaders/model.fs");
	program_->link();

	mvpUniform_ = program_->uniformLocation("mvp");

	scene_ = std::make_unique<Scene>();

	if (!scene_->loadGLB(":/Models/pallas_cat.glb", program_.get())) {
		qWarning() << "Failed to load GLB model";
	} else {
		qDebug() << "Loaded" << scene_->meshes().size() << "meshes";
	}
	
	// Еnable depth test and face culling
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// Clear all FBO buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Window::onRender()
{
	const auto guard = captureMetrics();

	// Clear buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	program_->bind();

	if (spotLightFollowsCamera_) {
		scene_->spotLight_.position  = scene_->camera().position();
		scene_->spotLight_.direction = scene_->camera().direction();
	}

	scene_->applyLights(*program_);

	static qint64 lastTime = timer_.elapsed();
	qint64 now = timer_.elapsed();
	float dt = float(now - lastTime) / 1000.0f;
	lastTime = now;

	scene_->update(dt);

	//qDebug() << "Rendering" << scene_->meshes().size() << "meshes";

	for (size_t i = 0; i < scene_->meshes().size(); ++i) {
		auto& mesh = scene_->meshes()[i];

		if (!mesh->vao || !mesh->vao->isCreated()) {
			qWarning() << "Mesh" << i << "VAO is not valid!";
			continue;
		}

		QMatrix4x4 model;
		model.setToIdentity();

		float scaleFactor = 0.1f;
		model.scale(scaleFactor);
		model.rotate(-90.0f, 1.0f, 0.0f, 0.0f);
		model.translate(mesh->position);

		QMatrix4x4 mvp = scene_->camera().projection() * scene_->camera().view() * model;
		QMatrix3x3 normalMatrix = model.normalMatrix();

		program_->setUniformValue(mvpUniform_, mvp);
		program_->setUniformValue("model", model);
		program_->setUniformValue("normalMatrix", normalMatrix);

		program_->setUniformValue("morphFactor", morphFactor_);
		program_->setUniformValue("sphereRadius", mesh->radius);
		program_->setUniformValue("modelCenter", mesh->center);

		if (mesh->texture)
		{
			mesh->texture->bind(0);
			int diffuseLoc = program_->uniformLocation("diffuseTexture");
			program_->setUniformValue(diffuseLoc, 0);
			// qDebug() << "Mesh" << i << "binding texture";
		}

		if (mesh->vao)
		{
			mesh->vao->bind();
			glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh->indices.size()), GL_UNSIGNED_INT, nullptr);
			mesh->vao->release();
		}

		if (mesh->texture)
		{
			mesh->texture->release();
		}
	}

	program_->release();

	++frameCount_;

	// Request redraw if animated
	if (animated_)
	{
		update();
	}
}

void Window::onResize(const size_t width, const size_t height)
{
	// Configure viewport
	glViewport(0, 0, static_cast<GLint>(width), static_cast<GLint>(height));
	scene_->camera().setAspect(float(width) / float(height));
}

void Window::mousePressEvent(QMouseEvent* e)
{
	if (cameraMode_ == CameraMode::ClickToRotate && e->button() == Qt::LeftButton)
	{
		rotating_ = true;
		lastMousePos_ = e->pos();
	}
}

void Window::mouseReleaseEvent(QMouseEvent* e)
{
	if (cameraMode_ == CameraMode::ClickToRotate && e->button() == Qt::LeftButton)
	{
		rotating_ = false;
		firstMouse_ = true;
	}
}

void Window::mouseMoveEvent(QMouseEvent* e)
{
	if (firstMouse_)
	{
		lastMousePos_ = e->pos();
		firstMouse_ = false;
		return;
	}

	float dx = e->pos().x() - lastMousePos_.x();
	float dy = lastMousePos_.y() - e->pos().y();

	if (cameraMode_ == CameraMode::FreeLook || (cameraMode_ == CameraMode::ClickToRotate && rotating_))
	{
		scene_->camera().rotate(dx, dy);
	}

	lastMousePos_ = e->pos();

	if (cameraMode_ == CameraMode::FreeLook)
	{
		QPoint center = rect().center();
		QCursor::setPos(mapToGlobal(center));
		lastMousePos_ = center;
	}
}

void Window::keyPressEvent(QKeyEvent* e)
{
	if(e->key() == Qt::Key_F)
	{
		if (cameraMode_ == CameraMode::ClickToRotate) {
			cameraMode_ = CameraMode::FreeLook;
			grabMouse();
		} else {
			cameraMode_ = CameraMode::ClickToRotate;
			releaseMouse();
		}

		firstMouse_ = true;
		setCursor(cameraMode_ == CameraMode::FreeLook ? Qt::BlankCursor : Qt::ArrowCursor);
	}

	if(e->key() == Qt::Key_L) {
		spotLightFollowsCamera_ = !spotLightFollowsCamera_;
		if (spotLightFollowsCamera_) {
			qDebug() << "Spot light now follows camera";
		} else {
			qDebug() << "Spot light is independent";
		}
	}

	if(e->key() == Qt::Key_W) scene_->moveForward = true;
	if(e->key() == Qt::Key_S) scene_->moveBackward = true;
	if(e->key() == Qt::Key_A) scene_->moveLeft = true;
	if(e->key() == Qt::Key_D) scene_->moveRight = true;
}

void Window::keyReleaseEvent(QKeyEvent* e)
{
	if(e->key() == Qt::Key_W) scene_->moveForward = false;
	if(e->key() == Qt::Key_S) scene_->moveBackward = false;
	if(e->key() == Qt::Key_A) scene_->moveLeft = false;
	if(e->key() == Qt::Key_D) scene_->moveRight = false;
}

Window::PerfomanceMetricsGuard::PerfomanceMetricsGuard(std::function<void()> callback)
	: callback_{ std::move(callback) }
{
}

Window::PerfomanceMetricsGuard::~PerfomanceMetricsGuard()
{
	if (callback_)
	{
		callback_();
	}
}

auto Window::captureMetrics() -> PerfomanceMetricsGuard
{
	return PerfomanceMetricsGuard{
		[&] {
			if (fpsTimer_.elapsed() >= 1000)
			{
				const auto elapsedSeconds = static_cast<float>(fpsTimer_.restart()) / 1000.0f;
				ui_.fps = static_cast<size_t>(std::round(frameCount_ / elapsedSeconds));
				frameCount_ = 0;
				emit updateUI();
			}
		}
	};
}

void Window::createControlPanel()
{
	controlPanel_ = new QWidget(this);
	controlPanel_->setFixedWidth(260);
	controlPanel_->setStyleSheet("background:#2b2b2b; color:white;");

	auto* layout = new QVBoxLayout(controlPanel_);
	layout->setSpacing(8);

	auto* dirGroup = new QGroupBox("Directional Light");
	dirGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
	auto* dirLayout = new QVBoxLayout(dirGroup);

	dirLayout->addWidget(new QLabel("Intensity"));

	dirIntensitySlider_ = new QSlider(Qt::Horizontal);
	dirIntensitySlider_->setRange(0, 200);
	dirIntensitySlider_->setValue(100);
	dirLayout->addWidget(dirIntensitySlider_);

	QObject::connect(dirIntensitySlider_, &QSlider::valueChanged, [this](int v) {
		float k = v / 100.0f;
		scene_->dirLight_.color = QVector3D(k, k, k);
		update();
	});

	layout->addWidget(dirGroup);

	auto* spotGroup = new QGroupBox("Spot Light");
	spotGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
	auto* spotLayout = new QVBoxLayout(spotGroup);

	spotLayout->addWidget(new QLabel("Inner angle"));
	spotInnerSlider_ = new QSlider(Qt::Horizontal);
	spotInnerSlider_->setRange(1, 45);
	spotInnerSlider_->setValue(12);
	spotLayout->addWidget(spotInnerSlider_);

	spotLayout->addWidget(new QLabel("Outer angle"));
	spotOuterSlider_ = new QSlider(Qt::Horizontal);
	spotOuterSlider_->setRange(1, 60);
	spotOuterSlider_->setValue(17);
	spotLayout->addWidget(spotOuterSlider_);

	QObject::connect(spotInnerSlider_, &QSlider::valueChanged, [this](int v) {
		scene_->spotLight_.innerCutoff = qCos(qDegreesToRadians(float(v)));
		update();
	});

	QObject::connect(spotOuterSlider_, &QSlider::valueChanged, [this](int v) {
		scene_->spotLight_.outerCutoff = qCos(qDegreesToRadians(float(v)));
		update();
	});

	auto* colorPalette = new QWidget;
	auto* paletteLayout = new QHBoxLayout(colorPalette);
	paletteLayout->setSpacing(4);
	paletteLayout->setContentsMargins(0,0,0,0);

	std::vector<QColor> colors = { Qt::red, Qt::green, Qt::blue, Qt::yellow, Qt::magenta };

	for (const auto& c : colors) {
		auto* colorBtn = new QPushButton;
		colorBtn->setFixedSize(24,24);
		colorBtn->setStyleSheet(QString("background-color:%1; border:1px solid white;").arg(c.name()));
		colorBtn->setCursor(Qt::PointingHandCursor);

		QObject::connect(colorBtn, &QPushButton::clicked, [this, c]() {
			scene_->spotLight_.color = QVector3D(c.redF(), c.greenF(), c.blueF());
			update();
		});

		paletteLayout->addWidget(colorBtn);
	}

	spotLayout->addWidget(new QLabel("Spot Color:"));
	spotLayout->addWidget(colorPalette);

	layout->addWidget(spotGroup);
	layout->addStretch();

	auto* morphGroup = new QGroupBox("Morphing");
	morphGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
	auto* morphLayout = new QVBoxLayout(morphGroup);

	morphLayout->addWidget(new QLabel("Shpereness"));

	morphSlider_ = new QSlider(Qt::Horizontal);
	morphSlider_->setRange(0, 100);
	morphSlider_->setValue(0);
	morphLayout->addWidget(morphSlider_);

	QObject::connect(morphSlider_, &QSlider::valueChanged, [this](int value)
	{
		morphFactor_ = value / 100.0f;
		// qDebug() << "Morph factor:" << morphFactor_;
	});

	layout->addWidget(morphGroup);
}
