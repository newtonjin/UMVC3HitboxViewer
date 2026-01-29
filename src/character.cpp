
#include "game/character.h"
#include "core/memory.h"
#include <cmath>

Character::Character(uintptr_t characterPtr)
	: charPtr(characterPtr)
{
}

uint8_t Character::GetTagState() const
{
	if (!charPtr) return 0;
	return Memory::Read<uint8_t>(charPtr + CharacterOffsets::TAG_STATE);
}

uint32_t Character::GetActionID() const
{
	if (!charPtr) return 0;
	return Memory::Read<uint32_t>(charPtr + CharacterOffsets::ACTION_ID);
}

float Character::GetPositionX() const
{
	if (!charPtr) return 0.0f;
	return Memory::Read<float>(charPtr + CharacterOffsets::POS_X);
}

float Character::GetPositionY() const
{
	if (!charPtr) return 0.0f;
	return Memory::Read<float>(charPtr + CharacterOffsets::POS_Y);
}

float Character::GetPositionZ() const
{
	if (!charPtr) return 0.0f;
	return Memory::Read<float>(charPtr + CharacterOffsets::POS_Z);
}

float Character::GetVelocityX() const
{
	if (!charPtr) return 0.0f;
	// Se VEL_X não estiver definido no header, tente usar 0x58 (que funcionou no Python como pseudo-velocidade)
	// Caso contrário, use o offset correto se soubermos (geralmente próximo de 0x50)
	return Memory::Read<float>(charPtr + CharacterOffsets::VEL_X);
}

bool Character::ShouldRender() const
{
	if (!charPtr) return false;

	uint8_t tagState = GetTagState();
	float posX = GetPositionX();
	float posY = GetPositionY();

	// Rule 0: TAG_OUT means character is leaving via hardtag - don't render
	if (tagState == TagState::TAG_OUT)
	{
		return false;
	}

	// Rule 1: If ACTIVE or TAG_IN, always render (point character)
	if (tagState == TagState::ACTIVE || tagState == TagState::ACTIVE_ALT || tagState == TagState::TAG_IN)
	{
		return true;
	}

	// Rule 2: If INACTIVE, check if it's an active assist ON SCREEN
	if (tagState == TagState::INACTIVE)
	{
		uint32_t actionID = GetActionID();

		// 1. Action Check: Se ActionID for 0, é banco.
		if (actionID == 0) return false;

		// 2. CORREÇÃO DE GHOSTING (NOVA): Filtro de Estática
		// Personagens fantasmas no corner ou pós-tag costumam ficar parados (Vel = 0)
		// enquanto mantêm o ActionID antigo por alguns frames.
		// Assists reais entram correndo ou têm velocidade de inicialização.
		float velX = GetVelocityX();
		if (fabs(velX) < 0.01f) {
			// Verifica também Y para garantir que não é um assist vertical puro (raro começar com 0 total)
			// Se estiver totalmente estático, assumimos fantasma.
			return false; 
		}

		// 3. Position Check: Filtro de Coordenadas (Culling)
		// Assists nascem longe, mas o banco fica MUITO longe (Y > 1000)
		if (fabs(posX) > 2000.0f || fabs(posY) > 1000.0f)
		{
			return false;
		}

		return true; // É um Assist Ativo
	}

	return false;
}

uintptr_t Character::GetHitboxArrayPtr() const
{
	if (!charPtr) return 0;
	return Memory::Read<uintptr_t>(charPtr + CharacterOffsets::HITBOX_ARRAY);
}

uintptr_t Character::GetHurtboxArrayPtr() const
{
	if (!charPtr) return 0;
	return Memory::Read<uintptr_t>(charPtr + CharacterOffsets::HURTBOX_ARRAY);
}

uintptr_t Character::GetPointer() const
{
	return charPtr;
}
