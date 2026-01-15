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

sf::Image ResizeImageCPU(const sf::Image& original, unsigned int targetW, unsigned int targetH) {
    // In SFML 3.0, we initialize the image size directly in the constructor
    sf::Image resized(sf::Vector2u{ targetW, targetH }, sf::Color::Transparent);

    sf::Vector2u origSize = original.getSize();

    // Scaling factors based on the ratio between original and target sizes
    float scaleX = static_cast<float>(origSize.x) / targetW;
    float scaleY = static_cast<float>(origSize.y) / targetH;

    for (unsigned int y = 0; y < targetH; ++y) {
        for (unsigned int x = 0; x < targetW; ++x) {
            // Map the coordinates of the target image back to the original image
            unsigned int origX = static_cast<unsigned int>(x * scaleX);
            unsigned int origY = static_cast<unsigned int>(y * scaleY);

            // Bounds safety check
            if (origX < origSize.x && origY < origSize.y) {
                resized.setPixel({ x, y }, original.getPixel({ origX, origY }));
            }
        }
    }
    return resized;
}

// --- OPTIMIZED CPU LUMA WIPE (Multithreaded) ---
void ApplyCpuLumaWipeOptimized(const sf::Image& imgA, const sf::Image& imgB, sf::Texture& dstTex, float progress)
{
    sf::Vector2u size = imgA.getSize();
    size_t totalPixels = static_cast<size_t>(size.x) * size.y;

    if (totalPixels == 0) return;

    static sf::Vector2u lastSize = { 0, 0 };

    if (!lumaCacheValid || lastSize != size) {
        lumaCache.resize(totalPixels);
        const uint8_t* pB = imgB.getPixelsPtr();

        for (size_t i = 0; i < totalPixels; ++i) {
            size_t idx = i * 4;
            lumaCache[i] = (uint8_t)((299 * pB[idx] + 587 * pB[idx + 1] + 114 * pB[idx + 2]) / 1000);
        }
        lumaCacheValid = true;
        lastSize = size;
    }

    static std::vector<uint8_t> resultPixels;
    if (resultPixels.size() != totalPixels * 4) resultPixels.resize(totalPixels * 4);

    const uint8_t* pA = imgA.getPixelsPtr();
    const uint8_t* pB = imgB.getPixelsPtr();

    int threshold = static_cast<int>((1.0f - (progress * 1.1f)) * 255.0f);

    for (size_t i = 0; i < totalPixels; ++i) {
        size_t pixelIdx = i * 4;

        if (static_cast<int>(lumaCache[i]) >= threshold) {
            resultPixels[pixelIdx] = pB[pixelIdx];
            resultPixels[pixelIdx + 1] = pB[pixelIdx + 1];
            resultPixels[pixelIdx + 2] = pB[pixelIdx + 2];
        }
        else {
            resultPixels[pixelIdx] = pA[pixelIdx];
            resultPixels[pixelIdx + 1] = pA[pixelIdx + 1];
            resultPixels[pixelIdx + 2] = pA[pixelIdx + 2];
        }
        resultPixels[pixelIdx + 3] = 255; 
    }

    if (dstTex.getSize() != size) {
        dstTex.resize(size);
    }
    dstTex.update(resultPixels.data());
}

