#include "pch.h"
#include <iostream>
#include <cstdint>    // Required for std::uint8_t (Strict types in SFML 3.0)
#include <filesystem> // C++17 library for creating folders and handling paths
#include <sstream>    // Required for string stream manipulations (creating filenames)
#include <iomanip>    // Required for std::setw, std::setfill (formatting numbers like 001, 002)

// Create an alias for std::filesystem to save typing
namespace fs = std::filesystem;

// --- SHADER CODE: LUMA WIPE (BRIGHTNESS TRANSITION) ---
// This GLSL shader calculates the brightness of each pixel.
// Bright pixels appear first (at low progress), dark pixels appear last.
const std::string lumaShaderCode = R"(
    uniform sampler2D texture;
    uniform float progress; // Value from 0.0 to 1.0

    void main() {
        // Get the coordinate of the current pixel
        vec2 coord = gl_TexCoord[0].xy;
        
        // Get the color of the pixel from the texture
        vec4 pixel = texture2D(texture, coord);
        
        // Calculate Luminance (Perceived Brightness)
        // We use standard Rec. 601 coefficients: Red 30%, Green 59%, Blue 11%
        float luma = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
        
        // Logic: 
        // We want bright pixels (luma ~1.0) to appear EARLY (when progress is low).
        // We want dark pixels (luma ~0.0) to appear LATE (when progress is high).
        
        // Threshold moves from 1.0 down to 0.0 as progress goes 0.0 -> 1.0
        float threshold = 1.0 - progress;
        
        // Calculate Alpha (Transparency)
        // If pixel_brightness > threshold, alpha becomes 1.0 (Visible).
        // smoothstep is used to create a soft edge instead of a harsh pixelated line.
        float alpha = smoothstep(threshold, threshold + 0.1, luma);
        
        // Output the pixel with the modified alpha
        gl_FragColor = vec4(pixel.rgb, pixel.a * alpha);
    }
)";


// --- WINDOWS API BLOCK ---
// We need Windows API for file/folder dialogs and opening Explorer.
#define NOMINMAX      // Prevents Windows headers from defining min/max macros that conflict with std::min/std::max
#include <Windows.h>
#include <commdlg.h>  // Standard Open File Dialog library
#include <shellapi.h> // For ShellExecute (to open folder in Windows Explorer)
#include <shlobj.h>   // For SHBrowseForFolder (Folder selection dialog)

// --- HELPER FUNCTION: OPEN FILE DIALOG ---
// Opens a native Windows dialog to select an image file.
std::string OpenFileDialog(HWND ownerHandle)
{
    OPENFILENAMEA ofn; // Structure containing dialog settings
    char fileName[MAX_PATH] = ""; // Buffer to store the result

    // Initialize memory with zeros
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ownerHandle; // Attach dialog to our SFML window so it blocks interaction until closed
    // Filter determines which files are visible (Images only)
    ofn.lpstrFilter = "Image Files\0*.jpg;*.png;*.bmp;*.tga\0All Files\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST; // Ensure the user picks a real file

    // If the user clicks "Open", return the path. Otherwise, return an empty string.
    if (GetOpenFileNameA(&ofn)) return std::string(fileName);
    return "";
}

// --- HELPER FUNCTION: SELECT FOLDER DIALOG ---
// Opens a native Windows dialog to select a destination directory.
std::string SelectFolderDialog(HWND ownerHandle)
{
    char path[MAX_PATH];
    BROWSEINFOA bi = { 0 };
    bi.hwndOwner = ownerHandle;
    bi.lpszTitle = "Select Destination Folder";
    // Flags: Only allow file system directories, use the "New Style" dialog
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    // Open the dialog
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);

    if (pidl != 0)
    {
        // Convert the obscure "Item ID List" to a readable string path
        if (SHGetPathFromIDListA(pidl, path))
        {
            CoTaskMemFree(pidl); // IMPORTANT: Free memory allocated by Windows
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
}

// --- GUI STYLING ---
// Sets up a "Deep Dark" theme for ImGui with rounded corners.
void SetupModernStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // Rounding makes the UI look modern (like Windows 11 or macOS)
    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;

    // Spacing makes the UI less cramped
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 10);
    style.IndentSpacing = 20.0f;

    // Define the Dark Color Palette
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f); // Dark Grey Background
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f); // Purple accent on click
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.48f, 0.85f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.44f, 0.37f, 0.80f, 0.40f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.37f, 0.80f, 0.60f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.44f, 0.37f, 0.80f, 0.80f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f); // Almost white text
}

