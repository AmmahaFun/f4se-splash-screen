// =============================================================================
// SplashScreen.cpp - Версия Splash Screen для F4SE с поддержкой PNG и прозрачности
// =============================================================================
/*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * Copyright (c) 2025 TheYcUt
 */

 // Copyright (c) 2025 TheYcUt
 // SPDX-License-Identifier: GNU

// =============================================================================
// ЛИЦЕНЗИОННОЕ СОГЛАШЕНИЕ
// =============================================================================
/*
 * ВНИМАНИЕ: Коммерческое использование запрещено без явного разрешения автора.
 * This software is NOT licensed for commercial use without explicit permission.
 *
 * Для коммерческого лицензирования обращайтесь: iscrappyeso@gmail.com
 * For commercial licensing contact: iscrappyeso@gmail.com
 */

// В самом начале определяем версию Windows ДО включения заголовков
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601  // Windows 7

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shlobj.h>  // Для SHGetFolderPathW
#include <string>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <atomic>
#include <thread>
#include <vector>
#include <TlHelp32.h> // Для поиска процессов

// GDI+ для поддержки PNG
#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")

// Библиотека для AlphaBlend функции
#pragma comment(lib, "Msimg32.lib")

// Определяем типы данных для F4SE перед включением заголовков
typedef unsigned int UInt32;
typedef unsigned long long UInt64;
typedef signed int SInt32;
typedef signed long long SInt64;

// Включаем заголовки F4SE ПРАВИЛЬНЫМ образом
#include "f4se_common/f4se_version.h"
#include "f4se/PluginAPI.h"

// =============================================================================
// МЕТАДАННЫЕ
// =============================================================================

// Метаданные для DLL (дублируем из Version.h для удобства)
#define FILE_DESCRIPTION    "Custom Splash Screen for Fallout 4 - NON-COMMERCIAL"
#define FILE_VERSION        "1.0.0.0"
#define INTERNAL_NAME       "SplashScreen.dll"
#define COMPANY_NAME        "TheYcUt (Non-commercial)"
#define LEGAL_COPYRIGHT     "Copyright © 2025 TheYcUt - Non-commercial use only"
#define ORIGINAL_FILENAME   "SplashScreen.dll"
#define PRODUCT_NAME        "Fallout 4 Splash Screen"
#define PRODUCT_VERSION     "1.0.0.0"

// =============================================================================
// НАСТРОЙКИ
// =============================================================================
const int SPLASH_DURATION_MS = 9000;  // 9 секунд задержка перед завершением
const wchar_t* SPLASH_IMAGE_NAME = L"splash.png";
const wchar_t* WINDOW_CLASS_NAME = L"F4SESplashScreen";

// Информация о плагине
#define MODNAME "SplashScreen"
#define AUTHOR "YourName"
#define MOD_VERSION 1

// =============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ F4SE
// =============================================================================
PluginHandle g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface* g_messaging = nullptr;

// =============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ПЛАГИНА
// =============================================================================
static HMODULE g_hModule = nullptr;
static HBITMAP g_splashBitmap = nullptr;
static int g_splashWidth = 0;
static int g_splashHeight = 0;
static std::atomic<bool> g_splashThreadRunning{ false };
static std::atomic<bool> g_shutdownRequested{ false };
static std::thread g_splashThread;
static bool g_windowClassRegistered = false;
static std::wstring g_logFilePath;
static std::wstring g_runtimeDirectory;

// GDI+
static ULONG_PTR g_gdiplusToken = 0;

// =============================================================================
// ФУНКЦИИ ЛОГИРОВАНИЯ
// =============================================================================
void WriteToLog(const std::string& message) {
    std::ofstream logFile;
    logFile.open(g_logFilePath, std::ios_base::app);

    if (logFile.is_open()) {
        // Добавляем временную метку
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        logFile << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";
        logFile << message << std::endl;
        logFile.close();
    }
}