// OPTIMIZED CPU BLUR 
void ApplyCpuBlurOptimized(const sf::Image& src, sf::Texture& dstTex, int radius)
{
    // If radius is 0, we just show the original image
    if (radius < 1) {
        // Ensure texture size matches the original image before updating
        if (dstTex.getSize() != src.getSize()) {
            dstTex.resize(src.getSize());
        }
        dstTex.update(src);
        return;
    }

    sf::Vector2u orgSize = src.getSize();
    const int SCALE = 4;
    sf::Vector2u smallSize(orgSize.x / SCALE, orgSize.y / SCALE);

    // Safety check: avoid processing if image is too small
    if (smallSize.x < 1 || smallSize.y < 1) return;

    // 1. Downsample logic
    static std::vector<uint8_t> smallPixels;
    if (smallPixels.size() != smallSize.x * smallSize.y * 4)
        smallPixels.resize(smallSize.x * smallSize.y * 4);

    const uint8_t* srcPixels = src.getPixelsPtr();

    for (unsigned int y = 0; y < smallSize.y; ++y) {
        for (unsigned int x = 0; x < smallSize.x; ++x) {
            int srcIdx = ((y * SCALE) * orgSize.x + (x * SCALE)) * 4;
            int dstIdx = (y * smallSize.x + x) * 4;
            std::memcpy(&smallPixels[dstIdx], &srcPixels[srcIdx], 4);
        }
    }

    // 2. Separable Blur on small buffer
    int smallRadius = std::max(1, radius / SCALE);
    static std::vector<uint8_t> tempBuffer;
    if (tempBuffer.size() != smallPixels.size()) tempBuffer.resize(smallPixels.size());

    int w = smallSize.x;
    int h = smallSize.y;

    // Horizontal Pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int r = 0, g = 0, b = 0, count = 0;
            for (int k = -smallRadius; k <= smallRadius; ++k) {
                int nx = std::max(0, std::min(w - 1, x + k));
                int idx = (y * w + nx) * 4;
                r += smallPixels[idx]; g += smallPixels[idx + 1]; b += smallPixels[idx + 2];
                count++;
            }
            int outIdx = (y * w + x) * 4;
            tempBuffer[outIdx] = r / count; tempBuffer[outIdx + 1] = g / count; tempBuffer[outIdx + 2] = b / count; tempBuffer[outIdx + 3] = 255;
        }
    }

    // Vertical Pass
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int r = 0, g = 0, b = 0, count = 0;
            for (int k = -smallRadius; k <= smallRadius; ++k) {
                int ny = std::max(0, std::min(h - 1, y + k));
                int idx = (ny * w + x) * 4;
                r += tempBuffer[idx]; g += tempBuffer[idx + 1]; b += tempBuffer[idx + 2];
                count++;
            }
            int outIdx = (y * w + x) * 4;
            smallPixels[outIdx] = r / count; smallPixels[outIdx + 1] = g / count; smallPixels[outIdx + 2] = b / count; smallPixels[outIdx + 3] = 255;
        }
    }

    // 3. Update Texture: ensure it matches the SMALL size
    if (dstTex.getSize() != smallSize) {
        dstTex.resize(smallSize);
    }
    dstTex.update(smallPixels.data());
}

