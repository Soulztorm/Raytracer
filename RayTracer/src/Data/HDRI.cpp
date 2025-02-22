#pragma once
#include "HDRI.h"

#define TINYEXR_IMPLEMENTATION
#include "tinyexr.h"

#define _USE_MATH_DEFINES
#include <math.h>

#include <iostream>

#define MAX_LUMINANCE 10.0f

bool HDRI::LoadFromFile(const char* filename) {
	const char* err = NULL;
	float* data;

	// Load the actual file
	int ret = LoadEXR(&data, &m_width, &m_height, filename, &err);
	// If it didn't work, print error if any and free memory
	if (ret != TINYEXR_SUCCESS) {
		if (err) {
			fprintf(stderr, "ERR : %s\n", err);
			FreeEXRErrorMessage(err); // release memory of error message.
		}
		free(data);
		return false;
	}

	float max_lum = 0.0;
	float logSum = 0.0f;
	float epsilon = 1e-6f;

	// File is valid, fill the glm data vector with rgba
	m_data = (glm::vec4*)malloc(m_width * m_height * sizeof(glm::vec4));
	for (int y = 0; y < m_height; y++) {
		for (int x = 0; x < m_width; x++) {
			int pixelIndex = y * m_width + x;
			glm::vec4 rgba = glm::vec4(data[4 * pixelIndex], data[4 * pixelIndex + 1], data[4 * pixelIndex + 2], data[4 * pixelIndex + 3]);
			glm::vec3 rgb = glm::vec3(rgba);

			// Clamp luminance to reduce fireflies
			float lum = glm::dot(rgb, glm::vec3(0.212671f, 0.715160f, 0.072169f));

			logSum += std::log(lum + epsilon);

			if (lum > max_lum) {
				max_lum = lum;
				m_brightestUV = glm::vec2(x / (float)m_width, y / (float)m_height);
			}
			max_lum = std::max(max_lum, lum);
			if (lum > MAX_LUMINANCE)
			{
				rgba *= MAX_LUMINANCE / lum;
			}
			m_data[pixelIndex] = rgba;
		}
	}

	logSum = std::exp(logSum / (float)(m_width * m_height));

	std::cout << "Max HDRI lum        : " << max_lum << "\n\n";
	std::cout << "Log sum HDRI        : " << logSum << "\n\n";
	std::cout << "Bright spot UV HDRI : " << m_brightestUV.x << ", " << m_brightestUV.y << "\n\n";

	// Free data and flag this HDRI as valid
	free(data);
	return (m_isvalid = true);
}

const glm::vec4& HDRI::GetPixelFromWorldDirection(const glm::vec3& worldDir) const
{
	// Compute spherical coordinates
	float longitude = std::atan2(worldDir.z, worldDir.x);  // Ranges from -pi to pi
	float latitude = std::asin(worldDir.y);          // Ranges from -pi/2 to pi/2

	// Convert to normalized UV coordinates
	float u = (longitude + M_PI) / (2.0f * M_PI); // Map from [-pi, pi] to [0,1]
	float v = (M_PI_2 - latitude) / M_PI;    // Map from [-pi/2, pi/2] to [0,1], flip v

	// Convert to pixel coordinates
	int pixelX = static_cast<int>(u * m_width);
	int pixelY = static_cast<int>(v * m_height);

	pixelX = std::max(0, std::min(pixelX, m_width - 1));
	pixelY = std::max(0, std::min(pixelY, m_height - 1));

	return GetPixel(pixelX, pixelY);
}
