# Image Transitions Pro

A high-performance C++ application for creating and exporting smooth 2D and 3D transitions between images. Built with **SFML 3.0** and **Dear ImGui**, this tool provides a real-time preview and a frame-by-frame rendering engine for video production and motion design.

## 🚀 Features

* **16 Professional Transitions**: Includes classic slides, fades, and advanced 3D effects.
* **Real-time Preview**: Adjust transition progress manually using an interactive slider.
* **Performance Optimized**: 
    * Multithreaded-ready logic.
    * Luma Wipe with pre-cached luminance data.
    * Optimized CPU Blur using downsampling for high FPS.
* **Sequence Export**: Render the animation into a sequence of PNG frames with customizable frame counts.
* **Modern UI**: Clean, dark-themed interface for media management and settings.
* **Native File Dialogs**: Easy image selection and folder picking using Windows API.

## 🎞 Available Transition Types

| Category | Transitions |
| --- | --- |
| **Linear** | Slide (Left, Right, Top, Bottom), Shutter Open |
| **Fades** | Cross-Fade, Fade to Black, Blur Fade |
| **Geometric** | Box In, Box Out, Fly Away |
| **3D & Advanced** | 3D Cube Rotation, Ring, Page Turn (H/V), Luma Wipe |

## 🛠 Technical Stack

* **Language**: C++17
* **Graphics**: [SFML 3.0](https://www.sfml-dev.org/)
* **UI**: [Dear ImGui](https://github.com/ocornut/imgui)
* **Platform**: Windows (utilizes Win32 API for dialogs)

## 📂 Documentation

The project includes detailed documentation to help you understand the underlying math (3D projections, luminance formulas, etc.) and the codebase structure:
* **PDF Manual**: Full technical documentation is available in the project folder (e.g., `docs/documentation.pdf`) — *Coming soon*.

## ⚙️ How to Use

1.  **Select Media**: Use the "Select Image 1" and "Select Image 2" buttons to load your assets.
2.  **Preview**: Choose a transition mode from the dropdown menu and use the "Preview Progress" slider.
3.  **Configure Export**: Set your desired "Total Frames" (e.g., 60 frames for a 1-second animation at 60fps) and select the output folder.
4.  **Render**: Click "RENDER & SAVE SEQUENCE". The app will generate PNG files and automatically open the folder when finished.

## 🔧 Performance Monitor

The application includes a built-in FPS counter and status indicator:
* **Green (Excellent)**: Optimized performance (>50 FPS).
* **Yellow (Playable)**: Moderate load.
* **Red (Slow)**: CPU bottleneck detected (usually during heavy CPU-based blur or luma operations on very large images).

