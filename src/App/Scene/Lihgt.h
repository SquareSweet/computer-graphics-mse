#include <QVector3D>
#include <QtMath>

struct DirectionalLight {
	QVector3D direction = QVector3D(-0.2f, -1.0f, -0.3f);
	QVector3D color	    = QVector3D(1.0f, 1.0f, 1.0f);
	float intensity	    = 1.0f;
};

struct SpotLight {
	QVector3D position  = QVector3D(0.0f, 1.0f, 4.0f);
	QVector3D direction = QVector3D(0.0f, 0.1f, -1.0f).normalized();

	float innerCutoff = qCos(qDegreesToRadians(12.5f));
	float outerCutoff = qCos(qDegreesToRadians(17.5f));

	QVector3D color = QVector3D(1.0f, 0.0f, 0.0f);

	float constant  = 1.0f;
	float linear	= 0.09f;
	float quadratic = 0.032f;
};