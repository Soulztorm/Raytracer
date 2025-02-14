#pragma once
#include <glm/glm.hpp>

class HDRI
{
public:
	HDRI() : m_isvalid(false), m_data(NULL), m_width(0), m_height(0) {};
	~HDRI() { free(m_data); }

	bool LoadFromFile(const char* filename);

	const glm::vec4& GetPixel(int x, int y) const { return m_data[y * m_width + x]; }
	const glm::vec4& GetPixelFromWorldDirection(const glm::vec3& worldDir) const;

	const int GetWidth() const { return m_width; }
	const int GetHeight() const { return m_height; }

	const bool IsValid() const { return m_isvalid; }

protected:
	glm::vec4* m_data = NULL;
	int m_width, m_height;

	bool m_isvalid = false;
};