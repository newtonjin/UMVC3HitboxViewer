#pragma once

#include <cstdint>

namespace CharacterOffsets
{
	const uint32_t TAG_STATE = 0x14;  // Tag state within character struct
	const uint32_t POS_X = 0xB0;  // Rendered position (lower latency than 0x50)
	const uint32_t POS_Y = 0xB4;  // Rendered position (lower latency than 0x54)
	const uint32_t POS_Z = 0x58;  // Depth position (actual Z coordinate)
	const uint32_t VEL_X = 0x58;
	const uint32_t ACTION_ID = 0x1310;
	const uint32_t HITBOX_ARRAY = 0x4200;
	const uint32_t HURTBOX_ARRAY = 0x4E10;
}

namespace TagState
{
	const uint8_t INACTIVE = 0x09;
	const uint8_t ACTIVE = 0x0D;
	const uint8_t ACTIVE_ALT = 0x68;
	const uint8_t TAG_IN = 0x6C;
	const uint8_t TAG_OUT = 0xB4;
}

class Character
{
public:
	Character(uintptr_t characterPtr);
	
	uint8_t GetTagState() const;
	uint32_t GetActionID() const;
	float GetPositionX() const;
	float GetPositionY() const;
	float GetPositionZ() const;
	float GetVelocityX() const;
	bool ShouldRender() const;
	uintptr_t GetHitboxArrayPtr() const;
	uintptr_t GetHurtboxArrayPtr() const;
	uintptr_t GetPointer() const;

private:
	uintptr_t charPtr;
};
