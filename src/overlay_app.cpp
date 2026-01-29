// UMVC3 Overlay App - External overlay application
// Reads shared memory and renders hitbox/hurtbox visualization
#include <windows.h>
#include <gdiplus.h>
#include <commctrl.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")
#include <cmath>
#include <cstdio>
#include "include/shared_overlay_data.h"

// Include GLM for proper matrix projection
#include "../glm/glm/glm.hpp"
#include "../glm/glm/gtc/matrix_transform.hpp"
#include "../glm/glm/gtc/type_ptr.hpp"

// ============================================
// Status GUI Window - Dark Mode Style
// ============================================
static HWND g_statusWindow = NULL;
static HWND g_statusLabel = NULL;
static HWND g_authorLabel = NULL;
static HWND g_progressBar = NULL;
static HFONT g_statusFont = NULL;
static HFONT g_titleFont = NULL;
static HFONT g_authorFont = NULL;
static HBRUSH g_darkBrush = NULL;

// Dark mode colors
static const COLORREF COLOR_DARK_BG = RGB(25, 25, 30);
static const COLORREF COLOR_GREEN_TEXT = RGB(0, 255, 128);
static const COLORREF COLOR_GREEN_DIM = RGB(0, 180, 90);
static const COLORREF COLOR_ACCENT = RGB(0, 255, 180);

// Custom window procedure for dark mode
static LRESULT CALLBACK StatusWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CTLCOLORSTATIC:
		{
			HDC hdcStatic = (HDC)wParam;
			SetTextColor(hdcStatic, COLOR_GREEN_TEXT);
			SetBkColor(hdcStatic, COLOR_DARK_BG);
			SetBkMode(hdcStatic, TRANSPARENT);
			return (LRESULT)g_darkBrush;
		}
	case WM_ERASEBKGND:
		{
			HDC hdc = (HDC)wParam;
			RECT rect;
			GetClientRect(hWnd, &rect);
			FillRect(hdc, &rect, g_darkBrush);
			
			// Draw border glow effect
			HPEN greenPen = CreatePen(PS_SOLID, 2, COLOR_GREEN_DIM);
			HPEN oldPen = (HPEN)SelectObject(hdc, greenPen);
			HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
			Rectangle(hdc, 1, 1, rect.right - 1, rect.bottom - 1);
			SelectObject(hdc, oldPen);
			SelectObject(hdc, oldBrush);
			DeleteObject(greenPen);
			return TRUE;
		}
	case WM_NCHITTEST:
		{
			// Allow dragging the window from anywhere
			LRESULT hit = DefWindowProcA(hWnd, uMsg, wParam, lParam);
			if (hit == HTCLIENT) return HTCAPTION;
			return hit;
		}
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return 0;
		}
	}
	return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

