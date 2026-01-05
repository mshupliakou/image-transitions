#include "pch.h"
#include <iostream>
#include <cstdint>    // Required for std::uint8_t
#include <filesystem> // C++17 library for creating folders and handling paths
#include <sstream>    // Required for string stream manipulations
#include <iomanip>    // Required for std::setw, std::setfill
#include <vector>
#include <cstring>
#include <cmath>
#include <future>     // Required for multithreading (std::async)
#include <algorithm>  // Required for std::min, std::max
#include <optional>   // Required for sf::Event event handling in SFML 3.0

// Create an alias for std::filesystem to save typing
namespace fs = std::filesystem;

// --- WINDOWS API BLOCK ---
#define NOMINMAX      // Prevents Windows headers from defining min/max macros
#include <Windows.h>
#include <commdlg.h>  // Standard Open File Dialog library
#include <shellapi.h> // For ShellExecute 
#include <shlobj.h>   // For SHBrowseForFolder

// --- GLOBAL CACHE VARIABLES ---
sf::Image cachedImage1;
sf::Image cachedImage2;

// Luma Wipe Cache
std::vector<uint8_t> lumaCache;
bool lumaCacheValid = false;

// --- HELPER FUNCTION: OPEN FILE DIALOG ---
std::string OpenFileDialog(HWND ownerHandle)
{
    OPENFILENAMEA ofn;
    char fileName[MAX_PATH] = "";
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ownerHandle;
    ofn.lpstrFilter = "Image Files\0*.jpg;*.png;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) return std::string(fileName);
    return "";
}

// --- HELPER FUNCTION: SELECT FOLDER DIALOG ---
std::string SelectFolderDialog(HWND ownerHandle)
{
    char path[MAX_PATH];
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = ownerHandle;
    bi.lpszTitle = "Select Destination Folder";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl != 0) {
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
}

// --- GUI STYLING ---
void SetupModernStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 10);
    style.IndentSpacing = 20.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.48f, 0.85f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.44f, 0.37f, 0.80f, 0.40f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.37f, 0.80f, 0.60f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.44f, 0.37f, 0.80f, 0.80f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
}

// --- OPTIMIZED CPU LUMA WIPE (Multithreaded) ---
void ApplyCpuLumaWipeOptimized(const sf::Image& imgA, const sf::Image& imgB, sf::Texture& dstTex, float progress)
{
    sf::Vector2u size = imgA.getSize();
    const std::uint8_t* pA = imgA.getPixelsPtr();
    const std::uint8_t* pB = imgB.getPixelsPtr();

    size_t totalPixels = size.x * size.y;
    if (!lumaCacheValid || lumaCache.size() != totalPixels)
    {
        lumaCache.resize(totalPixels);
        unsigned int numThreads = std::thread::hardware_concurrency();
        unsigned int rowsPerThread = size.y / numThreads;
        std::vector<std::future<void>> futures;

        for (unsigned int i = 0; i < numThreads; ++i) {
            unsigned int startY = i * rowsPerThread;
            unsigned int endY = (i == numThreads - 1) ? size.y : (i + 1) * rowsPerThread;

            futures.push_back(std::async(std::launch::async, [=]() {
                float scaleX = (float)imgB.getSize().x / (float)size.x;
                float scaleY = (float)imgB.getSize().y / (float)size.y;
                int widthB = imgB.getSize().x;

                for (unsigned int y = startY; y < endY; ++y) {
                    for (unsigned int x = 0; x < size.x; ++x) {
                        int xb = (int)(x * scaleX);
                        int yb = (int)(y * scaleY);
                        size_t idxB = (yb * widthB + xb) * 4;

                        uint8_t r = pB[idxB];
                        uint8_t g = pB[idxB + 1];
                        uint8_t b = pB[idxB + 2];
                        lumaCache[y * size.x + x] = (uint8_t)((299 * r + 587 * g + 114 * b) / 1000);
                    }
                }
            }));
        }
        for (auto& f : futures) f.wait();
        lumaCacheValid = true;
    }

    static std::vector<std::uint8_t> resultPixels;
    if (resultPixels.size() != totalPixels * 4) resultPixels.resize(totalPixels * 4);

    int threshold = static_cast<int>((1.0f - progress) * 255.0f);
    unsigned int numThreads = std::thread::hardware_concurrency();
    unsigned int rowsPerThread = size.y / numThreads;
    std::vector<std::future<void>> futures;

    for (unsigned int i = 0; i < numThreads; ++i) {
        unsigned int startY = i * rowsPerThread;
        unsigned int endY = (i == numThreads - 1) ? size.y : (i + 1) * rowsPerThread;

        futures.push_back(std::async(std::launch::async, [=]() {
            float scaleX = (float)imgB.getSize().x / (float)size.x;
            float scaleY = (float)imgB.getSize().y / (float)size.y;
            int widthB = imgB.getSize().x;
            int widthA = size.x;

            for (unsigned int y = startY; y < endY; ++y) {
                size_t offsetA = y * widthA;
                size_t pixelIdx = offsetA * 4;

                for (unsigned int x = 0; x < widthA; ++x) {
                    if (lumaCache[offsetA + x] >= threshold) {
                        int xb = (int)(x * scaleX);
                        int yb = (int)(y * scaleY);
                        size_t idxB = (yb * widthB + xb) * 4;
                        resultPixels[pixelIdx]     = pB[idxB];
                        resultPixels[pixelIdx + 1] = pB[idxB + 1];
                        resultPixels[pixelIdx + 2] = pB[idxB + 2];
                        resultPixels[pixelIdx + 3] = 255;
                    } else {
                        resultPixels[pixelIdx]     = pA[pixelIdx];
                        resultPixels[pixelIdx + 1] = pA[pixelIdx + 1];
                        resultPixels[pixelIdx + 2] = pA[pixelIdx + 2];
                        resultPixels[pixelIdx + 3] = 255;
                    }
                    pixelIdx += 4;
                }
            }
        }));
    }
    for (auto& f : futures) f.wait();
    dstTex.update(resultPixels.data());
}

