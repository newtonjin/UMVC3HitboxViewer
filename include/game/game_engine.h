#pragma once

#include <cstdint>
#include <vector>
#include "character.h"

namespace GamePointers
{
	const uintptr_t PTR_S_CHARACTER = 0x140D44A70;
	const uintptr_t PTR_CAMERA_STATIC = 0x140E17930;
	const uintptr_t PTR_BATTLE_SETTING = 0x140D50E58;
	const uintptr_t PTR_S_SHOTLIST = 0x140D47F98;
	
	// Pre-computed view and projection matrices from game
	const uintptr_t PTR_DRAW_VIEW = 0x140B20590;
	const uintptr_t PTR_DRAW_PROJECTION = 0x140BB9070;
	
	const uint32_t P1_TEAM_OFFSET = 0x58;
	const uint32_t P2_TEAM_OFFSET = 0x328;
	const uint32_t NEXT_NODE_OFFSET = 0x10;
	const uint32_t CHAR_DATA_OFFSET = 0x08;
}

class GameEngine
{
public:
	static GameEngine& Get();
	
	bool Initialize();
	void Shutdown();
	bool IsInMatch() const;
	bool IsGameMode5() const;
	const std::vector<Character>& GetAllCharacters() const;
	void UpdateCharacters();

private:
	GameEngine();
	~GameEngine();
	
	std::vector<Character> characters;
	uintptr_t p1TeamRoot;
	uintptr_t p2TeamRoot;
};