static void CreateStatusWindow(HINSTANCE hInstance)
{
	// Create dark background brush
	g_darkBrush = CreateSolidBrush(COLOR_DARK_BG);
	
	// Register window class with custom proc
	WNDCLASSA wc = {0};
	wc.lpfnWndProc = StatusWndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = g_darkBrush;
	wc.lpszClassName = "UMVC3StatusClass";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	RegisterClassA(&wc);
	
	// Create window centered on screen - borderless modern style
	int width = 420;
	int height = 200;
	int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
	int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	
	g_statusWindow = CreateWindowExA(
		WS_EX_TOPMOST | WS_EX_LAYERED,
		"UMVC3StatusClass",
		"UMVC3 Hitbox Viewer",
		WS_POPUP,  // Borderless
		x, y, width, height,
		NULL, NULL, hInstance, NULL
	);
	
	// Set window transparency (230/255 = ~90% opaque)
	SetLayeredWindowAttributes(g_statusWindow, 0, 230, LWA_ALPHA);
	
	// Create fonts
	g_titleFont = CreateFontA(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
	
	g_statusFont = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
	
	g_authorFont = CreateFontA(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
	
	// Title label
	HWND titleLabel = CreateWindowExA(
		0, "STATIC", "UMVC3 HITBOX VIEWER",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		10, 15, width - 20, 30,
		g_statusWindow, NULL, hInstance, NULL
	);
	SendMessageA(titleLabel, WM_SETFONT, (WPARAM)g_titleFont, TRUE);
	
	// Author label - emphasized
	g_authorLabel = CreateWindowExA(
		0, "STATIC", "by N3",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		10, 45, width - 20, 25,
		g_statusWindow, NULL, hInstance, NULL
	);
	SendMessageA(g_authorLabel, WM_SETFONT, (WPARAM)g_authorFont, TRUE);
	
	// Status label
	g_statusLabel = CreateWindowExA(
		0, "STATIC", "Initializing...",
		WS_CHILD | WS_VISIBLE | SS_CENTER,
		10, 85, width - 20, 40,
		g_statusWindow, NULL, hInstance, NULL
	);
	SendMessageA(g_statusLabel, WM_SETFONT, (WPARAM)g_statusFont, TRUE);
	
	// Initialize common controls for progress bar
	INITCOMMONCONTROLSEX icex = { sizeof(INITCOMMONCONTROLSEX), ICC_PROGRESS_CLASS };
	InitCommonControlsEx(&icex);
	
	// Create progress bar
	g_progressBar = CreateWindowExA(
		0, PROGRESS_CLASSA, NULL,
		WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
		30, 140, width - 60, 20,
		g_statusWindow, NULL, hInstance, NULL
	);
	SendMessageA(g_progressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
	SendMessageA(g_progressBar, PBM_SETSTEP, 1, 0);
	
	// Set progress bar colors (green theme)
	SendMessageA(g_progressBar, PBM_SETBARCOLOR, 0, (LPARAM)COLOR_GREEN_TEXT);
	SendMessageA(g_progressBar, PBM_SETBKCOLOR, 0, (LPARAM)RGB(40, 40, 45));
	
	ShowWindow(g_statusWindow, SW_SHOW);
	UpdateWindow(g_statusWindow);
}

static void UpdateStatus(const char* status, int progress)
{
	if (g_statusLabel)
	{
		SetWindowTextA(g_statusLabel, status);
	}
	if (g_progressBar)
	{
		SendMessageA(g_progressBar, PBM_SETPOS, progress, 0);
	}
	
	// Process messages to keep window responsive
	MSG msg;
	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
	}
}

static void CloseStatusWindow()
{
	if (g_statusWindow)
	{
		DestroyWindow(g_statusWindow);
		g_statusWindow = NULL;
	}
	if (g_statusFont)
	{
		DeleteObject(g_statusFont);
		g_statusFont = NULL;
	}
	if (g_titleFont)
	{
		DeleteObject(g_titleFont);
		g_titleFont = NULL;
	}
	if (g_authorFont)
	{
		DeleteObject(g_authorFont);
		g_authorFont = NULL;
	}
	if (g_darkBrush)
	{
		DeleteObject(g_darkBrush);
		g_darkBrush = NULL;
	}
}

// ============================================
// Logging helper
// ============================================
static void OverlayLog(const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	
	FILE* f = fopen("UMVC3Overlay.log", "a");
	if (f)
	{
		fprintf(f, "[Overlay] ");
		vfprintf(f, fmt, args);
		fprintf(f, "\n");
		fflush(f);
		fclose(f);
	}
	
	va_end(args);
}

// Global variables
static HWND g_overlayWindow = NULL;
static HWND g_gameWindow = NULL;
static HANDLE g_sharedMemHandle = NULL;
static SharedOverlayData* g_sharedData = NULL;
static ULONG_PTR g_gdiplusToken = 0;
static bool g_running = true;
static UINT64 g_lastFrame = 0;
static int g_windowWidth = 1280;
static int g_windowHeight = 720;
static bool g_overlayEnabled = true;
static bool g_debugCircle = false;
static const bool g_useMatrices = true; // Enable matrix-based projection
// Screen-space alignment tweaks
static const float kScreenOffsetX = 0.0f;
static const float kScreenOffsetY = 0.0f;
static const float kZoomScale = 1.0f;
static const float kRadiusScale = 0.8f;   // Smaller circles (was 1.0)
static const float kRadiusBias = 2.0f;    // Minimum visible size
static const float kZoomDamping = 0.5f;   // Damping for zoom acceleration (0.0-1.0, lower = less dynamic)
static const float kHitboxYOffset = 30.0f; // Offset to raise hitboxes (world units)

// Dynamic Z offset for hurtcircle distance (controlled via numpad)
static float g_hurtcircleZOffset = 0.0f;  // Adjustable via Numpad +/-
static const float kZOffsetStep = 10.0f;  // How much to change per keypress

// Find UMVC3 game window
static HWND FindGameWindow()
{
	OverlayLog("FindGameWindow: Searching for UMVC3 window...");
	HWND hwnd = FindWindowA(NULL, "ULTIMATE MARVEL VS. CAPCOM 3");
	if (hwnd)
	{
		OverlayLog("FindGameWindow: SUCCESS - Found UMVC3 window at 0x%p", hwnd);
		return hwnd;
	}
	OverlayLog("FindGameWindow: FAILED - UMVC3 window not found");
	return NULL;
}

// Inject DLL into UMVC3 process
static bool InjectDLL(HWND gameWindow)
{
	OverlayLog("InjectDLL: Starting injection into UMVC3...");
	
	if (!gameWindow)
	{
		OverlayLog("InjectDLL: FAILED - game window not found");
		return false;
	}

	// Get process ID from window
	DWORD processId = 0;
	GetWindowThreadProcessId(gameWindow, &processId);
	
	if (!processId)
	{
		OverlayLog("InjectDLL: FAILED - could not get process ID");
		return false;
	}

	OverlayLog("InjectDLL: Found UMVC3 process ID: %lu", processId);

	// Open process
	HANDLE hProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, processId);
	if (!hProcess)
	{
		OverlayLog("InjectDLL: FAILED - could not open process (error: %lu)", GetLastError());
		return false;
	}

	OverlayLog("InjectDLL: Opened process handle");

	// Get full path to DLL (same directory as the EXE)
	char exePath[MAX_PATH];
	GetModuleFileNameA(NULL, exePath, MAX_PATH);
	
	// Remove filename to get directory
	char* lastSlash = strrchr(exePath, '\\');
	if (lastSlash) *lastSlash = '\0';
	
	char dllPath[MAX_PATH];
	sprintf_s(dllPath, MAX_PATH, "%s\\UMVC3Overlay.dll", exePath);
	
	OverlayLog("InjectDLL: DLL path: %s", dllPath);
	
	// Check if DLL exists
	if (GetFileAttributesA(dllPath) == INVALID_FILE_ATTRIBUTES)
	{
		OverlayLog("InjectDLL: FAILED - DLL file not found at: %s", dllPath);
		CloseHandle(hProcess);
		return false;
	}

	// Allocate memory in target process for DLL path
	LPVOID remoteDllPath = VirtualAllocEx(hProcess, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
	if (!remoteDllPath)
	{
		OverlayLog("InjectDLL: FAILED - VirtualAllocEx failed (error: %lu)", GetLastError());
		CloseHandle(hProcess);
		return false;
	}

	OverlayLog("InjectDLL: Allocated memory in remote process");

	// Write DLL path to remote memory
	if (!WriteProcessMemory(hProcess, remoteDllPath, dllPath, strlen(dllPath) + 1, NULL))
	{
		OverlayLog("InjectDLL: FAILED - WriteProcessMemory failed (error: %lu)", GetLastError());
		VirtualFreeEx(hProcess, remoteDllPath, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return false;
	}

	OverlayLog("InjectDLL: Wrote DLL path to remote memory");

	// Get address of LoadLibraryA
	HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
	LPVOID loadLibraryAddr = (LPVOID)GetProcAddress(kernel32, "LoadLibraryA");
	
	if (!loadLibraryAddr)
	{
		OverlayLog("InjectDLL: FAILED - could not get LoadLibraryA address");
		VirtualFreeEx(hProcess, remoteDllPath, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return false;
	}

	OverlayLog("InjectDLL: Got LoadLibraryA address");

	// Create remote thread to call LoadLibraryA
	HANDLE hRemoteThread = CreateRemoteThread(hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibraryAddr, remoteDllPath, 0, NULL);
	if (!hRemoteThread)
	{
		OverlayLog("InjectDLL: FAILED - CreateRemoteThread failed (error: %lu)", GetLastError());
		VirtualFreeEx(hProcess, remoteDllPath, 0, MEM_RELEASE);
		CloseHandle(hProcess);
		return false;
	}

	OverlayLog("InjectDLL: Created remote thread, waiting for completion...");

	// Wait for thread to complete
	WaitForSingleObject(hRemoteThread, INFINITE);
	
	// Cleanup
	CloseHandle(hRemoteThread);
	VirtualFreeEx(hProcess, remoteDllPath, 0, MEM_RELEASE);
	CloseHandle(hProcess);

	OverlayLog("InjectDLL: SUCCESS - DLL injected successfully");
	return true;
}

// Position overlay over game window
static bool PositionOverlayOnGame()
{
	if (!g_overlayWindow)
		return false;

	g_gameWindow = FindWindowA(NULL, "ULTIMATE MARVEL VS. CAPCOM 3");
	if (!g_gameWindow)
		return false;

	RECT rect = {};
	if (!GetWindowRect(g_gameWindow, &rect))
		return false;

	int width = rect.right - rect.left;
	int height = rect.bottom - rect.top;
	
	// Only update if size changed
	if (width != g_windowWidth || height != g_windowHeight)
	{
		g_windowWidth = width;
		g_windowHeight = height;
	}

	SetWindowPos(g_overlayWindow, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_SHOWWINDOW);
	return true;
}

// Function to open/map shared memory with retries
static bool OpenSharedMemory()
{
	const int maxRetries = 10;
	const int retryDelayMs = 500;
	
	for (int attempt = 1; attempt <= maxRetries; attempt++)
	{
		OverlayLog("OpenSharedMemory: Attempt %d/%d - Trying to open shared memory...", attempt, maxRetries);
		
		g_sharedMemHandle = OpenFileMappingW(FILE_MAP_READ, FALSE, SHARED_MEMORY_NAME);
		if (g_sharedMemHandle)
		{
			OverlayLog("OpenSharedMemory: Handle opened, mapping view...");
			g_sharedData = (SharedOverlayData*)MapViewOfFile(g_sharedMemHandle, FILE_MAP_READ, 0, 0, sizeof(SharedOverlayData));
			if (g_sharedData)
			{
				OverlayLog("OpenSharedMemory: SUCCESS - Shared memory opened and mapped");
				return true;
			}
			else
			{
				OverlayLog("OpenSharedMemory: FAILED - MapViewOfFile failed (error: %lu)", GetLastError());
				CloseHandle(g_sharedMemHandle);
				g_sharedMemHandle = NULL;
			}
		}
		else
		{
			OverlayLog("OpenSharedMemory: Shared memory not found yet (error: %lu)", GetLastError());
		}
		
		if (attempt < maxRetries)
		{
			OverlayLog("OpenSharedMemory: Retrying in %d ms...", retryDelayMs);
			Sleep(retryDelayMs);
		}
	}
	
	OverlayLog("OpenSharedMemory: FAILED - Could not open shared memory after %d attempts", maxRetries);
	return false;
}

// Function to close shared memory
static void CloseSharedMemory()
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

// Initialize Direct3D 9
// GDI+ helpers for drawing into a layered window
static void GdiPlusBegin()
{
	if (g_gdiplusToken == 0)
	{
		Gdiplus::GdiplusStartupInput gdiplusStartupInput;
		if (Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL) == Gdiplus::Ok)
		{
			OverlayLog("GDI+: Started");
		}
		else
		{
			OverlayLog("GDI+: FAILED to start");
		}
	}
}

static void GdiPlusShutdown()
{
	if (g_gdiplusToken)
	{
		Gdiplus::GdiplusShutdown(g_gdiplusToken);
		g_gdiplusToken = 0;
		OverlayLog("GDI+: Shutdown");
	}
}

// ==================================================================================
// Projeção usando a fórmula EXATA do TrainingToolsInt
// lookAt(camPos.xy + target.z, target.xy + camPos.z, up)
// ==================================================================================
static void ProjectToScreen(float gameX, float gameY, float gameZ, float& screenX, float& screenY, float& outZoom, int windowWidth, int windowHeight)
{
	if (!g_sharedData)
	{
		screenX = windowWidth / 2.0f;
		screenY = windowHeight / 2.0f;
		outZoom = 1.0f;
		return;
	}

	// Get game resolution
	float gameWidth = (float)g_sharedData->gameWidth;
	float gameHeight = (float)g_sharedData->gameHeight;
	if (gameWidth < 1.0f) gameWidth = 1600.0f;
	if (gameHeight < 1.0f) gameHeight = 900.0f;

	// Camera data from hook (offsets verified from TrainingToolsInt Camera.h)
	// Camera Position: +0x50, +0x54, +0x58 (X, Y, Z)
	// Target Position: +0x70, +0x74, +0x78 (X, Y, Z)  
	// FOV: +0x4C (degrees)
	float camPosX = g_sharedData->cameraX;       // +0x50
	float camPosY = g_sharedData->cameraY;       // +0x54
	float camPosZ = g_sharedData->cameraZ;       // +0x58 (~525)
	float targetX = g_sharedData->viewMatrix[0]; // +0x70
	float targetY = g_sharedData->viewMatrix[1]; // +0x74
	float targetZ = g_sharedData->viewMatrix[2]; // +0x78 (~0)
	float fovDeg = g_sharedData->cameraFov;      // +0x4C (~32)
	
	// Validation
	if (camPosZ < 1.0f) camPosZ = 525.0f;
	if (fovDeg < 1.0f || fovDeg > 120.0f) fovDeg = 32.0f;
	float fovRad = fovDeg * 3.14159265f / 180.0f;

	// IMPORTANT: TrainingToolsInt sets _coord.z = posz (camera Z)
	// This means all objects are projected as if they're at the camera's Z plane
	float projZ = camPosZ;
	
	// TrainingToolsInt projection formula (Menu.cpp:4705):
	// lookAt(vec3(posx, posy, posz2), vec3(posx2, posy2, posz), up)
	// FROM: (camPosX, camPosY, targetZ)
	// TO:   (targetX, targetY, camPosZ)
	glm::vec3 cameraFrom(camPosX, camPosY, targetZ);
	glm::vec3 cameraTo(targetX, targetY, camPosZ);
	glm::vec3 upVector(0.0f, 1.0f, 0.0f);
	
	glm::mat4 viewMatrix = glm::lookAt(cameraFrom, cameraTo, upVector);
	
	// Projection with game's FOV and aspect ratio
	float aspectRatio = gameWidth / gameHeight;
	glm::mat4 projection = glm::perspective(fovRad, aspectRatio, 0.01f, 10000.0f);
	
	// Viewport (TrainingToolsInt uses fixed 1600x900)
	glm::vec4 viewport(0.0f, 0.0f, gameWidth, gameHeight);
	
	// World position - IMPORTANT: Z must be camPosZ as per TrainingToolsInt
	glm::vec3 worldPos(gameX, gameY, projZ);
	
	// Project to screen
	glm::vec3 screenPos = glm::project(worldPos, viewMatrix, projection, viewport);
	
	// Scale to actual window size
	float scaleX = (float)windowWidth / gameWidth;
	float scaleY = (float)windowHeight / gameHeight;
	
	// X axis: invert because camera looks along negative Z in this setup
	screenX = (gameWidth - screenPos.x) * scaleX;
	screenY = (gameHeight - screenPos.y) * scaleY;  // GLM Y is bottom-up, flip it
	
	// Calculate zoom for hitbox/hurtbox size scaling
	float visibleHeight = 2.0f * camPosZ * tanf(fovRad / 2.0f);
	outZoom = gameHeight / visibleHeight * scaleY;
	
	static int debugCounter = 0;
	if ((debugCounter++ % 3600) == 0) {  // Log every ~1 minute
		OverlayLog("ProjectGLM: world(%.0f,%.0f) camPos(%.0f,%.0f,%.0f) target(%.0f,%.0f,%.0f)",
				   gameX, gameY, camPosX, camPosY, camPosZ, targetX, targetY, targetZ);
		OverlayLog("  fov=%.1f screenPos(%.0f,%.0f) -> final(%.0f,%.0f) zoom=%.2f",
				   fovDeg, screenPos.x, screenPos.y, screenX, screenY, outZoom);
	}
}

static const char* GetGameStateLabel(uint32_t state)
{
	switch (state)
	{
	case GameState_MainMenu:
		return "Main Menu";
	case GameState_CharacterSelect:
		return "Menus";
	case GameState_InMatch:
		return "Training/Match";
	default:
		return "Unknown";
	}
}

// Window procedure
static LRESULT CALLBACK OverlayWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
		}
		return 0;
	}
	return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

// Create transparent overlay window
static bool CreateOverlayWindow()
{
	OverlayLog("CreateOverlayWindow: Registering window class...");
	WNDCLASSA wc = {};
	wc.lpfnWndProc = OverlayWndProc;
	wc.lpszClassName = "UMVC3OverlayClass";
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);

	if (!RegisterClassA(&wc))
	{
		OverlayLog("CreateOverlayWindow: FAILED - RegisterClass failed");
		MessageBoxA(NULL, "Failed to register window class.", "Overlay Error", MB_OK | MB_ICONERROR);
		return false;
	}

	OverlayLog("CreateOverlayWindow: Creating window (will position over game)...");
	// Create window but position will be set when we find the game window
	g_overlayWindow = CreateWindowExA(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
		"UMVC3OverlayClass",
		"UMVC3 Overlay",
		WS_POPUP,
		0, 0, 1280, 720,
		NULL, NULL, GetModuleHandle(NULL), NULL
	);

	if (!g_overlayWindow)
	{
		OverlayLog("CreateOverlayWindow: FAILED - CreateWindowEx failed");
		MessageBoxA(NULL, "Failed to create overlay window.", "Overlay Error", MB_OK | MB_ICONERROR);
		UnregisterClassA("UMVC3OverlayClass", GetModuleHandle(NULL));
		return false;
	}
	OverlayLog("CreateOverlayWindow: Configured layered window with per-pixel alpha");

	// Position overlay over the game window
	if (!PositionOverlayOnGame())
	{
		OverlayLog("CreateOverlayWindow: WARNING - Could not position overlay over game yet");
		// This might fail if game isn't open yet, will retry in render loop
	}

	OverlayLog("CreateOverlayWindow: SUCCESS - Window created");
	return true;
}

