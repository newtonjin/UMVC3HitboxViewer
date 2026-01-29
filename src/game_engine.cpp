#include "game/game_engine.h"
#include "game/character.h"
#include "core/memory.h"
#include "logger.h"
#include <cmath>

// GameEngine Singleton Implementation
GameEngine::GameEngine()
{
}

GameEngine::~GameEngine()
{
}

GameEngine& GameEngine::Get()
{
	static GameEngine instance;
	return instance;
}

bool GameEngine::Initialize()
{
	LOG_MSG("GameEngine: Initializing...");
	
	// Check if we can read the base pointers
	uintptr_t charBase = Memory::Read<uintptr_t>(GamePointers::PTR_S_CHARACTER);
	
	if (!charBase)
	{
		LOG_MSG("GameEngine: ERROR - Failed to read character base pointer!");
		return false;
	}
	
	LOG_FMT("GameEngine: Character base = 0x%llX", charBase);
	return true;
}

void GameEngine::Shutdown()
{
	LOG_MSG("GameEngine: Shutting down...");
	characters.clear();
}

bool GameEngine::IsInMatch() const
{
	return p1TeamRoot != 0 && p2TeamRoot != 0;
}

bool GameEngine::IsGameMode5() const
{
	// Game mode 5 is training/versus mode
	uintptr_t battleSetting = Memory::Read<uintptr_t>(GamePointers::PTR_BATTLE_SETTING);
	
	if (!battleSetting) return false;
	
	int gameMode = Memory::Read<int>(battleSetting + 0x34C);
	return gameMode == 5;
}

void GameEngine::UpdateCharacters()
{
	characters.clear();
	
	if (!IsGameMode5()) return;
	
	// Read character base
	uintptr_t charBase = Memory::Read<uintptr_t>(GamePointers::PTR_S_CHARACTER);
	if (!charBase) return;
	
	// Get team roots
	p1TeamRoot = Memory::Read<uintptr_t>(charBase + GamePointers::P1_TEAM_OFFSET);
	p2TeamRoot = Memory::Read<uintptr_t>(charBase + GamePointers::P2_TEAM_OFFSET);
	
	// Process P1 team (3 characters)
	{
		uintptr_t currentNode = p1TeamRoot;
		for (int i = 0; i < 3 && currentNode; ++i)
		{
			uintptr_t charData = Memory::Read<uintptr_t>(currentNode + GamePointers::CHAR_DATA_OFFSET);
			if (charData)
			{
				Character character(charData);
				if (character.ShouldRender())
				{
					characters.push_back(character);
				}
			}
			currentNode = Memory::Read<uintptr_t>(currentNode + GamePointers::NEXT_NODE_OFFSET);
		}
	}
	
	// Process P2 team (3 characters)
	{
		uintptr_t currentNode = p2TeamRoot;
		for (int i = 0; i < 3 && currentNode; ++i)
		{
			uintptr_t charData = Memory::Read<uintptr_t>(currentNode + GamePointers::CHAR_DATA_OFFSET);
			if (charData)
			{
				Character character(charData);
				if (character.ShouldRender())
				{
					characters.push_back(character);
				}
			}
			currentNode = Memory::Read<uintptr_t>(currentNode + GamePointers::NEXT_NODE_OFFSET);
		}
	}
}

const std::vector<Character>& GameEngine::GetAllCharacters() const
{
	return characters;
}
