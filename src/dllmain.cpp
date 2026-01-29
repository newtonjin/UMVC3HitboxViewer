#include <map>
#include <windows.h>
#include <d3d9.h>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <cmath>
#include "../utils/Trampoline.h"
#include "../utils/MemoryMgr.h"
#include "../utils/ScopedUnprotect.hpp"
#include "../utils/addr.h"
#include "../include/shared_overlay_data.h"
#include "../include/core/memory.h"
#include "../include/game/character.h"
#include "../include/game/game_engine.h"

using namespace Memory;

// Heartbeat maps: endereço do personagem -> último frame de animação e contador de congelamento
static std::map<uintptr_t, float> g_lastAnimFrame;
static std::map<uintptr_t, int> g_frozenCounter;
// Filtro extra: posição parada
static std::map<uintptr_t, std::pair<float, float>> g_lastPos;
static std::map<uintptr_t, int> g_stillCounter;

// File-based logging
static void LogToFile(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	FILE* f = fopen("UMVC3Overlay.log", "a");
	if (f)
	{
		fprintf(f, "[UMVC3] ");
		vfprintf(f, fmt, args);
		fprintf(f, "\n");
		fflush(f);
		fclose(f);
	}
	
	va_end(args);
}

#define LOG_MSG(fmt, ...) LogToFile(fmt, __VA_ARGS__)

// Global camera pointer - captured from camera constructor hook
static uintptr_t g_cameraPtr = 0;

// Camera constructor hook - captures the camera object pointer
// Hooked at 0x14001A490 (same as UMVC3TrainingToolsInt and umvc3cammod)
typedef int64_t(__fastcall* CameraConstructor_t)(int64_t camera, int64_t a2);
static CameraConstructor_t g_origCameraConstructor = nullptr;

static int64_t __fastcall CameraConstructorHook(int64_t camera, int64_t a2)
{
	// Capture the camera pointer
	g_cameraPtr = (uintptr_t)camera;
	// logs   LogToFile("[CAMERA HOOK] Captured camera pointer: 0x%llX", camera);
	
	// Call original - same logic as UMVC3TrainingToolsInt
	if (*(char*)(camera + 1416) == 1)
	{
		((void(__fastcall*)(int64_t))_addr(0x140015710))(camera + 0x230);
	}
	else
	{
		char unk[72] = {};
		((void(__fastcall*)(int64_t*))_addr(0x14000BAA0))((int64_t*)&unk);
		((void(__fastcall*)(int64_t*, float, float, float, float))_addr(0x14045F7D0))(
			(int64_t*)&unk, 
			*(float*)(camera + 0x44), 
			*(float*)(camera + 0x40), 
			*(float*)(camera + 0x4C) * 0.017453292f, 
			*(float*)(camera + 0x48));
		((void(__fastcall*)(int64_t, int64_t*))_addr(0x14000BA60))(a2, (int64_t*)&unk);
	}
	return a2;
}

// Global shared memory handle
static HANDLE g_sharedMemHandle = NULL;
static SharedOverlayData* g_sharedData = NULL;

// Initialize shared memory
static bool InitializeSharedMemory()
{
	if (g_sharedMemHandle) return true;

	g_sharedMemHandle = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(SharedOverlayData), SHARED_MEMORY_NAME);
	if (!g_sharedMemHandle)
	{
		LOG_MSG("InitializeSharedMemory: CreateFileMapping failed");
		return false;
	}

	g_sharedData = (SharedOverlayData*)MapViewOfFile(g_sharedMemHandle, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(SharedOverlayData));
	if (!g_sharedData)
	{
		LOG_MSG("InitializeSharedMemory: MapViewOfFile failed");
		CloseHandle(g_sharedMemHandle);
		g_sharedMemHandle = NULL;
		return false;
	}

	InitializeSharedOverlayData(g_sharedData);
	LOG_MSG("InitializeSharedMemory: Shared memory initialized successfully");
	return true;
}

// Cleanup shared memory
static void CleanupSharedMemory()
{
	if (g_sharedData)
	{
		UnmapViewOfFile(g_sharedData);
		g_sharedData = NULL;
	}
	if (g_sharedMemHandle)
	{
		CloseHandle(g_sharedMemHandle);
		g_sharedMemHandle = NULL;
	}
}

// ============= Runtime counters =============
static DWORD g_frameCounter = 0;
static DWORD g_presentCount = 0;

// Game render function hook
using Present_t = HRESULT(WINAPI*)(LPDIRECT3DDEVICE9, const RECT*, const RECT*, HWND, const RGNDATA*);