// --- OPTIMIZED CPU BLUR (Two-Pass Separable + Multithreading) ---
void ApplyCpuBlurOptimized(const sf::Image& src, sf::Texture& dstTex, int radius)
{
    if (radius < 1) {
        dstTex.update(src);
        return;
    }

    sf::Vector2u size = src.getSize();
    const std::uint8_t* pixels = src.getPixelsPtr();

    static std::vector<uint8_t> pass1;
    static std::vector<uint8_t> pass2;
    
    size_t totalSize = size.x * size.y * 4;
    if (pass1.size() != totalSize) { pass1.resize(totalSize); pass2.resize(totalSize); }

    int w = size.x;
    int h = size.y;

    unsigned int numThreads = std::thread::hardware_concurrency();
    std::vector<std::future<void>> futures;
    int rowsPerThread = h / numThreads;

    // PASS 1: HORIZONTAL
    for (unsigned int i = 0; i < numThreads; ++i) {
        int startY = i * rowsPerThread;
        int endY = (i == numThreads - 1) ? h : (i + 1) * rowsPerThread;

        futures.push_back(std::async(std::launch::async, [=]() {
            for (int y = startY; y < endY; ++y) {
                for (int x = 0; x < w; ++x) {
                    int r = 0, g = 0, b = 0, count = 0;
                    int startK = std::max(0, x - radius);
                    int endK = std::min(w - 1, x + radius);
                    for (int k = startK; k <= endK; ++k) {
                        int idx = (y * w + k) * 4;
                        r += pixels[idx]; g += pixels[idx+1]; b += pixels[idx+2]; count++;
                    }
                    int destIdx = (y * w + x) * 4;
                    pass1[destIdx] = r/count; pass1[destIdx+1] = g/count; pass1[destIdx+2] = b/count; pass1[destIdx+3] = 255;
                }
            }
        }));
    }
    for (auto& f : futures) f.wait();
    futures.clear();

    // PASS 2: VERTICAL
    for (unsigned int i = 0; i < numThreads; ++i) {
        int startY = i * rowsPerThread;
        int endY = (i == numThreads - 1) ? h : (i + 1) * rowsPerThread;

        futures.push_back(std::async(std::launch::async, [=]() {
            for (int y = startY; y < endY; ++y) {
                for (int x = 0; x < w; ++x) {
                    int r = 0, g = 0, b = 0, count = 0;
                    int startK = std::max(0, y - radius);
                    int endK = std::min(h - 1, y + radius);
                    for (int k = startK; k <= endK; ++k) {
                        int idx = (k * w + x) * 4;
                        r += pass1[idx]; g += pass1[idx+1]; b += pass1[idx+2]; count++;
                    }
                    int destIdx = (y * w + x) * 4;
                    pass2[destIdx] = r/count; pass2[destIdx+1] = g/count; pass2[destIdx+2] = b/count; pass2[destIdx+3] = 255;
                }
            }
        }));
    }
    for (auto& f : futures) f.wait();
    dstTex.update(pass2.data());
}

