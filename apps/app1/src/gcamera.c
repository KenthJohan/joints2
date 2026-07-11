#include "gcamera.h"

GCamera gcamera_create(void)
{
	GCamera camera = {
	.center = {0.0f, 0.0f},
	.zoom   = 12.0f,
	.width  = 1920.0f,
	.height = 1080.0f,
	};
	return camera;
}

void gcamera_set_viewport_size(GCamera *camera, float width, float height)
{
	camera->width  = width;
	camera->height = height;
}

void gcamera_apply_draw_origin(const GCamera *camera, Draw *draw)
{
	SetDrawOrigin(draw, camera->center);
}

void gcamera_build_projection_matrix(const GCamera *camera, float *m)
{
	float ratio = camera->width / camera->height;
	float w     = 2.0f * camera->zoom * ratio;
	float h     = 2.0f * camera->zoom;

	m[0] = 2.0f / w;
	m[1] = 0.0f;
	m[2] = 0.0f;
	m[3] = 0.0f;

	m[4] = 0.0f;
	m[5] = 2.0f / h;
	m[6] = 0.0f;
	m[7] = 0.0f;

	m[8]  = 0.0f;
	m[9]  = 0.0f;
	m[10] = -1.0f;
	m[11] = 0.0f;

	m[12] = 0.0f;
	m[13] = 0.0f;
	m[14] = 0.0f;
	m[15] = 1.0f;
}

float gcamera_get_pixel_scale(const GCamera *camera)
{
	return camera->height / camera->zoom;
}
