#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// Encontra PID do processo
DWORD FindProcessByName(const std::string& processName) {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return 0;

    PROCESSENTRY32 entry = { sizeof(entry) };
    if (!Process32First(snapshot, &entry)) {
        CloseHandle(snapshot);
        return 0;
    }

    do {
        if (processName == entry.szExeFile) {
            CloseHandle(snapshot);
            return entry.th32ProcessID;
        }
    } while (Process32Next(snapshot, &entry));

    CloseHandle(snapshot);
    return 0;
}

// Injeta DLL no processo
bool InjectDLL(DWORD processID, const std::string& dllPath) {
    // Validar DLL existe
    if (!fs::exists(dllPath)) {
        std::cerr << "[ERRO] DLL nao encontrada: " << dllPath << std::endl;
        return false;
    }

    std::cout << "[INFO] Abrindo processo (PID: " << processID << ")..." << std::endl;

    HANDLE hProcess = OpenProcess(
        PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | 
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, processID
    );

    if (!hProcess) {
        std::cerr << "[ERRO] Falha ao abrir processo! Error: " << GetLastError() << std::endl;
        return false;
    }

    std::cout << "[INFO] Processo aberto com sucesso." << std::endl;

    // Alocar memoria para caminho DLL
    size_t pathLen = dllPath.length() + 1;
    LPVOID pDllPath = VirtualAllocEx(hProcess, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

    if (!pDllPath) {
        std::cerr << "[ERRO] Falha ao alocar memoria! Error: " << GetLastError() << std::endl;
        CloseHandle(hProcess);
        return false;
    }

    std::cout << "[INFO] Memoria alocada: " << (void*)pDllPath << std::endl;

    // Escrever caminho DLL na memoria
    if (!WriteProcessMemory(hProcess, pDllPath, dllPath.c_str(), pathLen, NULL)) {
        std::cerr << "[ERRO] Falha ao escrever memoria! Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::cout << "[INFO] Caminho DLL escrito na memoria." << std::endl;

    // Obter endereco LoadLibraryA
    LPVOID pLoadLibraryA = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    if (!pLoadLibraryA) {
        std::cerr << "[ERRO] Falha ao obter LoadLibraryA! Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::cout << "[INFO] Criando thread remota..." << std::endl;

    // Criar thread remota
    HANDLE hThread = CreateRemoteThread(hProcess, NULL, 0, 
        (LPTHREAD_START_ROUTINE)pLoadLibraryA, pDllPath, 0, NULL);

    if (!hThread) {
        std::cerr << "[ERRO] Falha ao criar thread remota! Error: " << GetLastError() << std::endl;
        VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
        CloseHandle(hProcess);
        return false;
    }

    std::cout << "[INFO] Thread remota criada. Aguardando conclusao..." << std::endl;

    // Aguardar conclusao
    WaitForSingleObject(hThread, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);

    // Limpar
    CloseHandle(hThread);
    VirtualFreeEx(hProcess, pDllPath, 0, MEM_RELEASE);
    CloseHandle(hProcess);

    if (exitCode == 0) {
        std::cerr << "[ERRO] DLL falhou ao carregar (exit code 0)" << std::endl;
        return false;
    }

    std::cout << "[SUCESSO] DLL injetada com sucesso!" << std::endl;
    return true;
}

int main() {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);

    std::cout << "================================================" << std::endl;
    std::cout << "      UMVC3 OVERLAY - DLL INJECTOR" << std::endl;
    std::cout << "================================================" << std::endl;
    std::cout << std::endl;

    // Procurar DLL
    std::string dllPath = "build/bin/Release/UMVC3Overlay.dll";
    if (!fs::exists(dllPath)) {
        dllPath = "../UMVC3 modding/build/bin/Release/UMVC3Overlay.dll";
    }
    if (!fs::exists(dllPath)) {
        dllPath = "UMVC3Overlay.dll";
    }

    std::cout << "[INFO] Procurando por umvc3.exe..." << std::endl;

    DWORD processID = FindProcessByName("umvc3.exe");

    if (processID == 0) {
        std::cerr << "[ERRO] UMVC3 nao esta rodando! Inicie o jogo e tente novamente." << std::endl;
        std::cout << "\nPressione qualquer tecla para sair..." << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "[INFO] UMVC3 encontrado! (PID: " << processID << ")" << std::endl;
    std::cout << "[INFO] DLL: " << dllPath << std::endl;
    std::cout << std::endl;

    if (InjectDLL(processID, dllPath)) {
        std::cout << std::endl;
        std::cout << "[INFO] Verifique o arquivo 'UMVC3Overlay.log' para debug." << std::endl;
        std::cout << "[INFO] Vá para Training Mode e teste as funcionalidades." << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "[ERRO] Falha na injecao!" << std::endl;
    }

    std::cout << "\nPressione qualquer tecla para sair..." << std::endl;
    std::cin.get();
    return 0;
}