void WriteToLog(const std::wstring& message) {
    std::ofstream logFile;
    logFile.open(g_logFilePath, std::ios_base::app);

    if (logFile.is_open()) {
        // Добавляем временную метку
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);

        logFile << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "] ";

        // Конвертируем wide string в narrow string для записи в файл
        std::string narrowMessage(message.begin(), message.end());
        logFile << narrowMessage << std::endl;
        logFile.close();
    }
}

bool InitializeLogFile() {
    wchar_t documentsPath[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, documentsPath) != S_OK) {
        return false;
    }

    // Создаем путь к лог-файлу: Documents\My Games\Fallout4\F4SE\SplashScreen.log
    g_logFilePath = std::wstring(documentsPath) + L"\\My Games\\Fallout4\\F4SE\\SplashScreen.log";

    // Создаем директории если их нет
    std::wstring pluginsDir = std::wstring(documentsPath) + L"\\My Games\\Fallout4\\F4SE";
    CreateDirectoryW(pluginsDir.c_str(), nullptr);

    // Записываем заголовок в лог
    std::ofstream logFile;
    logFile.open(g_logFilePath, std::ios_base::out);
    if (logFile.is_open()) {
        logFile << "=== SplashScreen Plugin Log ===" << std::endl;
        logFile << "Plugin loaded at: ";

        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        logFile << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << std::endl;
        logFile.close();

        WriteToLog("Log system initialized successfully");
        return true;
    }

    return false;
}

// =============================================================================
// ФУНКЦИИ ДЛЯ ЗАВЕРШЕНИЯ ПРОЦЕССОВ
// =============================================================================
DWORD FindProcessByName(const std::wstring& processName) {
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) {
        WriteToLog("Failed to create process snapshot");
        return 0;
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(PROCESSENTRY32);

    if (!Process32First(hSnapshot, &pe)) {
        CloseHandle(hSnapshot);
        return 0;
    }

    DWORD targetPID = 0;
    do {
        std::wstring currentProcessName = pe.szExeFile;
        if (currentProcessName == processName) {
            targetPID = pe.th32ProcessID;
            WriteToLog(L"Found " + processName + L" with PID: " + std::to_wstring(targetPID));
            break;
        }
    } while (Process32Next(hSnapshot, &pe));

    CloseHandle(hSnapshot);
    return targetPID;
}

void TerminateProcessByName(const std::wstring& processName) {
    WriteToLog(L"Attempting to terminate process: " + processName);

    DWORD processPID = FindProcessByName(processName);
    if (processPID == 0) {
        WriteToLog(L"Process " + processName + L" not found");
        return;
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, processPID);
    if (hProcess == NULL) {
        WriteToLog(L"Failed to open process " + processName + L" for termination");
        return;
    }

    // ЗАКОММЕНТИРОВАНО: завершение процесса
    /*
    if (TerminateProcess(hProcess, 0)) {
        WriteToLog(L"Successfully terminated " + processName);
    }
    else {
        DWORD error = GetLastError();
        WriteToLog(L"Failed to terminate " + processName + L". Error code: " + std::to_wstring(error));
    }
    */

    CloseHandle(hProcess);
}

void TerminateAllTargetProcesses() {
    WriteToLog("Starting termination of all target processes...");

    // ЗАКОММЕНТИРОВАНО: завершение f4se_loader.exe
    // TerminateProcessByName(L"f4se_loader.exe");

    // ЗАКОММЕНТИРОВАНО: завершение Fallout4.exe
    // TerminateProcessByName(L"Fallout4.exe");

    WriteToLog("Process termination sequence completed");
}

