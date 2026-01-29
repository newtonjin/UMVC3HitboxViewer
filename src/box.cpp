#include "game/box.h"
#include "core/memory.h"
#include "logger.h"

std::vector<Hitbox> BoxReader::ReadHitboxes(uintptr_t hitboxArrayPtr, size_t maxBoxes)
{
	std::vector<Hitbox> result;
	
	if (!hitboxArrayPtr) return result;
	
	// Simple memory read without SEH try/catch
	for (size_t i = 0; i < maxBoxes; ++i)
	{
		uintptr_t boxAddr = hitboxArrayPtr + (i * HITBOX_ENTRY_SIZE);
		
		// Read box data from memory (offsets may vary)
		float x = Memory::Read<float>(boxAddr + 0x00);
		float y = Memory::Read<float>(boxAddr + 0x04);
		float z = Memory::Read<float>(boxAddr + 0x08);
		float radius = Memory::Read<float>(boxAddr + 0x0C);
		
		// Check if box is valid (radius should be > 0)
		if (radius <= 0.0f) continue;
		
		Hitbox box;
		box.x = x;
		box.y = y;
		box.z = z;
		box.radius = radius;
		box.damage = Memory::Read<uint32_t>(boxAddr + 0x10);
		box.type = Memory::Read<uint32_t>(boxAddr + 0x14);
		box.width = Memory::Read<float>(boxAddr + 0x18);
		box.height = Memory::Read<float>(boxAddr + 0x1C);
		box.scale_x = Memory::Read<float>(boxAddr + 0x20);
		box.scale_y = Memory::Read<float>(boxAddr + 0x24);
		
		result.push_back(box);
	}
	
	return result;
}

std::vector<Hurtbox> BoxReader::ReadHurtboxes(uintptr_t hurtboxArrayPtr, size_t maxBoxes)
{
	std::vector<Hurtbox> result;
	
	if (!hurtboxArrayPtr) return result;
	
	// Simple memory read without SEH try/catch
	for (size_t i = 0; i < maxBoxes; ++i)
	{
		uintptr_t boxAddr = hurtboxArrayPtr + (i * HURTBOX_ENTRY_SIZE);
		
		// Read box data from memory
		float x = Memory::Read<float>(boxAddr + 0x00);
		float y = Memory::Read<float>(boxAddr + 0x04);
		float z = Memory::Read<float>(boxAddr + 0x08);
		float radius = Memory::Read<float>(boxAddr + 0x0C);
		
		// Check if box is valid
		if (radius <= 0.0f) continue;
		
		Hurtbox box;
		box.x = x;
		box.y = y;
		box.z = z;
		box.radius = radius;
		box.priority = Memory::Read<uint32_t>(boxAddr + 0x10);
		box.type = Memory::Read<uint32_t>(boxAddr + 0x14);
		box.width = Memory::Read<float>(boxAddr + 0x18);
		box.height = Memory::Read<float>(boxAddr + 0x1C);
		box.scale_x = Memory::Read<float>(boxAddr + 0x20);
		box.scale_y = Memory::Read<float>(boxAddr + 0x24);
		
		result.push_back(box);
	}
	
	return result;
}
