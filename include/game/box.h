#pragma once

#include <vector>
#include <cstdint>

// Hitbox/Hurtbox structure
// These are typically spheres or boxes in the game's physics engine
struct Box
{
	float x, y, z;           // Center position
	float radius;            // Radius (for sphere-based collision)
	float width, height;     // For box-based collision (optional)
	float scale_x, scale_y;  // Scale factors
	
	Box()
		: x(0), y(0), z(0), radius(0), width(0), height(0), scale_x(1.0f), scale_y(1.0f)
	{
	}
	
	Box(float x_, float y_, float z_, float r)
		: x(x_), y(y_), z(z_), radius(r), width(0), height(0), scale_x(1.0f), scale_y(1.0f)
	{
	}
};

// Hitbox array entry (attack boxes)
struct Hitbox : public Box
{
	uint32_t damage;
	uint32_t type;  // Attack type (punch, kick, projectile, etc.)
	
	Hitbox() : Box(), damage(0), type(0) {}
};

// Hurtbox array entry (damage boxes)
struct Hurtbox : public Box
{
	uint32_t priority;  // Block priority
	uint32_t type;      // Body part type
	
	Hurtbox() : Box(), priority(0), type(0) {}
};

// Class to manage reading hitbox/hurtbox data from game memory
class BoxReader
{
public:
	static std::vector<Hitbox> ReadHitboxes(uintptr_t hitboxArrayPtr, size_t maxBoxes = 32);
	static std::vector<Hurtbox> ReadHurtboxes(uintptr_t hurtboxArrayPtr, size_t maxBoxes = 32);

private:
	static constexpr size_t HITBOX_ENTRY_SIZE = 64;  // Estimated size per hitbox entry
	static constexpr size_t HURTBOX_ENTRY_SIZE = 64; // Estimated size per hurtbox entry
};