// --- CORE RENDERING LOGIC ---
void RenderTransitionFrame(sf::RenderTarget& target, int type, float progress,
    sf::Sprite& s1, sf::Sprite& s2, sf::Texture& t1, sf::Texture& t2,
    const sf::Image& imgCache1, const sf::Image& imgCache2)
{
    target.clear(sf::Color::Black);

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
    case 11: // Blur Fade transition
    {
        static sf::Texture tempTex1, tempTex2;

        int maxBlur = 12; // Maximum blur radius
        int currentBlur = 0;

        // Phase 1: Blur the first image (0% to 45% of progress)
        if (progress <= 0.45f) {
            // Calculate growing blur radius
            currentBlur = (int)(progress * (1.0f / 0.45f) * maxBlur);

            ApplyCpuBlurOptimized(imgCache1, tempTex1, currentBlur);

            sf::Sprite tempSprite(tempTex1);
            // Dynamically calculate scale because tempTex1 is now 4x smaller than original
            sf::Vector2u sz = tempTex1.getSize();
            tempSprite.setScale({ 1200.0f / (float)sz.x, 800.0f / (float)sz.y });

            target.draw(tempSprite);
        }
        // Phase 3: Un-blur the second image (55% to 100% of progress)
        else if (progress >= 0.55f) {
            // Calculate decreasing blur radius
            float localP = (progress - 0.55f) / 0.45f;
            currentBlur = (int)((1.0f - localP) * maxBlur);

            ApplyCpuBlurOptimized(imgCache2, tempTex2, currentBlur);

            sf::Sprite tempSprite(tempTex2);
            // Adjust scale to fit the 1200x800 window regardless of downsampling
            sf::Vector2u sz = tempTex2.getSize();
            tempSprite.setScale({ 1200.0f / (float)sz.x, 800.0f / (float)sz.y });

            target.draw(tempSprite);
        }
        // Phase 2: Cross-fade between two blurred images (45% to 55% of progress)
        else {
            // Both images are blurred at maximum radius
            ApplyCpuBlurOptimized(imgCache1, tempTex1, maxBlur);
            ApplyCpuBlurOptimized(imgCache2, tempTex2, maxBlur);

            sf::Sprite sA(tempTex1);
            sf::Sprite sB(tempTex2);

            // Apply scales for both sprites
            sA.setScale({ 1200.0f / (float)tempTex1.getSize().x, 800.0f / (float)tempTex1.getSize().y });
            sB.setScale({ 1200.0f / (float)tempTex2.getSize().x, 800.0f / (float)tempTex2.getSize().y });

            // Calculate alpha blending (mix) factor for the cross-fade
            float mix = (progress - 0.45f) * 10.0f; // Maps 0.45-0.55 range to 0.0-1.0

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
    case 14: // Luma Wipe transition
    {
        static sf::Texture resultTex;
        if (t1.getSize().x == 0 || t2.getSize().x == 0) return;

        // 1. Create working copies (non-const) from textures
        sf::Image workingImg1 = imgCache1;
        sf::Image workingImg2 = imgCache2;

        sf::Vector2u size1 = workingImg1.getSize();
        sf::Vector2u size2 = workingImg2.getSize();

        // 2. Logic: If sizes differ, resize both to the smallest common dimensions
        if (size1 != size2) {
            unsigned int minW = std::min(size1.x, size2.x);
            unsigned int minH = std::min(size1.y, size2.y);

            workingImg1 = ResizeImageCPU(workingImg1, minW, minH);
            workingImg2 = ResizeImageCPU(workingImg2, minW, minH);
        }

        // 3. Process the transition on CPU (no shaders used as per )
        ApplyCpuLumaWipeOptimized(workingImg1, workingImg2, resultTex, progress);

        // 4. Final display
        sf::Sprite s(resultTex);
        // Stretch the result to fill our standard 1200x800 canvas [cite: 11]
        s.setScale({ 1200.0f / resultTex.getSize().x, 800.0f / resultTex.getSize().y });

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

    // --- FPS COUNTER VARIABLES ---
    sf::Clock fpsClock;       // Clock to measure elapsed time per frame
    float fpsValue = 0;       // Current FPS value
    int frameCounter = 0;     // Counter for frames in the current second
    sf::Time timeSinceLastUpdate = sf::Time::Zero; // Accumulated time

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

        // Update FPS counter
        sf::Time dt = fpsClock.restart();
        timeSinceLastUpdate += dt;
        frameCounter++;

        if (timeSinceLastUpdate.asSeconds() >= 1.0f) {
            fpsValue = (float)frameCounter; // FPS is the number of frames per 1 second
            frameCounter = 0;
            timeSinceLastUpdate -= sf::seconds(1.0f);
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::SetNextWindowPos(ImVec2(820, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 780), ImGuiCond_FirstUseEver);

        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

        // CLAMPING
        ImVec2 pos = ImGui::GetWindowPos();   
        ImVec2 size = ImGui::GetWindowSize(); 
        float winW = 1200.0f; 
        float winH = 800.0f;  

        bool moved = false;

        if (pos.x < 0) { pos.x = 0; moved = true; }
        if (pos.y < 0) { pos.y = 0; moved = true; }

        if (pos.x + size.x > winW) { pos.x = winW - size.x; moved = true; }
        if (pos.y + size.y > winH) { pos.y = winH - size.y; moved = true; }

        if (moved) {
            ImGui::SetWindowPos(pos);
        }

        ImGui::TextDisabled("MEDIA LIBRARY");
        ImGui::Separator();

        if (ImGui::Button(" Select Image 1 ", ImVec2(150, 40))) {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            if (!path.empty() && texture1.loadFromFile(path)) {
                sprite1.setTexture(texture1, true);
                cachedImage1 = texture1.copyToImage(); 
                lumaCacheValid = false;
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

        // Display Performance Info
        ImGui::TextDisabled("PERFORMANCE");
        ImGui::Text("Current FPS: %.1f", fpsValue);

        // Add a color indicator: Green if FPS > 50, Yellow if > 25, Red if lower
        if (fpsValue > 50)
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Status: Excellent");
        else if (fpsValue > 25)
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "Status: Playable");
        else
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Status: Slow (CPU Bottleneck)");

        ImGui::Separator();

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