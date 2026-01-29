#pragma once

#include <cstdint>
#include <cstring>

// Shared memory structure for overlay data
// This is used to communicate between the DLL and the external overlay application

#define SHARED_MEMORY_NAME L"UMVC3OverlayData"
#define MAX_CHARACTERS 6
#define MAX_HITBOXES 48
#define MAX_HURTBOXES 64
#define MAX_PROJECTILES 20

enum SharedGameState : uint32_t
{
	GameState_Unknown = 0,
	GameState_MainMenu = 1,
	GameState_CharacterSelect = 2,
	GameState_InMatch = 3
};

struct SharedHitbox
{
	float x, y, z;
	float radius;
	uint32_t active;
};

struct SharedHurtbox
{
	float x, y, z;
	float radius;
	uint32_t active;
};

struct SharedProjectile
{
	float x, y, z;
	uint32_t active;
};

struct SharedCharacter
{
	// Position in world space (from +0x50/+0x54)
	float posX, posY, posZ;
	
	// Alternate position (from +0xB0/+0xB4) - for comparison
	float altPosX, altPosY;
	
	// Character state
	uint32_t charIndex;
	uint32_t isActive;
	uint32_t isOnScreen;
	
	// Hitboxes and hurtboxes
	uint32_t hitboxCount;
	SharedHitbox hitboxes[MAX_HITBOXES];
	
	uint32_t hurtboxCount;
	SharedHurtbox hurtboxes[MAX_HURTBOXES];
};

struct SharedOverlayData
{
	// Version for compatibility checking
	uint32_t version;
	
	// Frame counter
	uint32_t frameNumber;
	
	// Camera info (for world-to-screen conversion)
	float cameraX, cameraY, cameraZ;
	float cameraFov;
	
	// Camera matrices (precise world-to-screen)
	uint32_t matrixValid;
	float viewMatrix[16];
	float projectionMatrix[16];
	
	// Game state
	uint32_t inMatch;
	uint32_t gameMode;
	uint32_t gameState;
	
	// Game resolution (detected from D3D9)
	uint32_t gameWidth;
	uint32_t gameHeight;
	
	// Character data
	uint32_t characterCount;
	SharedCharacter characters[MAX_CHARACTERS];
	
	// Projectile data
	uint32_t projectileCount;
	SharedProjectile projectiles[MAX_PROJECTILES];
	
	// Padding for future expansion
	uint8_t padding[256];
};

// Helper to zero-initialize the structure
inline void InitializeSharedOverlayData(SharedOverlayData* data)
{
	if (data)
	{
		memset(data, 0, sizeof(SharedOverlayData));
		data->version = 3;
	}
}
