#pragma once

#include <d3d9.h>
#include <cstdint>
#include <cmath>

// Simple 2D/3D vector structs (replace d3dx9)
struct Vec2 {
	float x, y;
	Vec2() : x(0), y(0) {}
	Vec2(float x, float y) : x(x), y(y) {}
};

struct Vec3 {
	float x, y, z;
	Vec3() : x(0), y(0), z(0) {}
	Vec3(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Vec4 {
	float x, y, z, w;
	Vec4() : x(0), y(0), z(0), w(1.0f) {}
	Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

// Simple 4x4 matrix (replaces D3DXMATRIX)
struct Matrix4x4 {
	float m[4][4];
	Matrix4x4() { Identity(); }
	
	void Identity() {
		memset(m, 0, sizeof(m));
		m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
	}
};

namespace CameraOffsets
{
	const uint32_t CAMERA_OBJECT = 0x50;      // Offset from PTR_CAMERA_STATIC
	const uint32_t VIEW_MATRIX = 0xD0;        // Offset within camera object (approximate)
	const uint32_t PROJECTION_MATRIX = 0x100; // Offset within camera object (approximate)
}

class Camera
{
public:
	Camera(uintptr_t cameraPtr);
	
	// Get matrices
	Matrix4x4 GetViewMatrix() const;
	Matrix4x4 GetProjectionMatrix() const;
	Matrix4x4 GetViewProjectionMatrix() const;
	
	// World to screen transformation
	Vec2 WorldToScreen(const Vec3& worldPos) const;
	
	// Check if position is visible
	bool IsPositionVisible(const Vec3& worldPos) const;

private:
	uintptr_t cameraPtr;
	
	// Helper to read 4x4 matrix from memory
	Matrix4x4 ReadMatrix(uintptr_t address) const;
};