// =============================================================================
// DllMain
// =============================================================================
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        g_hModule = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);

        // Инициализируем GDI+
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);

        // Инициализируем систему логирования
        InitializeLogFile();
        WriteToLog("DLL loaded successfully");
        WriteToLog("GDI+ initialized for PNG support");
    }
    else if (fdwReason == DLL_PROCESS_DETACH) {
        WriteToLog("DLL unloaded");

        // Останавливаем поток если он запущен
        g_shutdownRequested = true;
        if (g_splashThread.joinable()) {
            g_splashThread.join();
        }

        // Освобождаем ресурсы
        if (g_splashBitmap) {
            DeleteObject(g_splashBitmap);
            g_splashBitmap = nullptr;
        }

        // Освобождаем GDI+
        if (g_gdiplusToken) {
            Gdiplus::GdiplusShutdown(g_gdiplusToken);
            WriteToLog("GDI+ shutdown");
        }
    }
    return TRUE;
}

// =============================================================================
// ФУНКЦИИ ОКНА С ПОДДЕРЖКОЙ ПРОЗРАЧНОСТИ
// =============================================================================
LRESULT CALLBACK SplashWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (g_splashBitmap) {
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, g_splashBitmap);

            // Используем AlphaBlend для поддержки прозрачности PNG
            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.BlendFlags = 0;
            blend.SourceConstantAlpha = 255;  // Полная непрозрачность
            blend.AlphaFormat = AC_SRC_ALPHA; // Используем альфа-канал из изображения

            AlphaBlend(hdc, 0, 0, g_splashWidth, g_splashHeight,
                hdcMem, 0, 0, g_splashWidth, g_splashHeight, blend);

            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMem);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Предотвращаем стирание фона

    case WM_DESTROY:
        WriteToLog("Splash window destroyed");
        return 0;

    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

bool RegisterSplashWindowClass() {
    if (g_windowClassRegistered) return true;

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = SplashWndProc;
    wc.hInstance = g_hModule;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = WINDOW_CLASS_NAME;

    if (RegisterClassEx(&wc)) {
        g_windowClassRegistered = true;
        WriteToLog("Window class registered successfully");
        return true;
    }

    WriteToLog("Failed to register window class");
    return false;
}

// =============================================================================
// ФУНКЦИИ ИЗОБРАЖЕНИЯ С ПОДДЕРЖКОЙ PNG И ПРОЗРАЧНОСТИ
// =============================================================================
bool LoadSplashImage(const std::wstring& imagePath) {
    WriteToLog(L"Attempting to load splash image with GDI+: " + imagePath);

    // Проверяем существование файла
    DWORD fileAttrib = GetFileAttributesW(imagePath.c_str());
    if (fileAttrib == INVALID_FILE_ATTRIBUTES) {
        WriteToLog(L"File does not exist or cannot be accessed");
        return false;
    }

    // Загружаем изображение через GDI+
    Gdiplus::Bitmap* gdiBitmap = Gdiplus::Bitmap::FromFile(imagePath.c_str());
    if (!gdiBitmap) {
        WriteToLog("Failed to create GDI+ bitmap from file");
        return false;
    }

    // Проверяем статус загрузки
    if (gdiBitmap->GetLastStatus() != Gdiplus::Ok) {
        WriteToLog("GDI+ failed to load image file");
        delete gdiBitmap;
        return false;
    }

    // Получаем размеры
    g_splashWidth = gdiBitmap->GetWidth();
    g_splashHeight = gdiBitmap->GetHeight();

    if (g_splashWidth <= 0 || g_splashHeight <= 0) {
        WriteToLog("Invalid image dimensions");
        delete gdiBitmap;
        return false;
    }

    // Проверяем поддерживает ли изображение альфа-канал
    Gdiplus::PixelFormat pixelFormat = gdiBitmap->GetPixelFormat();
    bool hasAlpha = (pixelFormat & PixelFormatAlpha) != 0;

    WriteToLog("GDI+ image loaded successfully. Dimensions: " +
        std::to_string(g_splashWidth) + "x" + std::to_string(g_splashHeight) +
        ", Has alpha: " + (hasAlpha ? "yes" : "no"));

    // Создаем 32-битный DIB с альфа-каналом для правильной прозрачности
    HDC hdcScreen = GetDC(nullptr);

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_splashWidth;
    bmi.bmiHeader.biHeight = -g_splashHeight; // Отрицательная высота для top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    bmi.bmiHeader.biSizeImage = 0;

    void* pBits = nullptr;
    HBITMAP hBitmap = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);

    if (!hBitmap) {
        WriteToLog("Failed to create DIB section for alpha channel");
        delete gdiBitmap;
        ReleaseDC(nullptr, hdcScreen);
        return false;
    }

    // Копируем данные из GDI+ Bitmap в наш DIB с альфа-каналом
    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);

    Gdiplus::Graphics graphics(hdcMem);
    Gdiplus::Rect rect(0, 0, g_splashWidth, g_splashHeight);

    // Очищаем фон прозрачным
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));

    // Рисуем изображение с сохранением альфа-канала
    graphics.DrawImage(gdiBitmap, rect);

    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
    delete gdiBitmap;

    // Устанавливаем глобальный битмап
    g_splashBitmap = hBitmap;

    WriteToLog("PNG image successfully converted to HBITMAP with alpha channel support");
    return true;
}

