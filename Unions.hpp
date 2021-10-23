#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

union vec3_as_byte
{
	glm::vec3 vec3_value;
	unsigned char bytes_value[sizeof(glm::vec3)];
};

union quat_as_byte
{
	glm::quat quat_value;
	unsigned char bytes_value[sizeof(glm::quat)];
};