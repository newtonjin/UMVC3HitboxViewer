#include "logger.h"
#include <iostream>
#include <cstdarg>
#include <chrono>

Logger::Logger() {}

Logger::~Logger()
{
	Shutdown();
}

Logger& Logger::Get()
{
	static Logger instance;
	return instance;
}

void Logger::Initialize(const std::string& logFile)
{
	logStream.open(logFile, std::ios::app);
	initialized = logStream.is_open();
	
	if (initialized)
	{
		Log("[UMVC3Overlay] Logger initialized successfully.");
	}
	else
	{
		// If file logging fails, at least try console
		std::cerr << "[UMVC3Overlay] Failed to open log file: " << logFile << std::endl;
	}
}

void Logger::Log(const std::string& message)
{
	if (!initialized) return;
	
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	
	logStream << "[" << std::ctime(&time);
	logStream.seekp(-1, std::ios_base::end);  // Remove newline from ctime
	logStream << "] " << message << std::endl;
	logStream.flush();
}

void Logger::LogFormat(const char* format, ...)
{
	if (!initialized) return;
	
	char buffer[1024];
	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);
	
	Log(buffer);
}

void Logger::Shutdown()
{
	if (logStream.is_open())
	{
		Log("[UMVC3Overlay] Logger shutting down.");
		logStream.close();
	}
	initialized = false;
}