// =============================================================================
// ФУНКЦИИ ДЛЯ АВТООПРЕДЕЛЕНИЯ ПУТИ
// =============================================================================
std::wstring FindSplashImage() {
    WriteToLog("Searching for splash image...");

    // 1. Первый приоритет: в папке модов (где находится ваш файл)
    std::vector<std::wstring> modPaths = {
        g_runtimeDirectory + L"\\modpack\\mods\\Splash Screen F4\\F4SE\\Plugins\\Splash\\" + SPLASH_IMAGE_NAME,
        g_runtimeDirectory + L"\\mods\\Splash Screen F4\\F4SE\\Plugins\\Splash\\" + SPLASH_IMAGE_NAME,
        g_runtimeDirectory + L"\\Splash Screen F4\\F4SE\\Plugins\\Splash\\" + SPLASH_IMAGE_NAME,
    };

    for (const auto& path : modPaths) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            WriteToLog(L"Found splash image in mods folder: " + path);
            return path;
        }
        WriteToLog(L"Not found in mods: " + path);
    }

    // 2. Второй приоритет: в той же папке, где находится сам плагин DLL
    wchar_t pluginPath[MAX_PATH];
    if (GetModuleFileNameW(g_hModule, pluginPath, MAX_PATH)) {
        std::wstring pluginDir = pluginPath;
        size_t lastSlash = pluginDir.find_last_of(L"\\/");
        if (lastSlash != std::wstring::npos) {
            pluginDir = pluginDir.substr(0, lastSlash);

            // Пробуем разные варианты в папке плагина
            std::vector<std::wstring> localPaths = {
                pluginDir + L"\\" + SPLASH_IMAGE_NAME,                    // Рядом с DLL
                pluginDir + L"\\Splash\\" + SPLASH_IMAGE_NAME,            // В подпапке Splash
                pluginDir + L"\\..\\Splash\\" + SPLASH_IMAGE_NAME,        // В соседней папке Splash
            };

            for (const auto& path : localPaths) {
                DWORD attrib = GetFileAttributesW(path.c_str());
                if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
                    WriteToLog(L"Found splash image at: " + path);
                    return path;
                }
                WriteToLog(L"Not found: " + path);
            }
        }
    }

    // 3. Третий приоритет: стандартные пути F4SE
    std::vector<std::wstring> standardPaths = {
        g_runtimeDirectory + L"\\Data\\F4SE\\Plugins\\Splash\\" + SPLASH_IMAGE_NAME,
        g_runtimeDirectory + L"\\Data\\F4SE\\Plugins\\" + SPLASH_IMAGE_NAME,
        g_runtimeDirectory + L"\\F4SE\\Plugins\\Splash\\" + SPLASH_IMAGE_NAME,
    };

    for (const auto& path : standardPaths) {
        DWORD attrib = GetFileAttributesW(path.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY)) {
            WriteToLog(L"Found splash image at: " + path);
            return path;
        }
        WriteToLog(L"Not found: " + path);
    }

    WriteToLog("Splash image not found in any location");
    return L"";
}

