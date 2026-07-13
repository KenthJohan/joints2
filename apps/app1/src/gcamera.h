#pragma once

#include <box2d/types.h>

typedef struct GCamera {
	b2Pos center;
	float zoom;
	float width;
	float height;
} GCamera;

GCamera gcamera_create(void);
void    gcamera_set_viewport_size(GCamera *camera, float width, float height);
void    gcamera_build_projection_matrix(const GCamera *camera, float *m);
float   gcamera_get_pixel_scale(const GCamera *camera);
