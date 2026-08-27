#ifdef USE_OIDN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "oidn/oidn.h"


int denoise(float *img, float *norm, float *alb, int width, int height)
{
	int i, npixels;
	OIDNDevice dev;
	OIDNFilter filter;
	float *outimg, *inimg, *fbptr;
	const char *msg;

	npixels = width * height;
	if(!(outimg = malloc(npixels * 6 * sizeof *outimg))) {
		fprintf(stderr, "denoise: failed to allocate denoise buffer\n");
		return -1;
	}
	inimg = outimg + npixels * 3;

	if(!(dev = oidnNewDevice(OIDN_DEVICE_TYPE_CPU))) {
		fprintf(stderr, "denoise: failed to create OIDN CPU device\n");
		free(outimg);
		return -1;
	}
	oidnCommitDevice(dev);

	filter = oidnNewFilter(dev, "RT");

	oidnSetSharedFilterImage(filter, "color", inimg, OIDN_FORMAT_FLOAT3, width,
			height, 0, 0, 0);
	oidnSetSharedFilterImage(filter, "normal", norm, OIDN_FORMAT_FLOAT3,
			width, height, 0, 0, 0);
	oidnSetSharedFilterImage(filter, "albedo", alb, OIDN_FORMAT_FLOAT3,
			width, height, 0, 0, 0);
	oidnSetSharedFilterImage(filter, "output", outimg, OIDN_FORMAT_FLOAT3, width,
			height, 0, 0, 0);
	oidnSetFilterBool(filter, "hdr", 1);
	oidnCommitFilter(filter);

	if(oidnGetDeviceError(dev, &msg) != OIDN_ERROR_NONE) {
		fprintf(stderr, "denoise: OIDN error: %s\n", msg);
		oidnReleaseDevice(dev);
		oidnReleaseFilter(filter);
		free(outimg);
		return -1;
	}

	fbptr = img;
	for(i=0; i<npixels; i++) {
		float s = 1.0f / fbptr[3];
		inimg[0] = fbptr[0] * s;
		inimg[1] = fbptr[1] * s;
		inimg[2] = fbptr[2] * s;
		inimg += 3;
		fbptr += 4;
	}

	oidnExecuteFilter(filter);

	fbptr = outimg;
	for(i=0; i<npixels; i++) {
		img[0] = fbptr[0];
		img[1] = fbptr[1];
		img[2] = fbptr[2];
		img[3] = 1.0f;
		fbptr += 3;
		img += 4;
	}

	free(outimg);
	oidnReleaseFilter(filter);
	oidnReleaseDevice(dev);
	return 0;
}

#else

int denoise(float *img, float *norm, float *alb, int width, int height)
{
	return -1;
}

#endif
