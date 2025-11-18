#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstdio>

struct Vec2 {
    float x, y;
};

struct Entity {
    SDL_FRect rect;
    SDL_Color color;
    Vec2 velocity;
};

void updateEntity(Entity& e, float deltaTime, float speedMultiplier) {
    e.rect.x += e.velocity.x * deltaTime * speedMultiplier;
    e.rect.y += e.velocity.y * deltaTime * speedMultiplier;

    if (e.rect.x < 0 || e.rect.x + e.rect.w > 1000)
        e.velocity.x *= -1;
    if (e.rect.y < 0 || e.rect.y + e.rect.h > 800)
        e.velocity.y *= -1;
}

void drawEntity(Entity& e) {
    glColor3f(e.color.r / 255.0f, e.color.g / 255.0f, e.color.b / 255.0f);
    glBegin(GL_QUADS);
    glVertex2f(e.rect.x, e.rect.y);
    glVertex2f(e.rect.x + e.rect.w, e.rect.y);
    glVertex2f(e.rect.x + e.rect.w, e.rect.y + e.rect.h);
    glVertex2f(e.rect.x, e.rect.y + e.rect.h);
    glEnd();
}

bool checkCollision(const Entity& a, const Entity& b) { //AABB collision detection
    return (a.rect.x < b.rect.x + b.rect.w &&
        a.rect.x + a.rect.w > b.rect.x &&
        a.rect.y < b.rect.y + b.rect.h &&
        a.rect.y + a.rect.h > b.rect.y);
}

int main(int argc, char** argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("Eroare SDL: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("GameEngine", 1000, 800, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("Eroare creare fereastra: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 150");

    std::vector<Entity> entities;
    float spawnRate = 1.0f;     
    float speedMultiplier = 1.0f;
    float spawnTimer = 0.0f;

    bool running = true;
    SDL_Event e;
    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 last = now;

    srand((unsigned int)time(nullptr));

    while (running) {
        last = now;
        now = SDL_GetPerformanceCounter();
        float deltaTime = static_cast<float>((now - last) / (double)SDL_GetPerformanceFrequency());

        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT)
                running = false;
        }

        spawnTimer += deltaTime;
        if (spawnTimer >= 1.0f / spawnRate && entities.size() < 100000) {
            spawnTimer = 0.0f;

            Entity e{};
            e.rect = { float(rand() % 1150), float(rand() % 750), 30.0f, 30.0f };
            e.color = { Uint8(rand() % 256), Uint8(rand() % 256), Uint8(rand() % 256), 255 };

            float speed = 50.0f + rand() % 100;
            e.velocity = { ((rand() & 1) ? 1.f : -1.f) * speed,
                           ((rand() & 1) ? 1.f : -1.f) * speed };

            entities.push_back(e);
        }

        for (auto& ent : entities)
            updateEntity(ent, deltaTime, speedMultiplier);

        for (size_t i = 0; i < entities.size(); i++) {
            for (size_t j = i + 1; j < entities.size(); j++) {
                if (checkCollision(entities[i], entities[j])) {
                    entities[i].color = { 255, 0, 0, 255 };
                    entities[j].color = { 255, 0, 0, 255 };
                }
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Control Panel");
        ImGui::SliderFloat("Entity Speed", &speedMultiplier, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Spawn Rate", &spawnRate, 0.1f, 10.0f, "%.1f / s");
        ImGui::Text("Entity count: %d", (int)entities.size());
        ImGui::End();

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, 1000.0, 800.0, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        for (auto& ent : entities)
            drawEntity(ent);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