// =============================================================================
// ФУНКЦИЯ ДЛЯ БЛОКИРОВКИ И ЗАВЕРШЕНИЯ ПРОЦЕССОВ
// =============================================================================
void BlockAndTerminate() {
    WriteToLog("Starting splash screen with process termination after delay...");

    // Автоопределение пути к картинке
    std::wstring imagePath = FindSplashImage();

    if (imagePath.empty()) {
        WriteToLog("Cannot find splash image file, terminating immediately");
        // ЗАКОММЕНТИРОВАНО: завершение процессов при отсутствии изображения
        // TerminateAllTargetProcesses();
        return;
    }

    // Загружаем изображение
    if (!LoadSplashImage(imagePath)) {
        WriteToLog("Failed to load splash image, terminating immediately");
        // ЗАКОММЕНТИРОВАНО: завершение процессов при ошибке загрузки изображения
        // TerminateAllTargetProcesses();
        return;
    }

    // Запускаем сплеш-скрин в главном потоке (блокирующе)
    WriteToLog("Starting blocking splash screen for " + std::to_string(SPLASH_DURATION_MS) + "ms before termination");

    if (!RegisterSplashWindowClass() || !g_splashBitmap) {
        WriteToLog("Failed to start splash screen - registration or bitmap failed, terminating immediately");
        // ЗАКОММЕНТИРОВАНО: завершение процессов при ошибке создания окна
        // TerminateAllTargetProcesses();
        return;
    }

    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int windowX = (screenWidth - g_splashWidth) / 2;
    int windowY = (screenHeight - g_splashHeight) / 2;

    WriteToLog("Creating splash window with transparency support...");

    // Создаем окно с поддержкой прозрачности
    HWND hWnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        WINDOW_CLASS_NAME,
        L"",
        WS_POPUP,
        windowX, windowY, g_splashWidth, g_splashHeight,
        nullptr, nullptr, g_hModule, nullptr
    );

    if (!hWnd) {
        DWORD error = GetLastError();
        WriteToLog("Failed to create splash window. Error code: " + std::to_string(error));
        // ЗАКОММЕНТИРОВАНО: завершение процессов при ошибке создания окна
        // TerminateAllTargetProcesses();
        return;
    }

    WriteToLog("Splash window created successfully");

    // Устанавливаем layered window attributes для поддержки прозрачности
    SetLayeredWindowAttributes(hWnd, 0, 255, LWA_ALPHA);

    // Показываем окно
    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    WriteToLog("Splash window shown with transparency support. Blocking for " + std::to_string(SPLASH_DURATION_MS) + "ms before termination");

    // БЛОКИРУЕМ ГЛАВНЫЙ ПОТОК НА 9 СЕКУНД
    ULONGLONG startTime = GetTickCount64();

    MSG msg;
    while (true) {
        // Проверяем время
        ULONGLONG currentTime = GetTickCount64();
        ULONGLONG elapsed = currentTime - startTime;

        if (elapsed >= (ULONGLONG)SPLASH_DURATION_MS) {
            WriteToLog("Splash screen duration completed");
            break;
        }

        // Обрабатываем сообщения окна
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Обновляем окно для корректного отображения прозрачности
        InvalidateRect(hWnd, nullptr, TRUE);
        UpdateWindow(hWnd);

        Sleep(10);
    }

    // Закрываем окно
    if (IsWindow(hWnd)) {
        DestroyWindow(hWnd);
        WriteToLog("Splash window destroyed");
    }

    // ЗАКОММЕНТИРОВАНО: завершение целевых процессов
    // TerminateAllTargetProcesses();
}

// =============================================================================
// F4SE ИНТЕГРАЦИЯ
// =============================================================================

void F4SEMessageHandler(F4SEMessagingInterface::Message* msg) {
    WriteToLog("F4SE message received: " + std::to_string(msg->type));

    if (msg->type == F4SEMessagingInterface::kMessage_PostLoad) {
        WriteToLog("Game loaded message received");
        // Теперь игра загрузится только после завершения сплеш-скрина
    }
}

