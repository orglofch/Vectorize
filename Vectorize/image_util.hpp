#ifndef _IMAGE_UTIL_HPP_
#define _IMAGE_UTIL_HPP_

#include <iostream>

#include <cassert>
#include <cstdint>
#include <fstream>
#include <string>
#include <png.h>
#include <vector>

#include "intrinsics.hpp"

struct Image
{
	Image() : width(0), height(0), data(NULL) {}

	~Image() {
		if (data)
			delete data;
	}

	unsigned int width;
	unsigned int height;
	unsigned int channels;
	float *data;
};

bool LoadBMP(const std::string &filename, Image *image) {
	unsigned char header[54];
	unsigned int dataPos;
	unsigned int imageSize;

	FILE *file = fopen(filename.c_str(), "rb");
	if (!file) {
		LOG("Unable to open %s\n", filename.c_str());
		return false;
	}

	if (fread(header, 1, 54, file) != 54) {
		LOG("%s is not a BMP file\n", filename.c_str());
		return false;
	}

	if (header[0] != 'B' || header[1] != 'M') {
		LOG("%s is not a BMP file\n", filename.c_str());
		return false;
	}

	dataPos = *(int*)&(header[0x0A]);
	imageSize = *(int*)&(header[0x22]);
	image->width = *(int*)&(header[0x12]);
	image->height = *(int*)&(header[0x16]);

	if (imageSize == 0) {
		imageSize = image->width * image->height * 4;
		image->channels = 4; // TODO(orglofch): Figure out location
	}
	if (dataPos == 0)      
		dataPos = 54;

	image->data = new float[imageSize];

	fread(image->data, 1, imageSize, file);

	fclose(file);
	return true;
}

bool LoadPNG(const std::string &filename, Image *image) {
	png_byte buf[8];

	FILE* in = fopen(filename.c_str(), "rb");
	if (!in)  {
		LOG("Unable to open %s\n", filename.c_str());
		return false;
	}

	for (int i = 0; i < 8; ++i) {
		if (!(buf[i] = fgetc(in))) {
			LOG("Unable to read %s\n", filename.c_str());
			return false;
		}
	}
	if (png_sig_cmp(buf, 0, 8)) {
		LOG("Bad PNG signature %s\n", filename.c_str());
		return false;
	}

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png_ptr) {
		LOG("Failed to acquire png_ptr %s\n", filename.c_str());
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr) {
		LOG("Failed to acquire info_ptr %s\n", filename.c_str());
		png_destroy_read_struct(&png_ptr, 0, 0);
		return false;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		LOG("Failed to setjmp %s\n", filename.c_str());
		png_destroy_read_struct(&png_ptr, 0, 0);
		return false;
	}

	png_init_io(png_ptr, in);

	png_set_sig_bytes(png_ptr, 8);

	png_read_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, 0);

	int bit_depth = png_get_bit_depth(png_ptr, info_ptr);
	if (bit_depth % 8) {
		png_destroy_read_struct(&png_ptr, 0, 0);
		LOG("Invalid bit depth %d, %s\n", bit_depth, filename.c_str());
		return false;
	}

	int colour_type = png_get_color_type(png_ptr, info_ptr);

	if (colour_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png_ptr);
		colour_type = PNG_COLOR_TYPE_RGB;
	}

	if (colour_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png_ptr);
	}

	if (colour_type != PNG_COLOR_TYPE_RGB
		&& colour_type != PNG_COLOR_TYPE_GRAY
		&& colour_type != PNG_COLOR_TYPE_RGBA) {
		png_destroy_read_struct(&png_ptr, 0, 0);
		LOG("Invalid colour_type %d, %s\n", colour_type, filename.c_str());
		return false;
	}

	image->width = png_get_image_width(png_ptr, info_ptr);
	image->height = png_get_image_height(png_ptr, info_ptr);
	switch (colour_type)
	{
		case PNG_COLOR_TYPE_RGB:
			image->channels = 3;
			break;
		case PNG_COLOR_TYPE_RGBA:
			image->channels = 4;
			break;
		default:
			image->channels = 1;
			break;
	}

	png_bytep* row_pointers = png_get_rows(png_ptr, info_ptr);

	image->data = new float[image->width * image->height * image->channels];
	assert(image->data);

	for (unsigned int y = 0; y < image->height; y++) {
		for (unsigned int x = 0; x < image->width; x++) {
			for (unsigned int c = 0; c < image->channels; c++) {
				png_byte *row = row_pointers[y];
				int index = image->channels * ((image->height - y - 1) * image->width + x) + c;

				long element = 0;
				for (int j = bit_depth / 8 - 1; j >= 0; j--) {
					element <<= 6;
					element += row[(x * image->channels + c) * bit_depth / 8 + j];
				}

				image->data[index] = element / static_cast<float>((1 << bit_depth) - 1);
			}
		}
	}

	png_destroy_read_struct(&png_ptr, &info_ptr, 0);

	return true;
}

#endif