// Main render loop
static void RenderFrame()
{
	if (!g_sharedData)
		return;

	const UINT64 currentFrame = g_sharedData->frameNumber;
	bool shouldDrawOverlay = true;
	if (currentFrame < g_lastFrame)
	{
		// Training restart or game reset detected - silently clear
		shouldDrawOverlay = false;
	}

	// Reposition overlay periodically (in case game window moved)
	static int repositionCounter = 0;
	repositionCounter++;
	if (repositionCounter > 120)  // Every ~2 seconds at 60fps
	{
		repositionCounter = 0;
		PositionOverlayOnGame();  // Silent - don't log failures
	}

	// Always refresh the layered window so visuals don't persist
	// even if the shared frame number hasn't advanced yet.

	g_lastFrame = currentFrame;

	// Create an ARGB DIB section to draw per-pixel alpha content
	HDC hdcScreen = GetDC(NULL);
	HDC hdcMem = CreateCompatibleDC(hdcScreen);

	BITMAPINFO bi = {};
	bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bi.bmiHeader.biWidth = g_windowWidth;
	bi.bmiHeader.biHeight = -g_windowHeight; // top-down DIB
	bi.bmiHeader.biPlanes = 1;
	bi.bmiHeader.biBitCount = 32; // ARGB
	bi.bmiHeader.biCompression = BI_RGB;

	void* pvBits = nullptr;
	HBITMAP hbm = CreateDIBSection(hdcScreen, &bi, DIB_RGB_COLORS, &pvBits, NULL, 0);
	HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbm);

	// Clear to fully transparent
	memset(pvBits, 0, g_windowWidth * g_windowHeight * 4);

	// Use GDI+ to draw anti-aliased circles
	Gdiplus::Graphics gfx(hdcMem);
	gfx.SetSmoothingMode(Gdiplus::SmoothingMode::SmoothingModeHighQuality);

	// Draw game state text (top-left)
	const char* stateLabel = GetGameStateLabel(g_sharedData->gameState);
	char stateText[64] = {};
	snprintf(stateText, sizeof(stateText), "State: %s", stateLabel);
	wchar_t stateTextW[64] = {};
	MultiByteToWideChar(CP_UTF8, 0, stateText, -1, stateTextW, 64);
	Gdiplus::FontFamily fontFamily(L"Segoe UI");
	Gdiplus::Font font(&fontFamily, 16.0f, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 0, 255, 0));
	gfx.DrawString(stateTextW, -1, &font, Gdiplus::PointF(10.0f, 50.0f), &textBrush);

	// Optional debug circle
	if (g_debugCircle)
	{
		Gdiplus::SolidBrush brushDebug(Gdiplus::Color(160, 255, 0, 0));
		float cx = 50.0f;
		float cy = 50.0f;
		float r = 20.0f;
		gfx.FillEllipse(&brushDebug, Gdiplus::RectF(cx - r, cy - r, r * 2, r * 2));
	}

	// Draw character hitboxes and hurtboxes
	int hitboxCount = 0;
	int hurtboxCount = 0;

	Gdiplus::SolidBrush brushHit(Gdiplus::Color(150, 255, 0, 0));
	Gdiplus::SolidBrush brushHurt(Gdiplus::Color(100, 0, 255, 0));

	const bool inMatch = (g_sharedData->gameState == GameState_InMatch) || (g_sharedData->inMatch != 0);
	
	// Get camera position for distance-based filtering
	float camX = g_sharedData->cameraX;
	float camY = g_sharedData->cameraY;
	
	if (g_overlayEnabled && inMatch && shouldDrawOverlay)
	{
		for (int i = 0; i < (int)g_sharedData->characterCount && i < MAX_CHARACTERS; i++)
		{
			const SharedCharacter& ch = g_sharedData->characters[i];
			// Skip inactive or off-screen characters to prevent stale hitbox/hurtbox rendering
			if (!ch.isActive || !ch.isOnScreen) continue;
			
			// Filter by distance from camera (not absolute position)
			// This handles corners correctly - only filter if character is far from camera view
			float distFromCamX = fabs(ch.posX - camX);
			float distFromCamY = fabs(ch.posY - camY);
			if (distFromCamX > 500.0f || distFromCamY > 400.0f) continue;

			// Hitboxes (use provided count) - positions are WORLD coordinates, not offsets
			int hbc = (int)ch.hitboxCount;
			if (hbc < 0) hbc = 0; if (hbc > MAX_HITBOXES) hbc = MAX_HITBOXES;
			for (int j = 0; j < hbc; j++)
			{
				const SharedHitbox& hb = ch.hitboxes[j];
				if (!hb.active) continue;
				float screenX, screenY, zoom;
				// hb.x/y/z are already absolute world coordinates
				// Add Y offset to raise hitboxes to match attack visuals
				ProjectToScreen(hb.x + 32.0f, hb.y + kHitboxYOffset, hb.z, screenX, screenY, zoom, g_windowWidth, g_windowHeight);
				float rr = (hb.radius * zoom * kRadiusScale) + kRadiusBias;
				if (rr < 1.0f) rr = 1.0f; if (rr > 500.0f) rr = 500.0f;
				gfx.FillEllipse(&brushHit, Gdiplus::RectF(screenX - rr, screenY - rr, rr * 2, rr * 2));
				hitboxCount++;
			}

			// Hurtboxes (use provided count) - must add character position like hitboxes
			int huc = (int)ch.hurtboxCount;
			if (huc < 0) huc = 0; if (huc > MAX_HURTBOXES) huc = MAX_HURTBOXES;
		
			// Render hurtboxes - positions are WORLD coordinates, not offsets
			for (int j = 0; j < huc; j++)
			{
				const SharedHurtbox& hb = ch.hurtboxes[j];
				if (!hb.active) continue;
				float screenX, screenY, zoom;
				// hb.x/y/z are already absolute world coordinates
				ProjectToScreen(hb.x, hb.y, hb.z, screenX, screenY, zoom, g_windowWidth, g_windowHeight);
				float rr = (hb.radius * zoom * kRadiusScale) + kRadiusBias;
				if (rr < 1.0f) rr = 1.0f; if (rr > 500.0f) rr = 500.0f;
				
				// Log first hurtbox of first character for debugging
				if (i == 0 && j == 0) {
					static int hbLogCounter = 0;
					if ((hbLogCounter++ % 3600) == 0) {  // Log every ~1 minute
						OverlayLog("HURTBOX[0][0]: world(%.0f,%.0f) -> screen(%.0f,%.0f) r=%.1f zoom=%.2f charPos(%.0f,%.0f)",
							hb.x, hb.y, screenX, screenY, rr, zoom, ch.posX, ch.posY);
					}
				}
				
				gfx.FillEllipse(&brushHurt, Gdiplus::RectF(screenX - rr, screenY - rr, rr * 2, rr * 2));
				hurtboxCount++;
			}
	}
}