static Present_t g_origPresent = nullptr;
static volatile LONG g_hookInstalled = 0;

struct TeamScanResult
{
	bool hasLeader = false;
	bool leaderTagging = false;
	bool hasAny = false;
	float leaderX = 0.0f;
	float leaderY = 0.0f;
	float firstX = 0.0f;
	float firstY = 0.0f;
};

static void ScanTeam(uintptr_t teamRoot, TeamScanResult& out, std::vector<uintptr_t>& renderList)
{
	uintptr_t currentNode = teamRoot;
	for (int i = 0; i < 3 && currentNode; ++i)
	{
		uintptr_t charData = Memory::Read<uintptr_t>(currentNode + GamePointers::CHAR_DATA_OFFSET);
		if (charData)
		{
			Character character(charData);
			if (character.ShouldRender())
			{
				renderList.push_back(charData);
				if (!out.hasAny)
				{
					out.hasAny = true;
					out.firstX = character.GetPositionX();
					out.firstY = character.GetPositionY();
				}
			}

			uint8_t tagState = character.GetTagState();
			if (!out.hasLeader && (tagState == TagState::ACTIVE || tagState == TagState::TAG_IN || tagState == TagState::ACTIVE_ALT))
			{
				out.hasLeader = true;
				out.leaderX = character.GetPositionX();
				out.leaderY = character.GetPositionY();
				out.leaderTagging = (tagState == TagState::TAG_IN);
			}
		}
		currentNode = Memory::Read<uintptr_t>(currentNode + GamePointers::NEXT_NODE_OFFSET);
	}
}

static void UpdateCameraFromTeams(const TeamScanResult& p1, const TeamScanResult& p2)
{
	if (!g_sharedData) return;
	
	// Use the hooked camera pointer if available
	if (g_cameraPtr)
	{
		// Camera structure - testing different offsets
		// Position seems correct at 0x50, 0x54, 0x58
		// Target may be at 0x70 (not 0x7C as Camera.h suggests)
		
		float posx = Memory::Read<float>(g_cameraPtr + 0x50);
		float posy = Memory::Read<float>(g_cameraPtr + 0x54);
		float posz = Memory::Read<float>(g_cameraPtr + 0x58);
		float posx2 = Memory::Read<float>(g_cameraPtr + 0x70);  // Try 0x70 again
		float posy2 = Memory::Read<float>(g_cameraPtr + 0x74);
		float posz2 = Memory::Read<float>(g_cameraPtr + 0x78);
		float fov = Memory::Read<float>(g_cameraPtr + 0x4C);
		
		// Validate FOV (should be ~32 degrees typically)
		if (fov < 1.0f || fov > 180.0f) fov = 32.0f;
		
		// Convert FOV from degrees to radians
		float fovRad = fov * 3.14159265f / 180.0f;
		
		// Store camera POSITION (where the camera is)
		g_sharedData->cameraX = posx;
		g_sharedData->cameraY = posy;
		g_sharedData->cameraZ = posz;  // Camera Z distance for zoom calculation (~525)
		g_sharedData->cameraFov = fov; // FOV in degrees for fallback projection
		
		// Store TARGET position in viewMatrix[0-2]
		g_sharedData->viewMatrix[0] = posx2;  // Target X
		g_sharedData->viewMatrix[1] = posy2;  // Target Y (~135.5)
		g_sharedData->viewMatrix[2] = posz2;  // Target Z (~0)
		g_sharedData->viewMatrix[3] = posx;   // Cam pos X (backup)
		g_sharedData->viewMatrix[4] = posy;   // Cam pos Y (backup)
		g_sharedData->viewMatrix[5] = posz;   // Cam pos Z (backup)
		g_sharedData->viewMatrix[6] = fovRad; // FOV in RADIANS
		
		static int logCounter = 0;
		if ((logCounter++ % 1800) == 0)  // Log every ~30 seconds
		{
			LogToFile("[CAMERA] camPos(%.1f,%.1f,%.1f) target(%.1f,%.1f,%.1f) fov=%.1f deg (%.3f rad)", 
				posx, posy, posz, posx2, posy2, posz2, fov, fovRad);
		}
		return;
	}
	
	// Fallback: calculate from character positions if camera hook not captured yet
	float camX = 0.0f;
	float camY = 0.0f;
	int validChars = 0;
	
	if (p1.hasLeader)
	{
		camX += p1.leaderX;
		camY += p1.leaderY;
		validChars++;
	}
	else if (p1.hasAny)
	{
		camX += p1.firstX;
		camY += p1.firstY;
		validChars++;
	}
	
	if (p2.hasLeader)
	{
		camX += p2.leaderX;
		camY += p2.leaderY;
		validChars++;
	}
	else if (p2.hasAny)
	{
		camX += p2.firstX;
		camY += p2.firstY;
		validChars++;
	}
	
	if (validChars > 0)
	{
		camX /= validChars;
		camY /= validChars;
	}
	
	float camDist = 270.5f;
	g_sharedData->cameraX = camX;
	g_sharedData->cameraY = camY;
	g_sharedData->cameraZ = camDist;
	
	static int logCounter = 0;
	if ((logCounter++ % 1800) == 0)  // Log every ~30 seconds
	{
		LogToFile("[CAMERA FALLBACK] cam(%.1f,%.1f) dist=%.1f (from %d chars)", 
			camX, camY, camDist, validChars);
	}
}

