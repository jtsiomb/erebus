erebus - photorealistic renderer
================================

![Erebus](http://nuclear.mutantstargoat.com/sw/erebus/img/erebus_banner_med.jpg)

About
-----
Erebus is a photorealistic renderer under development ...

License
-------
Copyright (C) 2026 John Tsiombikas <nuclear@mutantstargoat.com>

This program is Free Software. Feel free to use, modify, and/or redistribute it
under the terms of the GNU General Public License v3, or at your option any
later version published by the Free Software Foundation. See COPYING for
details.

Denoiser
--------
Erebus can be optionally compiled with denoising support through the Intel Open
Image Denoiser (OIDN): https://www.openimagedenoise.org

To build erebus with OIDN support, follow these steps:

  1. By default OIDN includes ~50mb of neural network weight data for different
     use cases. To trim it down, change into the OIDN directory and apply the
     patch provided in erebus `libs/oidn/oidn-debloat.patch`.

  2. Build OIDN as a static library, without lightmap or GPU support, naming it
     `oidn`.

    mkdir build && cd build
    cmake -DOIDN_APPS=OFF -DOIDN_DEVICE_CUDA=OFF -DOIDN_DEVICE_HIP=OFF \
          -DOIDN_DEVICE_SYCL=OFF -DOIDN_FILTER_RTLIGHTMAP=OFF \
          -DOIDN_LIBRARY_NAME=oidn -DOIDN_STATIC_LIB=ON ..

  3. Copy the three `liboidn*.a` files from the build directory to erebus
     `libs/oidn/`.

  4. Copy the header files `oidn.h` and `config.h` from
     `include/OpenImageDenoise` to erebus `libs/oidn`. If you applied the
     debloat patch in step 1, skip `config.h`.

  5. Create a `cfg.mk` file with the line `oidn = true`
