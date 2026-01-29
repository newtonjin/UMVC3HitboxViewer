#pragma once
#include <cstdint>
// Configuration constants for UMVC3 Overlay
namespace Config
{
	// Character detection thresholds
	constexpr float ASSIST_VELOCITY_THRESHOLD = 0.001f;  // Minimum velocity to consider assist active
	constexpr float BENCHED_Y_THRESHOLD = 800.0f;        // Characters beyond this Y are benched
	
	// Rendering options
	constexpr uint32_t HITBOX_COLOR = 0xFF0000FF;        // Red for hitboxes (BGRA)
	constexpr uint32_t HURTBOX_COLOR = 0xFF00FF00;       // Green for hurtboxes
	constexpr uint32_t INACTIVE_COLOR = 0xFF888888;      // Gray for inactive
	
	constexpr float BOX_LINE_THICKNESS = 2.0f;           // Thickness of drawn lines
	constexpr int BOX_CIRCLE_SEGMENTS = 16;              // Segments for circle rendering
	
	// Memory limits
	constexpr size_t MAX_HITBOXES_PER_CHARACTER = 32;
	constexpr size_t MAX_HURTBOXES_PER_CHARACTER = 48;
	constexpr size_t MAX_CHARACTERS = 6;                 // 3 per team
	
	// Logging
	constexpr bool ENABLE_DETAILED_LOGGING = true;
	constexpr const char* LOG_FILE_NAME = "UMVC3Overlay.log";
	
	// ImGui debug info
	constexpr bool SHOW_DEBUG_WINDOW = true;
	constexpr bool SHOW_CHARACTER_INFO = true;
	
	// Camera matrix reading
	// These offsets may need adjustment based on game version
	constexpr uint32_t CAMERA_OBJECT_OFFSET = 0x50;
	constexpr uint32_t VIEW_MATRIX_OFFSET = 0xD0;
	constexpr uint32_t PROJECTION_MATRIX_OFFSET = 0x100;
	
	// Hitbox/Hurtbox structure sizes (in bytes)
	constexpr size_t HITBOX_ENTRY_SIZE = 64;
	constexpr size_t HURTBOX_ENTRY_SIZE = 64;
	
	// Position offsets within box entries
	constexpr uint32_t BOX_POS_X_OFFSET = 0x00;
	constexpr uint32_t BOX_POS_Y_OFFSET = 0x04;
	constexpr uint32_t BOX_POS_Z_OFFSET = 0x08;
	constexpr uint32_t BOX_RADIUS_OFFSET = 0x0C;
}