static void ReadHitboxes(uintptr_t hitboxRoot, SharedCharacter& outChar)
{
	outChar.hitboxCount = 0;
	if (!hitboxRoot) return;

	uint32_t count = Memory::Read<uint32_t>(hitboxRoot + 0x20);
	if (count == 0 || count > 60) return;
	uintptr_t arr = Memory::Read<uintptr_t>(hitboxRoot + 0x30);
	if (!arr) return;

	uint32_t maxCount = count;
	if (maxCount > MAX_HITBOXES) maxCount = MAX_HITBOXES;
	for (uint32_t i = 0; i < maxCount; ++i)
	{
		uintptr_t entry = Memory::Read<uintptr_t>(arr + (i * 8));
		if (!entry) continue;
		float x = Memory::Read<float>(entry + 0x20);
		float y = Memory::Read<float>(entry + 0x24);
		float z = Memory::Read<float>(entry + 0x28);
		float radius = Memory::Read<float>(entry + 0x38);
		if (radius <= 0.0f) continue;

		SharedHitbox& hb = outChar.hitboxes[outChar.hitboxCount++];
		// Positions are already in world coordinates per TrainingTools reference
		hb.x = x;
		hb.y = y;
		hb.z = z;
		hb.radius = radius;
		hb.active = 1;

		if (outChar.hitboxCount >= MAX_HITBOXES) break;
	}
}

static void ReadHurtboxes(uintptr_t hurtboxRoot, SharedCharacter& outChar)
{
	outChar.hurtboxCount = 0;
	if (!hurtboxRoot) return;

	// TrainingTools: t = PlayerPtr + 0x4E10; tt = *(t + 0x30)
	uintptr_t curr = Memory::Read<uintptr_t>(hurtboxRoot + 0x30);
	if (!curr) return;

	for (uint32_t i = 0; i < MAX_HURTBOXES; ++i)
	{
		uintptr_t val = Memory::Read<uintptr_t>(curr);
		if (!val) break;
		
		uintptr_t data = Memory::Read<uintptr_t>(val + 0x10);
		
		float x, y, z, radius;
		
		if (data) {
			// Primary source: PointerToMoreData + offsets
			x = Memory::Read<float>(data + 0x20);
			y = Memory::Read<float>(data + 0x24);
			z = Memory::Read<float>(data + 0x28);
			radius = Memory::Read<float>(data + 0x2C);
		} else {
			// Fallback: read directly from val (SecondaryX/Y/Z in TrainingTools)
			x = Memory::Read<float>(val + 0x20);
			y = Memory::Read<float>(val + 0x24);
			z = Memory::Read<float>(val + 0x28);
			radius = Memory::Read<float>(val + 0x2C);
		}
		
		if (radius <= 0.0f) {
			curr += 8;
			continue;
		}

		SharedHurtbox& hb = outChar.hurtboxes[outChar.hurtboxCount++];
		hb.x = x;
		hb.y = y;
		hb.z = z;
		hb.radius = radius;
		hb.active = 1;

		if (outChar.hurtboxCount >= MAX_HURTBOXES) break;
		curr += 8;
	}
}

