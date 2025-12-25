#pragma once

#include <QMatrix4x4>
#include <QVector3D>

class Camera
{
public:
	Camera();

	void setAspect(float aspect);

	void move(float forward, float right, float dt);
	void rotate(float dx, float dy);

	QMatrix4x4 view() const;
	QMatrix4x4 projection() const;

	QVector3D position() const { return position_; }
	QVector3D direction() const { return front_; }

private:
	void updateVectors();

private:
	QVector3D position_{0.0f, 1.0f, 3.0f};
	QVector3D front_{0.0f, 0.0f, -1.0f};
	QVector3D right_{1.0f, 0.0f, 0.0f};
	QVector3D up_{0.0f, 1.0f, 0.0f};

	float yaw_   = -90.0f;
	float pitch_ = 0.0f;

	float fov_ = 60.0f;
	float aspect_ = 1.0f;

	float speed_ = 3.0f;
	float sensitivity_ = 0.1f;
};