// --- SHADER ROZMYCIA (BLUR) ---
const std::string blurShaderCode = R"(
    uniform sampler2D texture;
    uniform float blurRadius; // 0.0 do 1.0

    void main() {
        vec2 texCoord = gl_TexCoord[0].xy;
        vec4 pixel = texture2D(texture, texCoord);
        
        if (blurRadius < 0.001) {
            gl_FragColor = pixel * gl_Color;
            return;
        }

        vec4 sum = vec4(0.0);
        float weightSum = 0.0;
        
        // ZMIANA KRYTYCZNA: Ekstremalnie mały krok (0.00005).
        // To skleja próbki w jednolitą masę.
        float spread = blurRadius * 0.00005; 

        // Pętla 17x17 (289 próbek na piksel)
        for (float x = -8.0; x <= 8.0; x += 1.0) {
            for (float y = -8.0; y <= 8.0; y += 1.0) {
                float weight = exp(-(x*x + y*y) / 40.0);
                vec2 offset = vec2(x, y) * spread;
                sum += texture2D(texture, texCoord + offset) * weight;
                weightSum += weight;
            }
        }
        
        gl_FragColor = (sum / weightSum) * gl_Color;
    }
)";

// --- CORE RENDERING LOGIC ---
// This function calculates the mathematics for every transition.
// It is designed to work with ANY RenderTarget (Window for preview, or RenderTexture for saving).
// Params:
// - target: Where to draw?
// - type: Which transition effect?
// - progress: 0.0 (start) to 1.0 (end)
// - s1, s2: The sprites
// - t1, t2: The textures (needed to check sizes)
void RenderTransitionFrame(sf::RenderTarget& target, int type, float progress,
    sf::Sprite& s1, sf::Sprite& s2, sf::Texture& t1, sf::Texture& t2)
{
    float width = 1200.0f; // Canvas width
    float height = 800.0f; // Canvas height

    // 1. STATE RESET (CRITICAL STEP)
    // SFML Sprites are "stateful". If we rotated Sprite1 in the previous frame, 
    // it stays rotated in this frame unless we reset it.
    // We must reset Color, Origin, Position, and Scale before calculating the current frame.
    if (t1.getSize().x > 0) {
        s1.setColor(sf::Color::White); // Reset Alpha/Opacity to 100%
        s1.setOrigin({ 0.f, 0.f });    // Reset Pivot Point to top-left
        s1.setPosition({ 0.f, 0.f });  // Reset Position to top-left
        s1.setRotation(sf::degrees(0.f)); // Reset Rotation
        sf::Vector2u sz1 = t1.getSize();
        s1.setScale({ width / sz1.x, height / sz1.y }); // Fit to screen
    }
    if (t2.getSize().x > 0) {
        s2.setColor(sf::Color::White);
        s2.setOrigin({ 0.f, 0.f });
        s2.setPosition({ 0.f, 0.f });
        s2.setRotation(sf::degrees(0.f));
        sf::Vector2u sz2 = t2.getSize();
        s2.setScale({ width / sz2.x, height / sz2.y });
    }

    // Safety check: if images are missing, do not crash, just draw what we have
    if (t1.getSize().x == 0 || t2.getSize().x == 0) {
        if (t1.getSize().x > 0) target.draw(s1);
        if (t2.getSize().x > 0) target.draw(s2);
        return;
    }

    // 2. MATH LOGIC
    int drawMode = 0; // 0=Draw Both, 1=Draw Only Sprite 1, 2=Draw Only Sprite 2
    float xOffset = 0.0f, yOffset = 0.0f;


    switch (type) {
    case 0: // Slide Left
        // Sprite 2 moves from Right (width) to Center (0)
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

    case 4: // Box In (Zoom In)
    {
        // Sprite 2 grows from the center of the screen
        sf::Vector2u sz2 = t2.getSize();
        // Set Origin to center of the image
        s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
        // Set Position to center of the screen
        s2.setPosition({ width / 2.f, height / 2.f });

        // Calculate scale factor based on progress (0.0 to 1.0)
        float tX = width / sz2.x;
        float tY = height / sz2.y;
        s2.setScale({ tX * progress, tY * progress });
    }
    break;

    case 5: // Box Out (Zoom Out)
    {
        // Sprite 1 shrinks into the center
        sf::Vector2u sz1 = t1.getSize();
        s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
        s1.setPosition({ width / 2.f, height / 2.f });

        float tX = width / sz1.x;
        float tY = height / sz1.y;
        // Inverse progress: starts at 1.0, ends at 0.0
        float sf = 1.0f - progress;
        s1.setScale({ tX * sf, tY * sf });
    }
    break;

    case 6: // Fade to Black
        // First half (0.0 - 0.5): Fade out Image 1
        if (progress <= 0.5f) {
            float lp = progress * 2.0f; // Local progress 0.0 -> 1.0
            // Calculate alpha (255 -> 0)
            s1.setColor({ 255, 255, 255, (std::uint8_t)(255 * (1.0f - lp)) });
            s2.setColor({ 255, 255, 255, 0 }); // Hide Image 2
        }
        // Second half (0.5 - 1.0): Fade in Image 2
        else {
            float lp = (progress - 0.5f) * 2.0f; // Local progress 0.0 -> 1.0
            s1.setColor({ 255, 255, 255, 0 }); // Hide Image 1
            s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * lp) });
        }
        break;

    case 7: // Cross-Fade (Alpha)
    {
        // Image 1 stays as background. Image 2 fades in on top.
        s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * progress) });
        s2.setPosition({ 0.f, 0.f });
    }
    break;

    case 8: // Page Turn Horizontal
    {
        sf::Vector2u sz1 = t1.getSize(); sf::Vector2u sz2 = t2.getSize();
        // Center everything
        s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
        s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
        s1.setPosition({ width / 2.f, height / 2.f });
        s2.setPosition({ width / 2.f, height / 2.f });

        if (progress <= 0.5f) {
            // Shrink width of Image 1 to 0
            drawMode = 1;
            float sf = 1.0f - (progress * 2.0f);
            s1.setScale({ (width / sz1.x) * sf, height / sz1.y });
        }
        else {
            // Expand width of Image 2 from 0
            drawMode = 2;
            float sf = (progress - 0.5f) * 2.0f;
            s2.setScale({ (width / sz2.x) * sf, height / sz2.y });
        }
    }
    break;

    case 9: // Page Turn Vertical
    {
        // Same as Horizontal, but manipulating Height (Y axis)
        sf::Vector2u sz1 = t1.getSize(); sf::Vector2u sz2 = t2.getSize();
        s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
        s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
        s1.setPosition({ width / 2.f, height / 2.f });
        s2.setPosition({ width / 2.f, height / 2.f });

        if (progress <= 0.5f) {
            drawMode = 1;
            float sf = 1.0f - (progress * 2.0f);
            s1.setScale({ width / sz1.x, (height / sz1.y) * sf });
        }
        else {
            drawMode = 2;
            float sf = (progress - 0.5f) * 2.0f;
            s2.setScale({ width / sz2.x, (height / sz2.y) * sf });
        }
    }
    break;

    case 10: // Shutter Open (Right)
    {
        sf::Vector2u sz1 = t1.getSize();
        // Pivot point is on the RIGHT edge of the image
        s1.setOrigin({ (float)sz1.x, (float)sz1.y / 2.f });
        s1.setPosition({ width, height / 2.f });

        // Scale X goes from 1.0 to 0.0 (Shrink towards right edge)
        s1.setScale({ (width / sz1.x) * (1.0f - progress), height / sz1.y });

        // Image 2 slides in from left to fill the gap
        float ex = -width * (1.0f - progress);
        s2.setPosition({ ex, 0.0f });
    }
    break;

    case 11: // Blur Fade
    {
        // 1. Loading
        static sf::Shader shader;
        static bool loaded = false;
        if (!loaded) {
            if (shader.loadFromMemory(blurShaderCode, sf::Shader::Type::Fragment)) {
                shader.setUniform("texture", sf::Shader::CurrentTexture);
            }
            loaded = true;
        }

        // 2. Strength
        float blurStrength = 0.0f;
        if (progress <= 0.5f) {
            blurStrength = progress * 2.0f;
        }
        else {
            blurStrength = (1.0f - progress) * 2.0f;
        }

        shader.setUniform("blurRadius", blurStrength * 150.0f);

        // 3. RDrawing
        s1.setColor(sf::Color::White);
        s2.setColor(sf::Color::White);

        if (progress <= 0.01f) target.draw(s1);
        else if (progress >= 0.99f) target.draw(s2);
        else {
            if (progress < 0.45f) target.draw(s1, &shader);
            else if (progress > 0.55f) target.draw(s2, &shader);
            else {
                float mix = (progress - 0.45f) * 10.0f;
                s1.setColor({ 255, 255, 255, (std::uint8_t)(255 * (1.0f - mix)) });
                target.draw(s1, &shader);
                s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * mix) });
                target.draw(s2, &shader);
            }
        }
        return;
    }
    break;

    case 12: // Cube Rotate 90°
    {
        // 1. QUALITY SETTINGS
        t1.setSmooth(true);
        t2.setSmooth(true);

        // CONFIGURATION
        float cx = width / 2.f;
        float cy = height / 2.f;
        float fov = 800.f;

        // Number of strips (TriangleStrip). 
        // 96 strips ensure the image is rigid (no waving effect) 
        // while maintaining the full sharpness of the texture.
        const int STRIPS = 96;

        float angle = progress * 1.5707963f; // 0..90 degrees

        // 2. SCALING (MAINTAIN ORIGINAL ASPECT RATIO)
        sf::Vector2f scale1 = s1.getScale();
        sf::Vector2f scale2 = s2.getScale();

        // Wall dimensions calculated based on the sprite scale declared earlier
        float faceW = t1.getSize().x * scale1.x;
        float faceH = t1.getSize().y * scale1.y;

        // Depth of the cube equals the width of the second image
        float cubeDepth = t2.getSize().x * scale2.x;
        float halfD = cubeDepth / 2.0f;

        //3. MATH HELPERS

        auto transformPoint = [&](sf::Vector3f p) -> sf::Vector3f {
            float pz = p.z - halfD;
            float px = p.x;
            float c = std::cos(angle);
            float s = std::sin(angle);
            // Rotate around Y axis
            return { px * c + pz * s, p.y, -px * s + pz * c + halfD };
            };

        auto project = [&](sf::Vector3f p) -> sf::Vector2f {
            float scale = fov / (fov + p.z);
            return { cx + p.x * scale, cy + p.y * scale };
            };

        // Shading calculation
        auto getShade = [&](float baseAngle) -> sf::Color {
            float currentAngle = std::abs(baseAngle - std::abs(progress * 90.0f));
            float rad = currentAngle * 0.017453f;
            float light = std::cos(rad);
            if (light < 0) light = 0;

            float brightness = 0.6f + (light * 0.4f);
            std::uint8_t val = static_cast<std::uint8_t>(255 * brightness);
            return sf::Color(val, val, val);
            };

        sf::Color shade1 = getShade(0.0f);
        sf::Color shade2 = getShade(90.0f);

        // 4. DRAWING WITH TRIANGLE STRIP
        auto drawStripMesh = [&](sf::Texture& tex, sf::Color col, bool isSideFace)
            {
                sf::VertexArray va(sf::PrimitiveType::TriangleStrip, (STRIPS + 1) * 2);

                float localW = faceW;
                float localH = faceH;

                float startX = -localW / 2.0f;
                float yTop = -localH / 2.0f;
                float yBot = localH / 2.0f;

                for (int i = 0; i <= STRIPS; ++i)
                {
                    float u = (float)i / STRIPS; // 0.0 -> 1.0

                    sf::Vector3f pTop, pBot;

                    if (!isSideFace) {
                        // FRONT FACE
                        float x = startX + (u * localW);
                        pTop = { x, yTop, 0.0f };
                        pBot = { x, yBot, 0.0f };
                    }
                    else {
                        // SIDE FACE
                        float z = u * cubeDepth;
                        float fixedX = localW / 2.0f;
                        pTop = { fixedX, yTop, z };
                        pBot = { fixedX, yBot, z };
                    }

                    pTop = transformPoint(pTop);
                    pBot = transformPoint(pBot);

                    sf::Vector2f sTop = project(pTop);
                    sf::Vector2f sBot = project(pBot);

                    // Texture mapping 1:1
                    float tx = u * tex.getSize().x;
                    float tyTop = 0.0f;
                    float tyBot = (float)tex.getSize().y;

                    int idx = i * 2;
                    va[idx].position = sTop;
                    va[idx].texCoords = { tx, tyTop };
                    va[idx].color = col;

                    va[idx + 1].position = sBot;
                    va[idx + 1].texCoords = { tx, tyBot };
                    va[idx + 1].color = col;
                }

                sf::RenderStates rs;
                rs.texture = &tex;
                target.draw(va, rs);
            };

        target.clear(sf::Color::Black);

        //5. RENDERING (Depth Sorting)
        sf::Vector3f tf = transformPoint({ 0.f, 0.f, 0.f });
        sf::Vector3f ts = transformPoint({ faceW / 2.f, 0.f, cubeDepth / 2.f });

        if (tf.z > ts.z) {
            drawStripMesh(t1, shade1, false);
            drawStripMesh(t2, shade2, true);
        }
        else {
            drawStripMesh(t2, shade2, true);
            drawStripMesh(t1, shade1, false);
        }

        return;
    }


    case 13: // Ring Transition 
    {
        float cx = width / 2.f;
        float cy = height / 2.f;

        float radius = 1000.f;  // Orbit radius (how far sideways it moves)
        float depth = 670.f;  // Perspective: lower value = stronger "3D" effect (fisheye)

        // Convert progress (0..1) to radians (0..PI/2)
        float a1 = progress * 1.5707963f;          // Image 1: 0 -> 90 degrees
        float a2 = (1.0f - progress) * 1.5707963f; // Image 2: 90 -> 0 degrees

        auto ringPos = [&](float angle, float sideSign)
            {
                // sideSign: +1 = right, -1 = left
                float x = sideSign * (radius - std::cos(angle) * radius);
                float z = std::sin(angle) * radius;
                float s = depth / (depth + z); // Perspective scale factor (0.0 to 1.0)
                return std::tuple<float, float, float>(x, z, s);
            };


        // IMAGE 1 - Starts at center, moves right along the arc
        auto [x1, z1, s1] = ringPos(a1, +1.f);

        sf::VertexArray quad1(sf::PrimitiveType::Triangles, 6);

        // NOTE: Calculations are based on CANVAS size, not texture size
        float w1 = width * s1;
        float h1 = height * s1;

        float left1 = cx + x1 - w1 / 2.f;
        float top1 = cy - h1 / 2.f;
        float right1 = left1 + w1;
        float bottom1 = top1 + h1;

        // Set vertex positions
        quad1[0].position = { left1,  top1 };
        quad1[1].position = { right1, top1 };
        quad1[2].position = { right1, bottom1 };
        quad1[3].position = { left1,  top1 };
        quad1[4].position = { right1, bottom1 };
        quad1[5].position = { left1,  bottom1 };

        // Texture Coordinates - Map full texture
        quad1[0].texCoords = { 0.f, 0.f };
        quad1[1].texCoords = { (float)t1.getSize().x, 0.f };
        quad1[2].texCoords = { (float)t1.getSize().x, (float)t1.getSize().y };
        quad1[3].texCoords = { 0.f, 0.f };
        quad1[4].texCoords = { (float)t1.getSize().x, (float)t1.getSize().y };
        quad1[5].texCoords = { 0.f, (float)t1.getSize().y };


        // IMAGE 2 - Starts at left (background), moves towards center
        auto [x2, z2, s2] = ringPos(a2, -1.f);

        sf::VertexArray quad2(sf::PrimitiveType::Triangles, 6);

        float w2 = width * s2;
        float h2 = height * s2;

        float left2 = cx + x2 - w2 / 2.f;
        float top2 = cy - h2 / 2.f;
        float right2 = left2 + w2;
        float bottom2 = top2 + h2;

        quad2[0].position = { left2,  top2 };
        quad2[1].position = { right2, top2 };
        quad2[2].position = { right2, bottom2 };
        quad2[3].position = { left2,  top2 };
        quad2[4].position = { right2, bottom2 };
        quad2[5].position = { left2,  bottom2 };

        quad2[0].texCoords = { 0.f, 0.f };
        quad2[1].texCoords = { (float)t2.getSize().x, 0.f };
        quad2[2].texCoords = { (float)t2.getSize().x, (float)t2.getSize().y };
        quad2[3].texCoords = { 0.f, 0.f };
        quad2[4].texCoords = { (float)t2.getSize().x, (float)t2.getSize().y };
        quad2[5].texCoords = { 0.f, (float)t2.getSize().y };

        // PERSPECTIVE DRAWING (Depth Sorting)
        target.clear(sf::Color::Black);

        sf::RenderStates rs1;
        rs1.texture = &t1;

        sf::RenderStates rs2;
        rs2.texture = &t2;

        // Painter's Algorithm: Draw the furthest object (larger Z) first
        if (z1 > z2) {
            target.draw(quad1, rs1);
            target.draw(quad2, rs2);
        }
        else {
            target.draw(quad2, rs2);
            target.draw(quad1, rs1);
        }

        return;
    }
    case 14: // Luma Wipe (Brightness Based)
    {
        // 1. Load the shader (only once)
        static sf::Shader lumaShader;
        static bool loaded = false;
        if (!loaded) {
            // Load from the string constant defined at the top of the file
            if (lumaShader.loadFromMemory(lumaShaderCode, sf::Shader::Type::Fragment)) {
                // Tell shader which texture to use (optional in SFML but good practice)
                lumaShader.setUniform("texture", sf::Shader::CurrentTexture);
            }
            loaded = true;
        }

        // 2. Pass the progress to the shader
        lumaShader.setUniform("progress", progress);

        // 3. Draw Setup
        // We draw Image 1 normally as the background.
        // We draw Image 2 ON TOP using the shader. 
        // The shader will make parts of Image 2 transparent based on brightness.
        target.draw(s1); // Background
        target.draw(s2, &lumaShader); // Foreground with Luma Mask

        return; // Exit early since we handled drawing manually
    }
    break;
        
    case 15: // Fly Away (Zdmuchiwanie)
    {
        sf::Vector2u sz1 = t1.getSize();
        sf::Vector2u sz2 = t2.getSize();
        sf::Vector2f center(width / 2.f, height / 2.f);

        if (progress <= 0.5f) {
            // PHASE 1: Image 1 flies away 
            float lp = progress * 2.0f; // Local progress for first half (0.0 to 1.0)
            float invLp = 1.0f - lp;

            // Set pivot to center for scaling and rotation
            s1.setOrigin({ (float)sz1.x / 2.f, (float)sz1.y / 2.f });
            s1.setPosition(center);
            s1.setScale({ (width / sz1.x) * invLp, (height / sz1.y) * invLp });

            // Shrink scale to 0 and rotate by 180 degrees
            s1.setRotation(sf::degrees(lp * 180.0f));

            // Smooth fade out
            s1.setColor({ 255, 255, 255, (std::uint8_t)(255 * invLp) });

            target.clear(sf::Color::Black);
            target.draw(s1);
        }
        else {
            // PHASE 2: Image 2 flies in
            float lp = (progress - 0.5f) * 2.0f;

            s2.setOrigin({ (float)sz2.x / 2.f, (float)sz2.y / 2.f });
            s2.setPosition(center);
            s2.setScale({ (width / sz2.x) * lp, (height / sz2.y) * lp });

            // Grow scale from 0 to 1.0 and unspin rotation
            s2.setRotation(sf::degrees((1.0f - lp) * -180.0f));

            s2.setColor({ 255, 255, 255, (std::uint8_t)(255 * lp) });

            target.clear(sf::Color::Black);
            target.draw(s2);
        }
        return;
    }


    }


    // 3. DRAWING
    target.clear(sf::Color::Black);

    // Some transitions require specific draw order (Painter's Algorithm)
    if (type == 5) { // Box Out: Draw background (2) first, then shrinking image (1) on top
        target.draw(s2);
        target.draw(s1);
    }
    else if (type == 10) { // Shutter: Draw incoming image (2) first, then outgoing (1)
        target.draw(s2);
        target.draw(s1);
    }
    else if (drawMode == 1) { // Page Turn Phase 1
        target.draw(s1);
    }
    else if (drawMode == 2) { // Page Turn Phase 2
        target.draw(s2);
    }
    else {
        // Standard order: Draw 1 (Background), then 2 (Foreground)
        target.draw(s1);
        target.draw(s2);
    }
}

