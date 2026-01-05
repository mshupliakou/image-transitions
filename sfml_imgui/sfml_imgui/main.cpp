#include "pch.h"
#include <iostream>
#include <cstdint>    // Required for std::uint8_t (Strict types in SFML 3.0)
#include <filesystem> // C++17 standard library for creating folders and managing paths
#include <sstream>    // Required for string stream manipulations (constructing filenames)
#include <iomanip>    // Required for std::setw, std::setfill (formatting numbers like 001, 002)

// Create an alias for std::filesystem to save typing 'std::filesystem' repeatedly
namespace fs = std::filesystem;

// --- WINDOWS API BLOCK ---
// We need the Windows API to access native File Explorer dialogs.
#define NOMINMAX      // Prevents Windows headers from defining min/max macros that conflict with std::min/std::max
#include <Windows.h>
#include <commdlg.h>  // Standard Open File Dialog library
#include <shellapi.h> // For ShellExecute (to open folder in Windows Explorer)
#include <shlobj.h>   // For SHBrowseForFolder (Folder selection dialog)

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

// --- HELPER FUNCTION: OPEN FILE DIALOG ---
// Opens a native Windows dialog to select an image file.
// Returns the file path as a string, or empty string if cancelled.
std::string OpenFileDialog(HWND ownerHandle)
{
    OPENFILENAMEA ofn; // Structure containing dialog settings
    char fileName[MAX_PATH] = ""; // Buffer to store the result path

    // Initialize memory with zeros to avoid garbage values
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

    // Open the dialog and get the Item ID List (pidl)
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
    style.FrameRounding = 6.0f;    // Buttons
    style.GrabRounding = 6.0f;     // Sliders
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

// --- CORE RENDERING LOGIC ---
// This function calculates the mathematics for every transition.
// It is designed to work with ANY RenderTarget (Window for preview, or RenderTexture for saving).
// Params:
// - target: Where to draw? (The Window or the File buffer)
// - type: Which transition effect to use?
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
        sf::Vector2u sz1 = t1.getSize();
        s1.setScale({ width / sz1.x, height / sz1.y }); // Fit to screen
    }
    if (t2.getSize().x > 0) {
        s2.setColor(sf::Color::White);
        s2.setOrigin({ 0.f, 0.f });
        s2.setPosition({ 0.f, 0.f });
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
            // Shrink width of Image 1 to 0 (First half)
            drawMode = 1;
            float sf = 1.0f - (progress * 2.0f);
            s1.setScale({ (width / sz1.x) * sf, height / sz1.y });
        }
        else {
            // Expand width of Image 2 from 0 (Second half)
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

    case 11: // Luma Wipe (Brightness Based)
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

    // List of transition names for the Dropdown menu
    const char* transitionNames[] = {
        "Slide Left", "Slide Right", "Slide Top", "Slide Bottom",
        "Box In", "Box Out", "Fade to Black", "Cross-Fade",
        "Page Turn Horizontal", "Page Turn Vertical", "Shutter Open",
        "Luma Wipe (Brightness)" // <-- New Transition Added
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