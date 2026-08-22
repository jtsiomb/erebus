#ifdef USE_OIDN

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <OpenImageDenoise/oidn.h>


int denoise(float *img, int width, int height)
{
	int npixels;
	OIDNDevice dev;
	OIDNFilter filter;
	float *outimg;
	const char *msg;

	npixels = width * height;
	if(!(outimg = malloc(npixels * 3 * sizeof *outimg))) {
		fprintf(stderr, "denoise: failed to allocate denoise output buffer\n");
		return -1;
	}

	if(!(dev = oidnNewDevice(OIDN_DEVICE_TYPE_CPU))) {
		fprintf(stderr, "denoise: failed to create OIDN CPU device\n");
		free(outimg);
		return -1;
	}
	oidnCommitDevice(dev);

	filter = oidnNewFilter(dev, "RT");

	oidnSetSharedFilterImage(filter, "color", img, OIDN_FORMAT_FLOAT3, width,
			height, 0, 0, 0);
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

	oidnExecuteFilter(filter);
	memcpy(img, outimg, npixels * 3 * sizeof *img);

	free(outimg);
	oidnReleaseFilter(filter);
	oidnReleaseDevice(dev);
	return 0;
}

#else

int denoise(float *img, int width, int height)
{
	return -1;
}

#endif