// --- CORE RENDERING LOGIC ---
void RenderTransitionFrame(sf::RenderTarget& target, int type, float progress,
    sf::Sprite& s1, sf::Sprite& s2, sf::Texture& t1, sf::Texture& t2,
    const sf::Image& imgCache1, const sf::Image& imgCache2)
{
    float width = 1200.0f;
    float height = 800.0f;

    // Reset state
    if (t1.getSize().x > 0) {
        s1.setColor(sf::Color::White);
        s1.setOrigin({ 0.f, 0.f });
        s1.setPosition({ 0.f, 0.f });
        s1.setRotation(sf::degrees(0.f));
        sf::Vector2u sz1 = t1.getSize();
        s1.setScale({ width / sz1.x, height / sz1.y });
    }
    if (t2.getSize().x > 0) {
        s2.setColor(sf::Color::White);
        s2.setOrigin({ 0.f, 0.f });
        s2.setPosition({ 0.f, 0.f });
        s2.setRotation(sf::degrees(0.f));
        sf::Vector2u sz2 = t2.getSize();
        s2.setScale({ width / sz2.x, height / sz2.y });
    }

    if (t1.getSize().x == 0 || t2.getSize().x == 0) {
        if (t1.getSize().x > 0) target.draw(s1);
        if (t2.getSize().x > 0) target.draw(s2);
        return;
    }

    int drawMode = 0; 
    float xOffset = 0.0f, yOffset = 0.0f;

    switch (type) {
    case 0: // Slide Left
        xOffset = -width * (1.0f - progress);
        s2.setPosition({ xOffset, yOffset });
        break;
    case 1: // Slide Right
        xOffset = width * (1.0f - progress);
        s2.setPosition({ xOffset, yOffset });
        break;
    case 2: // Slide Top
        yOffset = -height * (1.0f - progress);
        s2.setPosition({ xOffset, yOffset });
        break;
    case 3: // Slide Bottom
        yOffset = height * (1.0f - progress);
        s2.setPosition({ xOffset, yOffset });
        break;
    case 4: // Box In
    {
        sf::Vector2u sz2 = t2.getSize();
        s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
        s2.setPosition({ width / 2.f, height / 2.f });
        float tX = width / sz2.x;
        float tY = height / sz2.y;
        s2.setScale({ tX * progress, tY * progress });
    }
    break;
    case 5: // Box Out
    {
        sf::Vector2u sz1 = t1.getSize();
        s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
        s1.setPosition({ width / 2.f, height / 2.f });
        float tX = width / sz1.x;
        float tY = height / sz1.y;
        float sf = 1.0f - progress;
        s1.setScale({ tX * sf, tY * sf });
    }
    break;
    case 6: // Fade to Black
        if (progress <= 0.5f) {
            float lp = progress * 2.0f;
            s1.setColor({ 255, 255, 255, (std::uint8_t)(255 * (1.0f - lp)) });
            s2.setColor({ 255, 255, 255, 0 });
        }
        else {
            float lp = (progress - 0.5f) * 2.0f;
            s1.setColor({ 255, 255, 255, 0 });
            s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * lp) });
        }
        break;
    case 7: // Cross-Fade
        s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * progress) });
        s2.setPosition({ 0.f, 0.f });
        break;
    case 8: // Page Turn H
    {
        sf::Vector2u sz1 = t1.getSize(); sf::Vector2u sz2 = t2.getSize();
        s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
        s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
        s1.setPosition({ width / 2.f, height / 2.f });
        s2.setPosition({ width / 2.f, height / 2.f });
        if (progress <= 0.5f) {
            drawMode = 1;
            float sf = 1.0f - (progress * 2.0f);
            s1.setScale({ (width / sz1.x) * sf, height / sz1.y });
        } else {
            drawMode = 2;
            float sf = (progress - 0.5f) * 2.0f;
            s2.setScale({ (width / sz2.x) * sf, height / sz2.y });
        }
    }
    break;
    case 9: // Page Turn V
    {
        sf::Vector2u sz1 = t1.getSize(); sf::Vector2u sz2 = t2.getSize();
        s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
        s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
        s1.setPosition({ width / 2.f, height / 2.f });
        s2.setPosition({ width / 2.f, height / 2.f });
        if (progress <= 0.5f) {
            drawMode = 1;
            float sf = 1.0f - (progress * 2.0f);
            s1.setScale({ width / sz1.x, (height / sz1.y) * sf });
        } else {
            drawMode = 2;
            float sf = (progress - 0.5f) * 2.0f;
            s2.setScale({ width / sz2.x, (height / sz2.y) * sf });
        }
    }
    break;
    case 10: // Shutter Open
    {
        sf::Vector2u sz1 = t1.getSize();
        s1.setOrigin({ (float)sz1.x, (float)sz1.y / 2.f });
        s1.setPosition({ width, height / 2.f });
        s1.setScale({ (width / sz1.x) * (1.0f - progress), height / sz1.y });
        float ex = -width * (1.0f - progress);
        s2.setPosition({ ex, 0.0f });
    }
    break;
    case 11: // Blur Fade
    {
        static sf::Texture tempTex1, tempTex2;
        // FIX: Replaced create() with resize() for SFML 3.0
        if (tempTex1.getSize() != imgCache1.getSize()) tempTex1.resize(imgCache1.getSize());
        if (tempTex2.getSize() != imgCache2.getSize()) tempTex2.resize(imgCache2.getSize());

        int maxBlur = 12;
        int currentBlur = 0;

        if (progress <= 0.45f) {
            currentBlur = (int)(progress * 2.2f * maxBlur);
            ApplyCpuBlurOptimized(imgCache1, tempTex1, currentBlur);
            sf::Sprite tempSprite(tempTex1);
            sf::Vector2u sz = tempTex1.getSize();
            tempSprite.setScale({ 1200.0f / sz.x, 800.0f / sz.y }); 
            target.draw(tempSprite);
        }
        else if (progress >= 0.55f) {
            float localP = (progress - 0.55f) / 0.45f;
            currentBlur = (int)((1.0f - localP) * maxBlur);
            ApplyCpuBlurOptimized(imgCache2, tempTex2, currentBlur);
            sf::Sprite tempSprite(tempTex2);
            sf::Vector2u sz = tempTex2.getSize();
            tempSprite.setScale({ 1200.0f / sz.x, 800.0f / sz.y });
            target.draw(tempSprite);
        }
        else {
            ApplyCpuBlurOptimized(imgCache1, tempTex1, maxBlur);
            ApplyCpuBlurOptimized(imgCache2, tempTex2, maxBlur);
            sf::Sprite sA(tempTex1);
            sf::Sprite sB(tempTex2);
            sf::Vector2u sz1 = tempTex1.getSize();
            sf::Vector2u sz2 = tempTex2.getSize();
            sA.setScale({ 1200.0f / sz1.x, 800.0f / sz1.y });
            sB.setScale({ 1200.0f / sz2.x, 800.0f / sz2.y });
            float mix = (progress - 0.45f) * 10.0f; 
            sA.setColor({ 255, 255, 255, (std::uint8_t)(255 * (1.0f - mix)) });
            sB.setColor({ 255, 255, 255, (std::uint8_t)(255 * mix) });
            target.draw(sA);
            target.draw(sB);
        }
        return;
    }
    break;
    case 12: // Cube Rotate
    {
        t1.setSmooth(true);
        t2.setSmooth(true);
        float cx = width / 2.f, cy = height / 2.f, fov = 800.f;
        const int STRIPS = 96;
        float angle = progress * 1.5707963f;

        sf::Vector2f scale1 = s1.getScale(), scale2 = s2.getScale();
        float faceW = t1.getSize().x * scale1.x;
        float faceH = t1.getSize().y * scale1.y;
        float cubeDepth = t2.getSize().x * scale2.x;
        float halfD = cubeDepth / 2.0f;

        auto transformPoint = [&](sf::Vector3f p) -> sf::Vector3f {
            float pz = p.z - halfD, px = p.x;
            float c = std::cos(angle), s = std::sin(angle);
            return { px * c + pz * s, p.y, -px * s + pz * c + halfD };
        };
        auto project = [&](sf::Vector3f p) -> sf::Vector2f {
            float scale = fov / (fov + p.z);
            return { cx + p.x * scale, cy + p.y * scale };
        };
        auto getShade = [&](float baseAngle) -> sf::Color {
            float currentAngle = std::abs(baseAngle - std::abs(progress * 90.0f));
            float rad = currentAngle * 0.017453f;
            float light = std::cos(rad);
            if (light < 0) light = 0;
            float brightness = 0.6f + (light * 0.4f);
            std::uint8_t val = static_cast<std::uint8_t>(255 * brightness);
            return sf::Color(val, val, val);
        };

        sf::Color shade1 = getShade(0.0f), shade2 = getShade(90.0f);

        auto drawStripMesh = [&](sf::Texture& tex, sf::Color col, bool isSideFace) {
            sf::VertexArray va(sf::PrimitiveType::TriangleStrip, (STRIPS + 1) * 2);
            float localW = faceW, localH = faceH;
            float startX = -localW / 2.0f, yTop = -localH / 2.0f, yBot = localH / 2.0f;
            for (int i = 0; i <= STRIPS; ++i) {
                float u = (float)i / STRIPS;
                sf::Vector3f pTop, pBot;
                if (!isSideFace) {
                    float x = startX + (u * localW);
                    pTop = { x, yTop, 0.0f }; pBot = { x, yBot, 0.0f };
                } else {
                    float z = u * cubeDepth; float fixedX = localW / 2.0f;
                    pTop = { fixedX, yTop, z }; pBot = { fixedX, yBot, z };
                }
                pTop = transformPoint(pTop); pBot = transformPoint(pBot);
                sf::Vector2f sTop = project(pTop), sBot = project(pBot);
                float tx = u * tex.getSize().x, tyTop = 0.0f, tyBot = (float)tex.getSize().y;
                int idx = i * 2;
                va[idx].position = sTop; va[idx].texCoords = { tx, tyTop }; va[idx].color = col;
                va[idx + 1].position = sBot; va[idx + 1].texCoords = { tx, tyBot }; va[idx + 1].color = col;
            }
            sf::RenderStates rs; rs.texture = &tex;
            target.draw(va, rs);
        };

        target.clear(sf::Color::Black);
        sf::Vector3f tf = transformPoint({ 0.f, 0.f, 0.f });
        sf::Vector3f ts = transformPoint({ faceW / 2.f, 0.f, cubeDepth / 2.f });
        if (tf.z > ts.z) { drawStripMesh(t1, shade1, false); drawStripMesh(t2, shade2, true); }
        else { drawStripMesh(t2, shade2, true); drawStripMesh(t1, shade1, false); }
        return;
    }
    case 13: // Ring
    {
        float cx = width / 2.f, cy = height / 2.f;
        float radius = 1000.f, depth = 670.f;
        float a1 = progress * 1.5707963f, a2 = (1.0f - progress) * 1.5707963f;
        auto ringPos = [&](float angle, float sideSign) {
            float x = sideSign * (radius - std::cos(angle) * radius);
            float z = std::sin(angle) * radius;
            float s = depth / (depth + z);
            return std::tuple<float, float, float>(x, z, s);
        };

        auto [x1, z1, sC1] = ringPos(a1, +1.f);
        sf::VertexArray quad1(sf::PrimitiveType::Triangles, 6);
        float w1 = width * sC1, h1 = height * sC1;
        float left1 = cx + x1 - w1 / 2.f, top1 = cy - h1 / 2.f;
        float right1 = left1 + w1, bottom1 = top1 + h1;
        quad1[0].position = { left1, top1 }; quad1[1].position = { right1, top1 }; quad1[2].position = { right1, bottom1 };
        quad1[3].position = { left1, top1 }; quad1[4].position = { right1, bottom1 }; quad1[5].position = { left1, bottom1 };
        quad1[0].texCoords = { 0.f, 0.f }; quad1[1].texCoords = { (float)t1.getSize().x, 0.f }; quad1[2].texCoords = { (float)t1.getSize().x, (float)t1.getSize().y };
        quad1[3].texCoords = { 0.f, 0.f }; quad1[4].texCoords = { (float)t1.getSize().x, (float)t1.getSize().y }; quad1[5].texCoords = { 0.f, (float)t1.getSize().y };

        auto [x2, z2, sC2] = ringPos(a2, -1.f);
        sf::VertexArray quad2(sf::PrimitiveType::Triangles, 6);
        float w2 = width * sC2, h2 = height * sC2;
        float left2 = cx + x2 - w2 / 2.f, top2 = cy - h2 / 2.f;
        float right2 = left2 + w2, bottom2 = top2 + h2;
        quad2[0].position = { left2, top2 }; quad2[1].position = { right2, top2 }; quad2[2].position = { right2, bottom2 };
        quad2[3].position = { left2, top2 }; quad2[4].position = { right2, bottom2 }; quad2[5].position = { left2, bottom2 };
        quad2[0].texCoords = { 0.f, 0.f }; quad2[1].texCoords = { (float)t2.getSize().x, 0.f }; quad2[2].texCoords = { (float)t2.getSize().x, (float)t2.getSize().y };
        quad2[3].texCoords = { 0.f, 0.f }; quad2[4].texCoords = { (float)t2.getSize().x, (float)t2.getSize().y }; quad2[5].texCoords = { 0.f, (float)t2.getSize().y };

        target.clear(sf::Color::Black);
        sf::RenderStates rs1; rs1.texture = &t1;
        sf::RenderStates rs2; rs2.texture = &t2;
        if (z1 > z2) { target.draw(quad1, rs1); target.draw(quad2, rs2); }
        else { target.draw(quad2, rs2); target.draw(quad1, rs1); }
        return;
    }
    case 14: // Luma Wipe
    {
        static sf::Texture resultTex;
        // FIX: Replaced create() with resize() for SFML 3.0
        if (resultTex.getSize() != imgCache1.getSize()) {
            resultTex.resize(imgCache1.getSize());
        }
        ApplyCpuLumaWipeOptimized(imgCache1, imgCache2, resultTex, progress);
        sf::Sprite s(resultTex);
        sf::Vector2u sz = resultTex.getSize();
        s.setScale({ 1200.0f / sz.x, 800.0f / sz.y });
        target.draw(s);
        return;
    }
    break;
    case 15: // Fly Away
    {
        sf::Vector2u sz1 = t1.getSize(); sf::Vector2u sz2 = t2.getSize();
        sf::Vector2f center(width / 2.f, height / 2.f);
        if (progress <= 0.5f) {
            float lp = progress * 2.0f, invLp = 1.0f - lp;
            s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
            s1.setPosition(center);
            s1.setScale({ (width / sz1.x) * invLp, (height / sz1.y) * invLp });
            s1.setRotation(sf::degrees(lp * 180.0f));
            s1.setColor({ 255, 255, 255, (std::uint8_t)(255 * invLp) });
            target.clear(sf::Color::Black); target.draw(s1);
        } else {
            float lp = (progress - 0.5f) * 2.0f;
            s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
            s2.setPosition(center);
            s2.setScale({ (width / sz2.x) * lp, (height / sz2.y) * lp });
            s2.setRotation(sf::degrees((1.0f - lp) * -180.0f));
            s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * lp) });
            target.clear(sf::Color::Black); target.draw(s2);
        }
        return;
    }
    }

    target.clear(sf::Color::Black);
    if (type == 5 || type == 10) { target.draw(s2); target.draw(s1); }
    else if (drawMode == 1) target.draw(s1);
    else if (drawMode == 2) target.draw(s2);
    else { target.draw(s1); target.draw(s2); }
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({ 1200, 800 }), "Project 28: Ultimate Transitions", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    std::ignore = ImGui::SFML::Init(window);
    SetupModernStyle();

    sf::Texture texture1, texture2;
    sf::Sprite sprite1(texture1);
    sf::Sprite sprite2(texture2);

    float progress = 0.0f; 
    int transitionType = 0; 
    int framesCount = 60;   

    std::string outputFolderPath = (fs::current_path() / "SavedAnimation").string();

    const char* transitionNames[] = {
        "Slide Left", "Slide Right", "Slide Top", "Slide Bottom",
        "Box In", "Box Out", "Fade to Black", "Cross-Fade",
        "Page Turn Horizontal", "Page Turn Vertical", "Shutter Open",
        "Blur Fade", "3D Cube Rotation", "Ring", "Luma Wipe", "Fly Away"
    };

    sf::Clock deltaClock;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::SetNextWindowPos(ImVec2(820, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 760), ImGuiCond_FirstUseEver);

        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse);
        ImGui::TextDisabled("MEDIA LIBRARY");
        ImGui::Separator();

        if (ImGui::Button(" Select Image 1 ", ImVec2(150, 40))) {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            if (!path.empty() && texture1.loadFromFile(path)) {
                sprite1.setTexture(texture1, true);
                cachedImage1 = texture1.copyToImage(); 
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(" Select Image 2 ", ImVec2(150, 40))) {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            if (!path.empty() && texture2.loadFromFile(path)) {
                sprite2.setTexture(texture2, true);
                cachedImage2 = texture2.copyToImage();
                lumaCacheValid = false; 
            }
        }

        ImGui::Spacing();
        if (texture1.getSize().x > 0) ImGui::Image(texture1, { 140.f, 100.f });
        else ImGui::Button("Empty 1", { 140.f, 100.f });
        ImGui::SameLine();
        if (texture2.getSize().x > 0) ImGui::Image(texture2, { 140.f, 100.f });
        else ImGui::Button("Empty 2", { 140.f, 100.f });

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextDisabled("ANIMATION CONTROLS");
        ImGui::Spacing();
        ImGui::Text("Preview Progress:");
        ImGui::SliderFloat("##progress", &progress, 0.0f, 1.0f, "%.2f");
        ImGui::Text("Mode:");
        ImGui::Combo("##type", &transitionType, transitionNames, IM_ARRAYSIZE(transitionNames));

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::TextDisabled("EXPORT SETTINGS");
        ImGui::Spacing();
        ImGui::Text("Output Folder:");
        ImGui::TextWrapped("%s", outputFolderPath.c_str());

        if (ImGui::Button(" Change Folder... ", ImVec2(150, 30))) {
            std::string newPath = SelectFolderDialog((HWND)window.getNativeHandle());
            if (!newPath.empty()) outputFolderPath = newPath;
        }

        ImGui::Spacing();
        ImGui::Text("Total Frames:");
        ImGui::InputInt("##frames", &framesCount);
        if (framesCount < 10) framesCount = 10;   
        if (framesCount > 1000) framesCount = 1000; 

        ImGui::Spacing();

        if (ImGui::Button(" RENDER & SAVE SEQUENCE ", ImVec2(320, 50)))
        {
            if (texture1.getSize().x > 0 && texture2.getSize().x > 0)
            {
                fs::path folderPath = outputFolderPath;
                if (!fs::exists(folderPath)) {
                    try { fs::create_directories(folderPath); } catch (...) {}
                }

                sf::RenderTexture renderTex;
                // FIX: Replaced create() with resize() for SFML 3.0
                renderTex.resize({ 1200, 800 });

                for (int i = 0; i <= framesCount; i++)
                {
                    float p = (float)i / (float)framesCount;
                    RenderTransitionFrame(renderTex, transitionType, p, sprite1, sprite2, texture1, texture2, cachedImage1, cachedImage2);
                    renderTex.display();

                    std::stringstream ss;
                    ss << folderPath.string() << "/frame_" << std::setw(3) << std::setfill('0') << i << ".png";
                    sf::Image img = renderTex.getTexture().copyToImage();
                    img.saveToFile(ss.str());
                }
                ShellExecuteA(NULL, "open", folderPath.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
            }
            else { ImGui::OpenPopup("ErrorNoImages"); }
        }

        if (ImGui::BeginPopup("ErrorNoImages")) {
            ImGui::Text("Error: Please select Image 1 and Image 2 first!");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End();

        RenderTransitionFrame(window, transitionType, progress, sprite1, sprite2, texture1, texture2, cachedImage1, cachedImage2);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}