// Draw projectiles
int projectileCount = 0;
if (g_overlayEnabled && inMatch && shouldDrawOverlay)
	{
		Gdiplus::SolidBrush brushProj(Gdiplus::Color(200, 255, 165, 0)); // Orange
		for (int i = 0; i < (int)g_sharedData->projectileCount && i < MAX_PROJECTILES; i++)
		{
			const SharedProjectile& proj = g_sharedData->projectiles[i];
			if (!proj.active) continue;
			float screenX, screenY, zoom;
			ProjectToScreen(proj.x, proj.y, proj.z, screenX, screenY, zoom, g_windowWidth, g_windowHeight);
			float rr = 15.0f * zoom;
			if (rr < 3.0f) rr = 3.0f; if (rr > 200.0f) rr = 200.0f;
			gfx.FillEllipse(&brushProj, Gdiplus::RectF(screenX - rr, screenY - rr, rr * 2, rr * 2));
			projectileCount++;
		}
	}

	// Removed per-frame logging - was causing stuttering

	// Push the ARGB surface to the layered window
	SIZE size = { g_windowWidth, g_windowHeight };
	POINT ptSrc = { 0, 0 };
	RECT rect;
	GetWindowRect(g_overlayWindow, &rect);
	POINT ptPos = { rect.left, rect.top };
	BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
	UpdateLayeredWindow(g_overlayWindow, hdcScreen, &ptPos, &size, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

	// Cleanup GDI objects
	SelectObject(hdcMem, hbmOld);
	DeleteObject(hbm);
	DeleteDC(hdcMem);
	ReleaseDC(NULL, hdcScreen);
}

