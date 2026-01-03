#include "pch.h"
#include <iostream>

// --- WINDOWS API BLOCK ---
#define NOMINMAX 
#include <Windows.h>
#include <commdlg.h> 

// File selection function
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

// --- FUNCTION FOR MODERN DESIGN ---
void SetupModernStyle()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // 1. Corner rounding (Modern Look)
    style.WindowRounding = 12.0f;
    style.ChildRounding = 12.0f;
    style.FrameRounding = 6.0f;    // Rounded buttons and inputs
    style.GrabRounding = 6.0f;     // Rounded sliders
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;

    // 2. Padding (for spacing)
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(10, 10);
    style.IndentSpacing = 20.0f;

    // 3. Color Palette (Deep Dark & Purple)
    ImVec4* colors = style.Colors;

    // Window and panel backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);

    // Buttons
    colors[ImGuiCol_Button] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);

    // Sliders and Checkboxes (Accent color - Purple)
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.44f, 0.37f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.55f, 0.48f, 0.85f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.44f, 0.37f, 0.80f, 0.40f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.44f, 0.37f, 0.80f, 0.60f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.44f, 0.37f, 0.80f, 0.80f);

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
}

int main()
{
    sf::RenderWindow window(sf::VideoMode({ 1200, 800 }), "Project 28: Modern Transitions", sf::Style::Titlebar | sf::Style::Close);
    window.setFramerateLimit(60);

    std::ignore = ImGui::SFML::Init(window);

    // !!! ENABLE MODERN STYLE !!!
    SetupModernStyle();

    sf::Texture texture1, texture2;
    // SFML 3.0: Initialize sprites
    sf::Sprite sprite1(texture1);
    sf::Sprite sprite2(texture2);

    float progress = 0.0f;
    int transitionType = 0;
    const char* transitionNames[] = { "From Left", "From Right", "From Top", "From Bottom" };

    sf::Clock deltaClock;

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            ImGui::SFML::ProcessEvent(window, *event);
            if (event->is<sf::Event::Closed>()) window.close();
        }

        ImGui::SFML::Update(window, deltaClock.restart());

        // --- IMPROVED PANEL (SIDEBAR) ---
        ImGui::SetNextWindowPos(ImVec2(820, 20), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(350, 760), ImGuiCond_FirstUseEver);

        ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_NoCollapse);

        ImGui::TextDisabled("MEDIA LIBRARY");
        ImGui::Separator();
        ImGui::Spacing();

        // BUTTON 1
        if (ImGui::Button(" Select Image 1 ", ImVec2(150, 40)))
        {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            if (!path.empty() && texture1.loadFromFile(path)) {
                // !!! IMPORTANT FIX BELOW !!!
                // Tell the sprite to update the texture and reset the size (true)
                sprite1.setTexture(texture1, true);

                sf::Vector2u sz = texture1.getSize();
                sprite1.setScale({ 1200.0f / sz.x, 800.0f / sz.y });
            }
        }
        ImGui::SameLine();

        // BUTTON 2
        if (ImGui::Button(" Select Image 2 ", ImVec2(150, 40)))
        {
            std::string path = OpenFileDialog((HWND)window.getNativeHandle());
            if (!path.empty() && texture2.loadFromFile(path)) {
                // !!! IMPORTANT FIX BELOW !!!
                sprite2.setTexture(texture2, true);

                sf::Vector2u sz = texture2.getSize();
                sprite2.setScale({ 1200.0f / sz.x, 800.0f / sz.y });
            }
        }

        ImGui::Spacing();
        ImGui::Text("Previews:");
        if (texture1.getSize().x > 0) ImGui::Image(texture1, { 140.f, 100.f });
        else ImGui::Button("Empty Slot 1", { 140.f, 100.f });

        ImGui::SameLine();
        if (texture2.getSize().x > 0) ImGui::Image(texture2, { 140.f, 100.f });
        else ImGui::Button("Empty Slot 2", { 140.f, 100.f });

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("ANIMATION CONTROLS");
        ImGui::Spacing();
        ImGui::Text("Transition Progress:");
        ImGui::SliderFloat("##progress", &progress, 0.0f, 1.0f, "%.2f");
        ImGui::Spacing();
        ImGui::Text("Animation Mode:");
        ImGui::Combo("##type", &transitionType, transitionNames, IM_ARRAYSIZE(transitionNames));

        ImGui::End();

        // --- LOGIC ---
        // Sprite 1 always stays in place (if image is loaded)
        if (texture1.getSize().x > 0) {
            sprite1.setPosition({ 0.0f, 0.0f });
        }

        // Sprite 2 moves (if loaded)
        if (texture2.getSize().x > 0)
        {
            float width = 1200.0f;
            float height = 800.0f;
            float xOffset = 0.0f, yOffset = 0.0f;

            switch (transitionType) {
            case 0: xOffset = -width * (1.0f - progress); break;
            case 1: xOffset = width * (1.0f - progress); break;
            case 2: yOffset = -height * (1.0f - progress); break;
            case 3: yOffset = height * (1.0f - progress); break;
            }
            sprite2.setPosition({ xOffset, yOffset });
        }

        // --- RENDERING ---
        window.clear(sf::Color(20, 20, 25));

        if (texture1.getSize().x > 0) window.draw(sprite1);
        if (texture2.getSize().x > 0) window.draw(sprite2);

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}