#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <random>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <opencv2/opencv.hpp>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#include "ImGuiFileDialog.h"

struct Drone { int id; float x, y; };

struct ImageState {
    std::string path;
    SDL_Texture* imageTex = nullptr;
    SDL_Texture* edgeTex  = nullptr;
    SDL_Rect    imgRect{0,0,0,0};
    cv::Mat     edgesMat;
    std::vector<Drone> drones;
    std::vector<std::vector<Drone>> history;
    size_t      historyIndex = 0;
    bool        placed      = false;
    int         droneCount  = 0;
};

int main() {
    // Initialize SDL2 and SDL_image
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);

    SDL_Window* window = SDL_CreateWindow("Drone Planner",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    ImGui::StyleColorsDark();

    std::vector<ImageState> images;
    int currentImage = -1;
    std::mt19937 rng{std::random_device{}()};
    bool running = true, dragging = false;
    int dragId = -1;
    SDL_Event event;

    auto pushHistory = [&](ImageState& st){
        if (st.historyIndex + 1 < st.history.size())
            st.history.erase(st.history.begin() + st.historyIndex + 1, st.history.end());
        st.history.push_back(st.drones);
        st.historyIndex = st.history.size() - 1;
    };

    // Main loop
    while (running) {
        // Poll events
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT) {
                running = false;
            }

            // Handle dragging for current image
            if (currentImage >= 0 && !ImGui::GetIO().WantCaptureMouse) {
                auto& st = images[currentImage];
                int cx = st.imgRect.x + st.imgRect.w/2;
                int cy = st.imgRect.y + st.imgRect.h/2;

                if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                    int mx = event.button.x, my = event.button.y;
                    for (auto& d : st.drones) {
                        int sx = cx + int(d.x), sy = cy - int(d.y);
                        if (std::abs(mx - sx) < 10 && std::abs(my - sy) < 10) {
                            dragging = true;
                            dragId = d.id;
                            break;
                        }
                    }
                }
                if (dragging && event.type == SDL_MOUSEBUTTONUP) {
                    dragging = false;
                    pushHistory(st);
                }
                if (dragging && event.type == SDL_MOUSEMOTION) {
                    int mx = event.motion.x, my = event.motion.y;
                    for (auto& d : st.drones) {
                        if (d.id == dragId) {
                            d.x = mx - cx;
                            d.y = cy - my;
                            break;
                        }
                    }
                }
            }
        }

        // Start ImGui frame
        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // Centered Insert button before any image
        if (currentImage < 0) {
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::SetNextWindowPos({402, 354});
            ImGui::SetNextWindowSize({220, 60});
            ImGui::Begin("##center", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
            if (ImGui::Button("Insert Image", {200, 40})) {
                ImGuiFileDialog::Instance()->OpenDialog(
                    "ChooseFile", "Select Image", ".png,.jpg", IGFD::FileDialogConfig{}
                );
            }
            ImGui::End();
        }

        // File dialog handling
        if (ImGuiFileDialog::Instance()->Display("ChooseFile")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                ImageState st;
                st.path = ImGuiFileDialog::Instance()->GetFilePathName();
                SDL_Surface* surf = IMG_Load(st.path.c_str());
                if (surf) {
                    // Set up image rect
                    st.imgRect.x = (1024 - surf->w) / 2;
                    st.imgRect.y = (768 - surf->h) / 2;
                    st.imgRect.w = surf->w;
                    st.imgRect.h = surf->h;
                    // Create texture
                    st.imageTex = SDL_CreateTextureFromSurface(renderer, surf);

                    // Edge detection
                    cv::Mat mat(surf->h, surf->w, CV_8UC4, surf->pixels);
                    cv::cvtColor(mat, st.edgesMat, cv::COLOR_BGRA2GRAY);
                    cv::Canny(st.edgesMat, st.edgesMat, 50, 150);

                    // Convert to SDL texture
                    SDL_Surface* eSurf = SDL_CreateRGBSurfaceWithFormatFrom(
                        st.edgesMat.data, st.edgesMat.cols, st.edgesMat.rows,
                        8, st.edgesMat.step, SDL_PIXELFORMAT_INDEX8
                    );
                    SDL_Palette* pal = eSurf->format->palette;
                    SDL_Color cols[256];
                    for (int i = 0; i < 256; i++)
                        cols[i] = {Uint8(i), Uint8(i), Uint8(i), 255};
                    SDL_SetPaletteColors(pal, cols, 0, 256);
                    st.edgeTex = SDL_CreateTextureFromSurface(renderer, eSurf);
                    SDL_FreeSurface(eSurf);
                    SDL_FreeSurface(surf);

                    images.push_back(st);
                    currentImage = (int)images.size() - 1;
                }
            }
            ImGuiFileDialog::Instance()->Close();
        }

        // Bottom taskbar once an image is loaded
        if (currentImage >= 0) {
            auto& st = images[currentImage];
            ImGui::SetNextWindowPos({0, 740});
            ImGui::SetNextWindowSize({1024, 28});
            ImGui::Begin("##task", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
            if (ImGui::Button("Insert"))
                ImGuiFileDialog::Instance()->OpenDialog(
                    "ChooseFile", "Select Image", ".png,.jpg", IGFD::FileDialogConfig{}
                );
            ImGui::SameLine();
            if (ImGui::Button("<< Prev") && currentImage > 0)
                currentImage--;
            ImGui::SameLine();
            if (ImGui::Button("Next >>") && currentImage + 1 < (int)images.size())
                currentImage++;
            ImGui::SameLine();
            ImGui::Text("Drones:");
            ImGui::SameLine();
            ImGui::PushItemWidth(80);
            ImGui::InputInt("##cnt", &st.droneCount);
            if (st.droneCount < 0) st.droneCount = 0;
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Place") && !st.placed && st.droneCount > 0) {
                // Gather edge points
                std::vector<cv::Point> pts;
                for (int y = 0; y < st.edgesMat.rows; ++y)
                    for (int x = 0; x < st.edgesMat.cols; ++x)
                        if (st.edgesMat.at<uchar>(y, x) > 0)
                            pts.emplace_back(x, y);
                // Randomly sample exactly droneCount points
                if ((int)pts.size() >= st.droneCount) {
                    std::shuffle(pts.begin(), pts.end(), rng);
                    st.drones.clear();
                    for (int i = 0; i < st.droneCount; ++i) {
                        auto& p = pts[i];
                        float dx = p.x - st.imgRect.w / 2.0f;
                        float dy = st.imgRect.h / 2.0f - p.y;
                        st.drones.push_back({i + 1, dx, dy});
                    }
                    pushHistory(st);
                    st.placed = true;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                std::string out = std::string(getenv("HOME")) + "/Desktop/dumm/src/result.txt";
                std::ofstream ofs(out);
                for (auto& img : images) {
                    ofs << img.path << "\n";
                    for (auto& d : img.drones)
                        ofs << "  " << d.id << ": x=" << d.x << ", y=" << d.y << "\n";
                }
            }
            ImGui::End();
        }

        // Rendering
        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        if (currentImage >= 0) {
            auto& st = images[currentImage];
            SDL_RenderCopy(renderer, st.imageTex, nullptr, &st.imgRect);
            SDL_SetTextureBlendMode(st.edgeTex, SDL_BLENDMODE_ADD);
            SDL_RenderCopy(renderer, st.edgeTex, nullptr, &st.imgRect);

            int cx = st.imgRect.x + st.imgRect.w / 2;
            int cy = st.imgRect.y + st.imgRect.h / 2;
            // Axes
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 200);
            SDL_RenderDrawLine(renderer, st.imgRect.x, cy, st.imgRect.x + st.imgRect.w, cy);
            SDL_RenderDrawLine(renderer, cx, st.imgRect.y, cx, st.imgRect.y + st.imgRect.h);
            // Origin circle
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            for (int dy = -5; dy <= 5; ++dy)
                for (int dx = -5; dx <= 5; ++dx)
                    if (dx*dx + dy*dy <= 25)
                        SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            // Drones
            SDL_SetRenderDrawColor(renderer, 255, 0, 0, 200);
            for (auto& d : st.drones) {
                int sx = cx + int(d.x), sy = cy - int(d.y);
                SDL_Rect r{sx - 6, sy - 6, 12, 12};
                SDL_RenderFillRect(renderer, &r);
                ImGui::GetBackgroundDrawList()->AddText(
                    { (float)sx + 6, (float)sy - 6 },
                    IM_COL32(255, 255, 255, 255),
                    std::to_string(d.id).c_str()
                );
            }
        }

        ImGui::Render();
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // --- Cleanup ---
    for (auto& img : images) {
        if (img.imageTex) SDL_DestroyTexture(img.imageTex);
        if (img.edgeTex)  SDL_DestroyTexture(img.edgeTex);
    }
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();

    return 0;
}