static void ReadProjectiles(SharedOverlayData& data)
{
	data.projectileCount = 0;
	
	try
	{
		uintptr_t shotList = GamePointers::PTR_S_SHOTLIST;
		uintptr_t curr = Memory::Read<uintptr_t>(shotList + 0x8);
		
		for (uint32_t i = 0; i < MAX_PROJECTILES && curr != 0 && curr != shotList; ++i)
		{
			// Shot structure is at curr - 0x1450
			uintptr_t shot = curr - 0x1450;
			float sx = Memory::Read<float>(shot + 0x50);
			float sy = Memory::Read<float>(shot + 0x54);
			
			// Only add if position is valid
			if (std::abs(sx) > 0.1f)
			{
				SharedProjectile& proj = data.projectiles[data.projectileCount++];
				proj.x = sx;
				proj.y = sy;
				proj.z = 0.0f;
				proj.active = 1;
			}
			
			curr = Memory::Read<uintptr_t>(curr + 0x8);
		}
	}
	catch (...)
	{
		// If projectile reading fails, just continue without them
		data.projectileCount = 0;
	}
}

static void UpdateGameState(SharedOverlayData& data)
{
	data.gameMode = 0;
	data.inMatch = 0;
	data.gameState = GameState_Unknown;

	uint64_t battleSetting = Memory::Read<uint64_t>(_addr(0x140d50e58));
	if (battleSetting)
	{
		int mode = Memory::Read<int>(battleSetting + 0x34C);
		data.gameMode = static_cast<uint32_t>(mode);
		if (mode == 5)
		{
			data.inMatch = 1;
			data.gameState = GameState_InMatch;
		}
		else
		{
			data.gameState = GameState_CharacterSelect;
		}
	}
	else
	{
		data.gameState = GameState_MainMenu;
	}
}

