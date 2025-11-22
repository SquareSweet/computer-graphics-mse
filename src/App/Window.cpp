#include "Window.h"

#include <QMouseEvent>
#include <QLabel>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QVBoxLayout>
#include <QScreen>
#include <QSlider>

#include <array>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <tinygltf/tiny_gltf.h>

namespace
{

constexpr std::array<GLfloat, 8u> vertices = {
    -1.f,  1.f,
    -1.f, -1.f,
     1.f,  1.f,
     1.f, -1.f
};
constexpr std::array<GLuint, 6u> indices = {
    0, 1, 2,
    2, 1, 3
};

}// namespace

Window::Window() noexcept
{
	const auto formatFPS = [](const auto value) {
		return QString("FPS: %1").arg(QString::number(value));
	};

	auto fps = new QLabel(formatFPS(0), this);
	fps->setStyleSheet("QLabel { color : white; }");

	auto iter_slider = new QSlider(Qt::Horizontal);
	iter_slider->setRange(1, 200);
	iter_slider->setValue(max_iter_);

	auto iter_label = new QLabel(QString::number(max_iter_), this);
    iter_label->setStyleSheet("QLabel { color: white; }");

	QObject::connect(iter_slider, &QSlider::valueChanged, [this, iter_label](int new_val) {
        this->max_iter_ = new_val;
        iter_label->setText(QString::number(new_val));
    });

	auto slider_layout = new QHBoxLayout();
    slider_layout->addWidget(iter_slider, 1);
    slider_layout->addWidget(iter_label, 0);

	auto layout = new QVBoxLayout();
	layout->addWidget(fps, 0);
	layout->addStretch(1);
    layout->addLayout(slider_layout);

	setLayout(layout);

	timer_.start();

	connect(this, &Window::updateUI, [=] {
		fps->setText(formatFPS(ui_.fps));
	});
}

Window::~Window()
{
	{
		// Free resources with context bounded.
		const auto guard = bindContext();
		program_.reset();
	}
}

void Window::onInit()
{
	// Configure shaders
	program_ = std::make_unique<QOpenGLShaderProgram>(this);
	program_->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/Shaders/fractal.vs");
	program_->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/Shaders/fractal.fs");
	program_->link();

	// Create VAO object
	vao_.create();
	vao_.bind();

	// Create VBO
	vbo_.create();
	vbo_.bind();
	vbo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
	vbo_.allocate(vertices.data(), static_cast<int>(vertices.size() * sizeof(GLfloat)));

	// Create IBO
	ibo_.create();
	ibo_.bind();
	ibo_.setUsagePattern(QOpenGLBuffer::StaticDraw);
	ibo_.allocate(indices.data(), static_cast<int>(indices.size() * sizeof(GLuint)));

	// Bind attributes
	program_->bind();

	program_->enableAttributeArray(0);
	program_->setAttributeBuffer(0, GL_FLOAT, 0, 2, static_cast<int>(2 * sizeof(GLfloat)));

	loc_center_= program_->uniformLocation("center");
	loc_zoom_ = program_->uniformLocation("zoom");
	loc_resolution_ = program_->uniformLocation("resolution");
	loc_max_iter_ = program_->uniformLocation("max_iter");

	// Release all
	program_->release();

	vao_.release();

	ibo_.release();
	vbo_.release();

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

	// Bind VAO and shader program
	program_->bind();
	vao_.bind();

	// modern uniforms
	program_->setUniformValue(loc_zoom_, zoom_);
	program_->setUniformValue(loc_center_, center_);
	program_->setUniformValue(loc_resolution_, resolution_);
	program_->setUniformValue(loc_max_iter_, max_iter_);

	// Draw
	glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);

	// Release VAO and shader program
	vao_.release();
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
	resolution_.setX(width);
	resolution_.setY(height);
}

void Window::wheelEvent(QWheelEvent *event) {
	const float zoom_factor = 0.01;

	float old_zoom = zoom_;
	zoom_ *= 1 + event->angleDelta().ry() * zoom_factor;

	QVector2D mouse_pos(event->position().x() / float(resolution_.x()),
                      	1.0 - event->position().y() / float(resolution_.y()));

    float aspect = resolution_.x() / float(resolution_.y());
    QVector2D delta(
        (mouse_pos.x() - 0.5) * old_zoom,
        (mouse_pos.y() - 0.5) * (old_zoom / aspect)
    );

    center_ += delta * (1.0 - zoom_ / old_zoom);
}

void Window::mousePressEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
        dragging_ = true;
        lastMousePos_ = event->pos();
    }
}

void Window::mouseReleaseEvent(QMouseEvent *event) {
	if (event->button() == Qt::LeftButton) {
        dragging_ = false;
    }	
}

void Window::mouseMoveEvent(QMouseEvent *event) {
	if (!dragging_)
        return;

    QPoint currentPos = event->pos();
    QPoint deltaPixels = currentPos - lastMousePos_;
    lastMousePos_ = currentPos;

    if (resolution_.x() == 0 || resolution_.y() == 0)
        return;

    float aspect = resolution_.x() / float(resolution_.y());

    QVector2D deltaComplex(
        -deltaPixels.x() / resolution_.x() * zoom_,
        deltaPixels.y() / resolution_.y() * (zoom_ / aspect)
    );

    center_ += deltaComplex;	
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
			if (timer_.elapsed() >= 1000)
			{
				const auto elapsedSeconds = static_cast<float>(timer_.restart()) / 1000.0f;
				ui_.fps = static_cast<size_t>(std::round(frameCount_ / elapsedSeconds));
				frameCount_ = 0;
				emit updateUI();
			}
		}
	};
}
