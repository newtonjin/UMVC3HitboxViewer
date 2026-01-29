#pragma once

#include <string>
#include <fstream>

class Logger
{
public:
	static Logger& Get();
	
	void Initialize(const std::string& logFile = "UMVC3Overlay.log");
	void Log(const std::string& message);
	void LogFormat(const char* format, ...);
	void Shutdown();

private:
	Logger();
	~Logger();
	
	std::ofstream logStream;
	bool initialized = false;
};

#define LOG_MSG(msg) Logger::Get().Log(msg)
#define LOG_FMT(fmt, ...) Logger::Get().LogFormat(fmt, __VA_ARGS__)