// Read game data and populate shared memory
static void UpdateSharedMemory()
{
	if (!g_sharedData)
		return;

	try
	{
		UpdateGameState(*g_sharedData);

		if (!g_sharedData->inMatch)
		{

			// Limpeza rigorosa: zera todos os campos do array de personagens
			g_sharedData->characterCount = 0;
			for (int i = 0; i < MAX_CHARACTERS; ++i) {
				SharedCharacter& c = g_sharedData->characters[i];
				memset(&c, 0, sizeof(SharedCharacter));
			}
			return;
		}

		std::vector<uintptr_t> renderList;
		TeamScanResult p1Team;
		TeamScanResult p2Team;

		uintptr_t charBase = Memory::Read<uintptr_t>(GamePointers::PTR_S_CHARACTER);
		if (!charBase)
		{
			g_sharedData->characterCount = 0;
			memset(g_sharedData->characters, 0, sizeof(g_sharedData->characters));
			return;
		}

		uintptr_t p1Root = Memory::Read<uintptr_t>(charBase + GamePointers::P1_TEAM_OFFSET);
		uintptr_t p2Root = Memory::Read<uintptr_t>(charBase + GamePointers::P2_TEAM_OFFSET);

		ScanTeam(p1Root, p1Team, renderList);
		ScanTeam(p2Root, p2Team, renderList);

		UpdateCameraFromTeams(p1Team, p2Team);


		// Limpeza rigorosa: zera todos os campos do array de personagens
		for (int i = 0; i < MAX_CHARACTERS; ++i) {
			SharedCharacter& c = g_sharedData->characters[i];
			memset(&c, 0, sizeof(SharedCharacter));
		}
		int outIdx = 0;
		float camX = g_sharedData->cameraX;
		float camY = g_sharedData->cameraY;
		const float CAM_MARGIN_X = 1500.0f; // ajuste conforme necessário
		const float CAM_MARGIN_Y = 800.0f;
		for (size_t i = 0; i < renderList.size() && outIdx < MAX_CHARACTERS; ++i) {
			uintptr_t charPtr = renderList[i];
			Character character(charPtr);
			uint8_t tagState = character.GetTagState();
			if (tagState == TagState::TAG_OUT || !character.ShouldRender()) {
				// Nunca renderizar nada para TAG_OUT ou ShouldRender==false
				continue;
			}
			// --- Heartbeat + posição parada para assists inativos ---
			bool assistOk = true;
			if (tagState == TagState::INACTIVE && character.GetActionID() != 0) {
				float animFrame = Memory::Read<float>(charPtr + 0x18);
				float posX = Memory::Read<float>(charPtr + 0x50);
				float posY = Memory::Read<float>(charPtr + 0x54);
				// Heartbeat
				if (g_lastAnimFrame.find(charPtr) == g_lastAnimFrame.end()) {
					g_lastAnimFrame[charPtr] = -1.0f;
					g_frozenCounter[charPtr] = 0;
				}
				if (fabs(animFrame - g_lastAnimFrame[charPtr]) < 0.001f) {
					g_frozenCounter[charPtr]++;
				} else {
					g_frozenCounter[charPtr] = 0;
				}
				g_lastAnimFrame[charPtr] = animFrame;
				bool isFrozen = (g_frozenCounter[charPtr] > 10);
				// Posição parada
				if (g_lastPos.find(charPtr) == g_lastPos.end()) {
					g_lastPos[charPtr] = {posX, posY};
					g_stillCounter[charPtr] = 0;
				}
				if (fabs(posX - g_lastPos[charPtr].first) < 0.01f && fabs(posY - g_lastPos[charPtr].second) < 0.01f) {
					g_stillCounter[charPtr]++;
				} else {
					g_stillCounter[charPtr] = 0;
				}
				g_lastPos[charPtr] = {posX, posY};
				bool isStill = (g_stillCounter[charPtr] > 15);
				// Só bloqueia se congelado E parado
				if (isFrozen && isStill) assistOk = false;
			}
			if (!assistOk) continue;

			SharedCharacter& outChar = g_sharedData->characters[outIdx++];
			float posX = character.GetPositionX();
			float posY = character.GetPositionY();
			float posZ = character.GetPositionZ();
			outChar.posX = posX;
			outChar.posY = posY;
			outChar.posZ = posZ;
			outChar.altPosX = Memory::Read<float>(charPtr + 0x50);
			outChar.altPosY = Memory::Read<float>(charPtr + 0x54);

			bool isPointChar = (tagState == TagState::ACTIVE || tagState == TagState::ACTIVE_ALT || tagState == TagState::TAG_IN);
			bool isAssist = (tagState == TagState::INACTIVE && character.GetActionID() != 0);
			int actionId = character.GetActionID();

			// Filtro relativo à câmera
			float dx = posX - camX;
			float dy = posY - camY;
			bool nearCamera = (std::abs(dx) < CAM_MARGIN_X && std::abs(dy) < CAM_MARGIN_Y);

			outChar.isActive = (isPointChar || (isAssist && actionId != 0)) ? 1 : 0;
			if (isPointChar) {
				outChar.isOnScreen = 1;
			} else if (isAssist && actionId != 0 && nearCamera) {
				outChar.isOnScreen = 1;
			} else {
				outChar.isOnScreen = 0;
			}
			outChar.charIndex = static_cast<uint32_t>(i);

			// Só preenche hitboxes/hurtboxes se não for TAG_OUT
			if (outChar.isActive && outChar.isOnScreen) {
				uintptr_t hitboxRoot = character.GetHitboxArrayPtr();
				uintptr_t hurtboxRoot = character.GetHurtboxArrayPtr();
				ReadHitboxes(hitboxRoot, outChar);
				ReadHurtboxes(hurtboxRoot, outChar);
			} else {
				outChar.hitboxCount = 0;
				outChar.hurtboxCount = 0;
			}
		}
		g_sharedData->characterCount = outIdx;
		// Limpa todos os slots não utilizados para evitar bonecos fantasmas
		for (int i = outIdx; i < MAX_CHARACTERS; ++i) {
			SharedCharacter& c = g_sharedData->characters[i];
			memset(&c, 0, sizeof(SharedCharacter));
		}
		
		// Read projectiles
		ReadProjectiles(*g_sharedData);
		
		if ((g_sharedData->frameNumber % 1800) == 0)  // Log every ~30 seconds
		{
			LOG_MSG("UpdateSharedMemory: Frame %llu, Char count: %d, Projectiles: %d", g_sharedData->frameNumber, g_sharedData->characterCount, g_sharedData->projectileCount);
		}
	}
	catch (...)
	{
		LOG_MSG("UpdateSharedMemory: Exception occurred");
	}
}