// Message loop
static void MessageLoop()
{
	MSG msg = {};
	DWORD lastGameCheckTime = GetTickCount();
	const DWORD GAME_CHECK_INTERVAL = 1000;  // Check game every 1 second

	while (g_running)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				g_running = false;
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		if (!g_running)
			break;

		// Periodically check if game window still exists
		DWORD currentTime = GetTickCount();
		if (currentTime - lastGameCheckTime >= GAME_CHECK_INTERVAL)
		{
			lastGameCheckTime = currentTime;
			HWND gameWindow = FindWindowA(NULL, "ULTIMATE MARVEL VS. CAPCOM 3");
			if (!gameWindow || !IsWindow(gameWindow))
			{
				OverlayLog("MessageLoop: Game window no longer found, exiting...");
				g_running = false;
				break;
			}
		}

		// Handle global hotkeys
		if (msg.message == WM_HOTKEY)
		{
			switch (msg.wParam)
			{
			case 1: // ALT+H toggle overlay
				g_overlayEnabled = !g_overlayEnabled;
				OverlayLog("Hotkey: overlayEnabled=%d", g_overlayEnabled ? 1 : 0);
				break;
			case 2: // ALT+ESC exit
				OverlayLog("Hotkey: exit requested");
				g_running = false;
				break;
			case 3: // ALT+D toggle debug circle
				g_debugCircle = !g_debugCircle;
				OverlayLog("Hotkey: debugCircle=%d", g_debugCircle ? 1 : 0);
				break;
			case 4: // Numpad+ increase Z offset (farther from camera)
				g_hurtcircleZOffset += kZOffsetStep;
				OverlayLog("Hotkey: Z offset = %.1f (farther)", g_hurtcircleZOffset);
				break;
			case 5: // Numpad- decrease Z offset (closer to camera)
				g_hurtcircleZOffset -= kZOffsetStep;
				OverlayLog("Hotkey: Z offset = %.1f (closer)", g_hurtcircleZOffset);
				break;
			case 6: // Numpad0 reset Z offset
				g_hurtcircleZOffset = 0.0f;
				OverlayLog("Hotkey: Z offset reset to 0");
				break;
			}
		}

		// Only render when the game window is focused
		if (g_gameWindow && GetForegroundWindow() == g_gameWindow)
		{
			RenderFrame();
		}
		Sleep(1);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	// Clear log file on start
	FILE* f = fopen("UMVC3OverlayApp.log", "w");
	if (f) fclose(f);
	
	OverlayLog("=== UMVC3 Overlay App Started ===");
	
	// Create status window
	CreateStatusWindow(hInstance);
	UpdateStatus("Waiting for UMVC3 to start...\nPlease launch the game.", 0);
	
	// Wait for game to start (max 5 minutes)
	const int MAX_WAIT_SECONDS = 300;
	int waitedSeconds = 0;
	HWND gameWindow = NULL;
	
	OverlayLog("Waiting for UMVC3 to start...");
	
	while (!gameWindow && waitedSeconds < MAX_WAIT_SECONDS)
	{
		gameWindow = FindWindowA(NULL, "ULTIMATE MARVEL VS. CAPCOM 3");
		
		if (!gameWindow)
		{
			// Update status with waiting time
			char statusText[256];
			sprintf(statusText, "Waiting for UMVC3...\n(%d seconds)", waitedSeconds);
			UpdateStatus(statusText, (waitedSeconds * 100) / MAX_WAIT_SECONDS);
			
			// Log every 10 seconds
			if (waitedSeconds % 10 == 0 && waitedSeconds > 0)
			{
				OverlayLog("Still waiting for UMVC3... (%d seconds)", waitedSeconds);
			}
			
			Sleep(1000);
			waitedSeconds++;
		}
	}
	
	if (!gameWindow)
	{
		OverlayLog("WinMain: TIMEOUT - UMVC3 game not found after %d seconds.", MAX_WAIT_SECONDS);
		UpdateStatus("Timeout! UMVC3 not found.\nPlease restart the overlay.", 0);
		Sleep(3000);
		CloseStatusWindow();
		return 1;
	}
	
	OverlayLog("WinMain: Found UMVC3 game window after %d seconds!", waitedSeconds);
	UpdateStatus("UMVC3 detected!\nWaiting for game to initialize...", 30);
	
	// Wait a bit for game to fully initialize
	OverlayLog("Waiting 3 seconds for game to fully initialize...");
	for (int i = 0; i < 3; i++)
	{
		char statusText[256];
		sprintf(statusText, "Game detected!\nInitializing... (%d/3)", i + 1);
		UpdateStatus(statusText, 30 + (i * 10));
		Sleep(1000);
	}
	
	// Inject DLL
	UpdateStatus("Injecting DLL into game...", 60);
	OverlayLog("WinMain: Attempting to inject DLL...");
	
	bool injected = InjectDLL(gameWindow);
	
	if (!injected)
	{
		OverlayLog("WinMain: DLL injection returned false (may already be injected)");
		UpdateStatus("DLL may already be loaded.\nConnecting...", 65);
	}
	else
	{
		OverlayLog("WinMain: DLL injection successful!");
		UpdateStatus("DLL injected successfully!\nWaiting for initialization...", 70);
		
		// Wait for DLL to initialize
		OverlayLog("Waiting 2 seconds for DLL to initialize...");
		Sleep(2000);
	}
	
	UpdateStatus("Creating overlay window...", 75);
	OverlayLog("WinMain: Creating overlay window...");
	
	if (!CreateOverlayWindow())
	{
		OverlayLog("WinMain: FAILED - CreateOverlayWindow");
		UpdateStatus("ERROR: Failed to create overlay window!", 0);
		Sleep(3000);
		CloseStatusWindow();
		return 1;
	}

	// Try to open shared memory with retries
	UpdateStatus("Connecting to DLL...", 80);
	OverlayLog("WinMain: Attempting to open shared memory...");
	int retries = 0;
	const int MAX_RETRIES = 20;
	
	while (!OpenSharedMemory() && retries < MAX_RETRIES)
	{
		char statusText[256];
		sprintf(statusText, "Connecting to DLL... (%d/%d)", retries + 1, MAX_RETRIES);
		UpdateStatus(statusText, 80 + (retries * 100 / MAX_RETRIES / 5));
		OverlayLog("WinMain: Shared memory not available yet, retry %d/%d...", retries + 1, MAX_RETRIES);
		Sleep(500);
		retries++;
	}
	
	if (!g_sharedData)
	{
		OverlayLog("WinMain: FAILED - Could not open shared memory after %d retries", MAX_RETRIES);
		UpdateStatus("ERROR: Could not connect to DLL!\nDLL may not have loaded.", 0);
		Sleep(3000);
		CloseStatusWindow();
		DestroyWindow(g_overlayWindow);
		UnregisterClassA("UMVC3OverlayClass", hInstance);
		return 1;
	}
	
	OverlayLog("WinMain: Shared memory connected successfully!");
	UpdateStatus("Connected! Starting overlay...", 95);
	Sleep(500);

	// Register system-wide hotkeys
	RegisterHotKey(NULL, 1, MOD_ALT, 'H');
	RegisterHotKey(NULL, 2, MOD_ALT, VK_ESCAPE);
	RegisterHotKey(NULL, 3, MOD_ALT, 'D');
	RegisterHotKey(NULL, 4, 0, VK_ADD);      // Numpad + (farther)
	RegisterHotKey(NULL, 5, 0, VK_SUBTRACT); // Numpad - (closer)
	RegisterHotKey(NULL, 6, 0, VK_NUMPAD0);  // Numpad 0 (reset)

	// Start GDI+
	GdiPlusBegin();

	OverlayLog("WinMain: All initialization complete!");
	OverlayLog("Hotkeys: Alt+H = Toggle overlay, Alt+D = Debug circle, Alt+Esc = Exit");
	OverlayLog("Numpad: + = Farther Z, - = Closer Z, 0 = Reset Z offset");
	
	UpdateStatus("Overlay active!\nClosing this window...", 100);
	Sleep(1000);
	
	// Close status window - overlay is now running
	CloseStatusWindow();

	MessageLoop();

	OverlayLog("WinMain: Message loop ended, cleaning up...");
	
	// Unregister hotkeys
	UnregisterHotKey(NULL, 1);
	UnregisterHotKey(NULL, 2);
	UnregisterHotKey(NULL, 3);
	UnregisterHotKey(NULL, 4);
	UnregisterHotKey(NULL, 5);
	UnregisterHotKey(NULL, 6);

	GdiPlusShutdown();
	CloseSharedMemory();
	DestroyWindow(g_overlayWindow);
	UnregisterClassA("UMVC3OverlayClass", hInstance);

	OverlayLog("=== UMVC3 Overlay App Closed ===");
	
	return 0;
}
