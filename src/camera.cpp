#include "game/camera.h"
#include "core/memory.h"
#include "logger.h"
#include <cstring>

Camera::Camera(uintptr_t cameraPtr)
	: cameraPtr(cameraPtr)
{
}

Matrix4x4 Camera::ReadMatrix(uintptr_t address) const
{
	Matrix4x4 result;
	result.Identity();
	
	if (!address) return result;
	
	// Simple memory read - SEH try/catch not used to avoid C++/CLI issues
	// In production, this would have proper exception handling
	for (int i = 0; i < 16; ++i)
	{
		// Use guarded memory read to avoid crashing on bad camera pointers
		float v = Memory::Read<float>(address + i * sizeof(float));
		result.m[i / 4][i % 4] = v;
	}
	
	return result;
}

Matrix4x4 Camera::GetViewMatrix() const
{
	Matrix4x4 identity;
	identity.Identity();
	
	if (!cameraPtr) return identity;
	
	// The camera object should be at cameraPtr + CAMERA_OBJECT
	uintptr_t cameraObj = Memory::Read<uintptr_t>(cameraPtr + CameraOffsets::CAMERA_OBJECT);
	
	if (!cameraObj) return identity;
	
	// Read view matrix from camera object
	return ReadMatrix(cameraObj + CameraOffsets::VIEW_MATRIX);
}

Matrix4x4 Camera::GetProjectionMatrix() const
{
	Matrix4x4 identity;
	identity.Identity();
	
	if (!cameraPtr) return identity;
	
	// The camera object should be at cameraPtr + CAMERA_OBJECT
	uintptr_t cameraObj = Memory::Read<uintptr_t>(cameraPtr + CameraOffsets::CAMERA_OBJECT);
	
	if (!cameraObj) return identity;
	
	// Read projection matrix from camera object
	return ReadMatrix(cameraObj + CameraOffsets::PROJECTION_MATRIX);
}

// Simple matrix multiplication for 4x4 matrices
static Matrix4x4 MultiplyMatrices(const Matrix4x4& a, const Matrix4x4& b) {
	Matrix4x4 result;
	for (int i = 0; i < 4; ++i) {
		for (int j = 0; j < 4; ++j) {
			result.m[i][j] = 0.0f;
			for (int k = 0; k < 4; ++k) {
				result.m[i][j] += a.m[i][k] * b.m[k][j];
			}
		}
	}
	return result;
}

Matrix4x4 Camera::GetViewProjectionMatrix() const
{
	return MultiplyMatrices(GetProjectionMatrix(), GetViewMatrix());
}

Vec2 Camera::WorldToScreen(const Vec3& worldPos) const
{
	Vec2 screenPos(-1.0f, -1.0f); // Invalid position
	
	Matrix4x4 vpMatrix = GetViewProjectionMatrix();
	
	// Transform world position to clip space (simple matrix-vector multiply)
	float clipX = vpMatrix.m[0][0] * worldPos.x + vpMatrix.m[0][1] * worldPos.y + vpMatrix.m[0][2] * worldPos.z + vpMatrix.m[0][3];
	float clipY = vpMatrix.m[1][0] * worldPos.x + vpMatrix.m[1][1] * worldPos.y + vpMatrix.m[1][2] * worldPos.z + vpMatrix.m[1][3];
	float clipZ = vpMatrix.m[2][0] * worldPos.x + vpMatrix.m[2][1] * worldPos.y + vpMatrix.m[2][2] * worldPos.z + vpMatrix.m[2][3];
	float clipW = vpMatrix.m[3][0] * worldPos.x + vpMatrix.m[3][1] * worldPos.y + vpMatrix.m[3][2] * worldPos.z + vpMatrix.m[3][3];
	
	// Perform perspective divide
	if (clipW == 0.0f) return screenPos;
	
	float invW = 1.0f / clipW;
	float ndcX = clipX * invW;
	float ndcY = clipY * invW;
	
	screenPos.x = ndcX;
	screenPos.y = ndcY;
	
	return screenPos;
}

bool Camera::IsPositionVisible(const Vec3& worldPos) const
{
	Matrix4x4 vpMatrix = GetViewProjectionMatrix();
	
	// Transform world position to clip space
	float clipX = vpMatrix.m[0][0] * worldPos.x + vpMatrix.m[0][1] * worldPos.y + vpMatrix.m[0][2] * worldPos.z + vpMatrix.m[0][3];
	float clipY = vpMatrix.m[1][0] * worldPos.x + vpMatrix.m[1][1] * worldPos.y + vpMatrix.m[1][2] * worldPos.z + vpMatrix.m[1][3];
	float clipZ = vpMatrix.m[2][0] * worldPos.x + vpMatrix.m[2][1] * worldPos.y + vpMatrix.m[2][2] * worldPos.z + vpMatrix.m[2][3];
	float clipW = vpMatrix.m[3][0] * worldPos.x + vpMatrix.m[3][1] * worldPos.y + vpMatrix.m[3][2] * worldPos.z + vpMatrix.m[3][3];
	
	// Check if position is within clip space bounds (w != 0)
	if (clipW <= 0.0f) return false;
	
	// Check NDC bounds (-1 to 1)
	float invW = 1.0f / clipW;
	float ndc_x = clipX * invW;
	float ndc_y = clipY * invW;
	float ndc_z = clipZ * invW;
	
	return ndc_x >= -1.0f && ndc_x <= 1.0f &&
		   ndc_y >= -1.0f && ndc_y <= 1.0f &&
		   ndc_z >= -1.0f && ndc_z <= 1.0f;
}