int main()
{
    // Create the main window
    sf::RenderWindow window(sf::VideoMode({ 1200, 800 }), "Project 28: Ultimate Transitions", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    // Initialize ImGui system
    std::ignore = ImGui::SFML::Init(window);
    SetupModernStyle(); // Apply our custom theme

    // Textures store the raw pixel data
    sf::Texture texture1, texture2;
    // Sprites are lightweight objects that display a texture on screen
    sf::Sprite sprite1(texture1);
    sf::Sprite sprite2(texture2);

    // State variables
    float progress = 0.0f;  // Current animation progress (0.0 to 1.0)
    int transitionType = 0; // Selected transition index
    int framesCount = 60;   // How many frames to generate for export

    // Store the output folder path. Default: "SavedAnimation" next to the .exe
    std::string outputFolderPath = (fs::current_path() / "SavedAnimation").string();

    const char* transitionNames[] = {
        "Slide Left", "Slide Right", "Slide Top", "Slide Bottom",
        "Box In", "Box Out", "Fade to Black", "Cross-Fade",
        "Page Turn Horizontal", "Page Turn Vertical", "Shutter Open",
        "Blur Fade", "3D CUbe Rotation", "Ring", "Luma Wipe (Brightness)", "Fly Away"
    };

    sf::Clock deltaClock;

    // Main Game Loop
    while (window.isOpen())
    {
        // Event Polling
        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // --- IMGUI SIDEBAR UI ---
        // Setup fixed sidebar position on the right
        ImGui::SetNextWindowPos(ImVec2(820, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 760), ImGuiCond_FirstUseEver);

        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse);

        // --- SECTION 1: MEDIA LIBRARY ---
        ImGui::TextDisabled("MEDIA LIBRARY");
        ImGui::Separator();

        // Button for Image 1
        if (ImGui::Button(" Select Image 1 ", ImVec2(150, 40))) {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            // If user picked a file, load it into texture
            if (!path.empty() && texture1.loadFromFile(path)) sprite1.setTexture(texture1, true);
        }
        ImGui::SameLine();
        // Button for Image 2
        if (ImGui::Button(" Select Image 2 ", ImVec2(150, 40))) {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            if (!path.empty() && texture2.loadFromFile(path)) sprite2.setTexture(texture2, true);
        }

        ImGui::Spacing();
        // Show tiny thumbnails (Previews) inside the UI
        if (texture1.getSize().x > 0) ImGui::Image(texture1, { 140.f, 100.f });
        else ImGui::Button("Empty 1", { 140.f, 100.f });
        ImGui::SameLine();
        if (texture2.getSize().x > 0) ImGui::Image(texture2, { 140.f, 100.f });
        else ImGui::Button("Empty 2", { 140.f, 100.f });

        ImGui::Spacing();
        ImGui::Separator();

        // --- SECTION 2: LIVE PREVIEW CONTROLS ---
        ImGui::TextDisabled("ANIMATION CONTROLS");
        ImGui::Spacing();
        ImGui::Text("Preview Progress:");
        ImGui::SliderFloat("##progress", &progress, 0.0f, 1.0f, "%.2f");
        ImGui::Text("Mode:");
        ImGui::Combo("##type", &transitionType, transitionNames, IM_ARRAYSIZE(transitionNames));

        ImGui::Spacing();
        ImGui::Separator();

        // --- SECTION 3: EXPORT TO DISK ---
        ImGui::TextDisabled("EXPORT SETTINGS");
        ImGui::Spacing();

        ImGui::Text("Output Folder:");
        ImGui::TextWrapped("%s", outputFolderPath.c_str());

        // Folder selection button
        if (ImGui::Button(" Change Folder... ", ImVec2(150, 30)))
        {
            std::string newPath = SelectFolderDialog((HWND)window.getNativeHandle());
            if (!newPath.empty()) {
                outputFolderPath = newPath;
            }
        }

        ImGui::Spacing();
        ImGui::Text("Total Frames:");
        ImGui::InputInt("##frames", &framesCount);
        if (framesCount < 10) framesCount = 10;   // Minimum limit
        if (framesCount > 1000) framesCount = 1000; // Maximum limit

        ImGui::Spacing();

        // --- EXPORT BUTTON LOGIC ---
        if (ImGui::Button(" RENDER & SAVE SEQUENCE ", ImVec2(320, 50)))
        {
            // Only proceed if images are loaded
            if (texture1.getSize().x > 0 && texture2.getSize().x > 0)
            {
                fs::path folderPath = outputFolderPath;

                // Try to create the directory if it doesn't exist
                if (!fs::exists(folderPath)) {
                    try {
                        fs::create_directories(folderPath);
                    }
                    catch (...) {
                        // Suppress errors for simplicity
                    }
                }

                // Create an off-screen texture (Virtual Canvas)
                sf::RenderTexture renderTex;
                renderTex.resize({ 1200, 800 });

                // LOOP: Generate frames one by one
                for (int i = 0; i <= framesCount; i++)
                {
                    // Calculate "Time" for this specific frame
                    float p = (float)i / (float)framesCount;

                    // Draw the transition to the off-screen texture (not the window!)
                    RenderTransitionFrame(renderTex, transitionType, p, sprite1, sprite2, texture1, texture2);
                    renderTex.display();

                    // Generate filename: frame_001.png
                    std::stringstream ss;
                    ss << folderPath.string() << "/frame_" << std::setw(3) << std::setfill('0') << i << ".png";

                    // Save to disk
                    sf::Image img = renderTex.getTexture().copyToImage();
                    img.saveToFile(ss.str());
                }

                // Automatically open the folder in Windows Explorer
                ShellExecuteA(NULL, "open", folderPath.string().c_str(), NULL, NULL, SW_SHOWDEFAULT);
            }
            else
            {
                ImGui::OpenPopup("ErrorNoImages");
            }
        }

        // Popup error if images are missing
        if (ImGui::BeginPopup("ErrorNoImages")) {
            ImGui::Text("Error: Please select Image 1 and Image 2 first!");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        ImGui::End();

        // --- MAIN RENDER (LIVE PREVIEW) ---
        // Draw the current state to the actual application window
        RenderTransitionFrame(window, transitionType, progress, sprite1, sprite2, texture1, texture2);

        // Draw UI on top
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}