// Present hook callback - updates shared memory each frame
HRESULT WINAPI PresentHook(LPDIRECT3DDEVICE9 pDevice, const RECT* pSourceRect, const RECT* pDestRect, HWND hDestWindowOverride, const RGNDATA* pDirtyRegion)
{
	g_frameCounter++;
	g_presentCount++;

	// Only update shared memory if it's initialized
	if (g_sharedData)
	{
		// Camera data is valid if we have captured the camera pointer
		g_sharedData->matrixValid = (g_cameraPtr != 0) ? 1 : 0;
		
		// Update frame counter
		g_sharedData->frameNumber++;
		
		// Use fixed game resolution (UMVC3 default)
		// D3D9 calls can fail with UMVC3's custom wrapper
		if (g_sharedData->gameWidth == 0)
		{
			g_sharedData->gameWidth = 1600;
			g_sharedData->gameHeight = 900;
			LogToFile("[RESOLUTION] Using default: 1600x900");
		}

		// Read game memory and update shared memory
		UpdateSharedMemory();
		
		if ((g_sharedData->frameNumber % 1800) == 0)  // Log every ~30 seconds
		{
			LOG_MSG("[PERIODIC] Frame %llu, Camera: 0x%llX, MatrixValid: %d", 
				g_sharedData->frameNumber, g_cameraPtr, g_sharedData->matrixValid);
		}
	}

	// Call original Present
	return g_origPresent(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

// Initialization thread - performs heavy work outside of DllMain
static DWORD WINAPI InitThread(LPVOID lpParameter)
{
	if (InterlockedCompareExchange(&g_hookInstalled, 1, 0) != 0)
	{
		LOG_MSG("InitThread: Hook already installed, skipping");
		return 0;
	}

	LOG_MSG("InitThread: Starting...");
	
	// Small delay to allow the host process to stabilize
	Sleep(2000);
	LOG_MSG("InitThread: Delay complete, proceeding...");

	// Initialize shared memory
	if (!InitializeSharedMemory())
	{
		LOG_MSG("InitThread: Failed to initialize shared memory");
		InterlockedExchange(&g_hookInstalled, 0);
		return 1;
	}

	LOG_MSG("InitThread: Creating trampoline for game hook (address 0x140289c5a)...");

	Trampoline* tramp = nullptr;
	try
	{
		tramp = Trampoline::MakeTrampoline((void*)_addr(0x140289c5a));
		LOG_MSG("InitThread: MakeTrampoline succeeded, tramp=%p", tramp);
	}
	catch (...) {
		LOG_MSG("InitThread: Exception while creating trampoline");
		InterlockedExchange(&g_hookInstalled, 0);
		return 1;
	}

	if (!tramp)
	{
		LOG_MSG("InitThread: Failed to create trampoline (returned null)");
		InterlockedExchange(&g_hookInstalled, 0);
		return 1;
	}

	LOG_MSG("InitThread: Trampoline created successfully at %p", tramp);

	// Unprotect target region while patching (RAII)
	LOG_MSG("InitThread: Unprotecting .text section...");
	auto unprotect = ScopedUnprotect::UnprotectSectionOrFullModule(GetModuleHandle(nullptr), ".text");
	LOG_MSG("InitThread: .text section unprotected");

	LOG_MSG("InitThread: Attempting VP::InterceptCall into 0x140289c5a...");
	try
	{
		// Read original call target and install interceptor (VP-protected)
		Memory::VP::InterceptCall(_addr(0x140289c5a), g_origPresent, tramp->Jump(PresentHook));
		LOG_MSG("InitThread: VP::InterceptCall succeeded!");
		LOG_MSG("InitThread: Original Present saved: %p", g_origPresent);
	}
	catch (...) {
		LOG_MSG("InitThread: Exception during VP::InterceptCall");
		InterlockedExchange(&g_hookInstalled, 0);
		return 1;
	}

	// Hook the camera constructor to capture camera pointer
	LOG_MSG("InitThread: Installing camera constructor hook at 0x14001A490...");
	try
	{
		Trampoline* camTramp = Trampoline::MakeTrampoline((void*)_addr(0x14001A490));
		if (camTramp)
		{
			Memory::VP::InjectHook(_addr(0x14001A490), camTramp->Jump(CameraConstructorHook), Memory::VP::HookType::Jump);
			LOG_MSG("InitThread: Camera hook installed successfully!");
		}
		else
		{
			LOG_MSG("InitThread: Failed to create camera trampoline");
		}
	}
	catch (...)
	{
		LOG_MSG("InitThread: Exception during camera hook installation");
	}

	LOG_MSG("InitThread: Hook installation complete!");
	return 0;
}

// DLL Entry Point
extern "C" BOOL WINAPI DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		DisableThreadLibraryCalls(hModule);

		LOG_MSG("=== UMVC3 Overlay DLL Loaded ===");

		// Spawn a thread for initialization to avoid doing heavy work inside DllMain
		HANDLE h = CreateThread(NULL, 0, InitThread, NULL, 0, NULL);
		if (h)
		{
			CloseHandle(h);
		}
		else
		{
			LOG_MSG("Failed to create init thread");
		}
	}
	else if (dwReason == DLL_PROCESS_DETACH)
	{
		LOG_MSG("=== UMVC3 Overlay DLL Unloading ===");
		CleanupSharedMemory();
	}

	return TRUE;
}