bool GetGameDirectory() {
    // Альтернативный способ получить путь к игре через GetModuleFileName
    wchar_t gamePath[MAX_PATH];
    if (GetModuleFileNameW(GetModuleHandleW(nullptr), gamePath, MAX_PATH) == 0) {
        WriteToLog("Failed to get game executable path");
        return false;
    }

    // Убираем имя исполняемого файла чтобы получить директорию
    std::wstring gameDir = gamePath;
    size_t lastSlash = gameDir.find_last_of(L"\\/");
    if (lastSlash != std::wstring::npos) {
        g_runtimeDirectory = gameDir.substr(0, lastSlash);
        WriteToLog(L"Game directory detected: " + g_runtimeDirectory);
        return true;
    }

    WriteToLog("Failed to extract game directory from path");
    return false;
}

// =============================================================================
// F4SE PLUGIN VERSION COMPATIBILITY (SIMPLE VERSION)
// =============================================================================
extern "C" {

    // Простая проверка через RUNTIME_VERSION
#if RUNTIME_VERSION_1_10_984 == CURRENT_RELEASE_RUNTIME
// for f4se 7.0 and newer
    __declspec(dllexport) F4SEPluginVersionData F4SEPlugin_Version =
    {
        F4SEPluginVersionData::kVersion,
        MOD_VERSION,
        MODNAME,
        AUTHOR,
        0, 0,
        { RUNTIME_VERSION_1_10_984, 0 },
        0,
    };
#else
// for f4se 6.23 and older
    __declspec(dllexport) bool F4SEPlugin_Query(const F4SEInterface* f4se, PluginInfo* info)
    {
        WriteToLog("F4SEPlugin_Query called (old F4SE version)");

        if (f4se->runtimeVersion != RUNTIME_VERSION_1_10_163) {
            WriteToLog("Unsupported runtime version for old F4SE");
            return false;
        }

        if (f4se->isEditor) {
            WriteToLog("Loaded in editor, marking as incompatible");
            return false;
        }

        info->infoVersion = PluginInfo::kInfoVersion;
        info->version = MOD_VERSION;
        info->name = MODNAME;

        g_pluginHandle = f4se->GetPluginHandle();
        WriteToLog("Plugin handle: " + std::to_string(g_pluginHandle));

        if (!GetGameDirectory()) {
            WriteToLog("Failed to detect game directory");
            return false;
        }

        BlockAndTerminate();

        WriteToLog("F4SEPlugin_Query completed successfully (old F4SE)");
        return true;
    }
#endif

    __declspec(dllexport) bool F4SEPlugin_Load(const F4SEInterface* f4se)
    {
        WriteToLog("F4SEPlugin_Load called");

        if (f4se->isEditor) {
            WriteToLog("Loaded in editor, returning false");
            return false;
        }

#if RUNTIME_VERSION_1_10_984 == CURRENT_RELEASE_RUNTIME
        // Для новой версии F4SE
        g_pluginHandle = f4se->GetPluginHandle();
        WriteToLog("Plugin handle: " + std::to_string(g_pluginHandle));

        if (!GetGameDirectory()) {
            WriteToLog("Failed to detect game directory");
            return false;
        }

        BlockAndTerminate();
#endif

        g_messaging = (F4SEMessagingInterface*)f4se->QueryInterface(kInterface_Messaging);
        if (!g_messaging) {
            WriteToLog("Failed to get messaging interface");
            return false;
        }

        WriteToLog("Messaging interface obtained successfully");

        if (g_messaging->RegisterListener(g_pluginHandle, "F4SE", F4SEMessageHandler) == false) {
            WriteToLog("Failed to register message listener");
            return false;
        }

        WriteToLog("Message listener registered successfully");
        WriteToLog("F4SEPlugin_Load completed successfully");

        return true;
    }

} // extern "C"