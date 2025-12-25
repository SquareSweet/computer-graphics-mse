#pragma once

#include <Base/GLWidget.hpp>

#include <QElapsedTimer>
#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QPushButton>
#include <QSlider>

#include <functional>
#include <memory>

#include "Scene/Scene.h"

enum class CameraMode { ClickToRotate, FreeLook };

class Window final : public fgl::GLWidget
{
	Q_OBJECT
public:
	Window() noexcept;
	~Window() override;

public: // fgl::GLWidget
	void onInit() override;
	void onRender() override;
	void onResize(size_t width, size_t height) override;

	void mousePressEvent(QMouseEvent * event) override;
	void mouseReleaseEvent(QMouseEvent * event) override;
	void mouseMoveEvent(QMouseEvent * event) override;
	void keyPressEvent(QKeyEvent * event) override;
	void keyReleaseEvent(QKeyEvent * event) override;

private:
	class PerfomanceMetricsGuard final
	{
	public:
		explicit PerfomanceMetricsGuard(std::function<void()> callback);
		~PerfomanceMetricsGuard();

		PerfomanceMetricsGuard(const PerfomanceMetricsGuard &) = delete;
		PerfomanceMetricsGuard(PerfomanceMetricsGuard &&) = delete;

		PerfomanceMetricsGuard & operator=(const PerfomanceMetricsGuard &) = delete;
		PerfomanceMetricsGuard & operator=(PerfomanceMetricsGuard &&) = delete;

	private:
		std::function<void()> callback_;
	};

private:
	[[nodiscard]] PerfomanceMetricsGuard captureMetrics();
	void createControlPanel();

signals:
	void updateUI();

private:
	GLint mvpUniform_ = -1;

	QOpenGLBuffer vbo_{QOpenGLBuffer::Type::VertexBuffer};
	QOpenGLBuffer ibo_{QOpenGLBuffer::Type::IndexBuffer};
	QOpenGLVertexArrayObject vao_;

	QMatrix4x4 model_;
	QMatrix4x4 view_;
	QMatrix4x4 projection_;

	std::unique_ptr<QOpenGLTexture> texture_;
	std::unique_ptr<QOpenGLShaderProgram> program_;

	QElapsedTimer timer_;
	QElapsedTimer fpsTimer_;
	size_t frameCount_ = 0;

	struct {
		size_t fps = 0;
	} ui_;

	bool animated_ = true;

	std::unique_ptr<Scene> scene_;

	CameraMode cameraMode_ = CameraMode::ClickToRotate;
	bool rotating_ = false; 
	QPoint lastMousePos_;
	bool firstMouse_ = true;

	float morphFactor_ = 0.0f;

	QWidget* controlPanel_ = nullptr;

	QSlider* dirIntensitySlider_ = nullptr;

	QSlider* spotIntensitySlider_ = nullptr;
	QSlider* spotInnerSlider_ = nullptr;
	QSlider* spotOuterSlider_ = nullptr;
	QPushButton* spotColorButton_ = nullptr;
	QSlider* morphSlider_ = nullptr;

	bool spotLightFollowsCamera_ = false;
};
