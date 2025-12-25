#include <QtMath>
#include "Camera.h"

Camera::Camera()
{
	updateVectors();
}

void Camera::setAspect(float aspect)
{
	aspect_ = aspect;
}

void Camera::move(float forward, float right, float dt)
{
	const float v = speed_ * dt;
	position_ += front_ * forward * v;
	position_ += right_ * right * v;
}

void Camera::rotate(float dx, float dy)
{
	dx *= sensitivity_;
	dy *= sensitivity_;

	yaw_   += dx;
	pitch_ += dy;

	pitch_ = qBound(-89.0f, pitch_, 89.0f);
	updateVectors();
}

void Camera::updateVectors()
{
	const float yawRad   = qDegreesToRadians(yaw_);
	const float pitchRad = qDegreesToRadians(pitch_);

	front_ = QVector3D(
		std::cos(yawRad) * std::cos(pitchRad),
		std::sin(pitchRad),
		std::sin(yawRad) * std::cos(pitchRad)
	).normalized();

	right_ = QVector3D::crossProduct(front_, QVector3D(0, 1, 0)).normalized();
	up_	= QVector3D::crossProduct(right_, front_).normalized();
}

QMatrix4x4 Camera::view() const
{
	QMatrix4x4 v;
	v.lookAt(position_, position_ + front_, up_);
	return v;
}

QMatrix4x4 Camera::projection() const
{
	QMatrix4x4 p;
	p.perspective(fov_, aspect_, 0.1f, 1000.0f);
	return p;
}
