#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>

#include "imgui.h"
#include "backends/imgui_impl_sdl3.h"
#include "backends/imgui_impl_opengl3.h"

#include <vector>
#include <cstdlib>
#include <ctime>
#include <cstdio>
#include <cmath>
#include <algorithm>

struct Vec2 { float x, y; };

struct Entity {
    SDL_FRect rect;
    SDL_Color color;
    Vec2 velocity;
    bool alive = true;
};

struct Mob : public Entity {
    int hp = 10;
    int dmg = 5;
};

struct Player {
    SDL_FRect rect;
    SDL_Color color;
    float speed = 300.0f;
    int hp = 100;
	int dmg = 4;
};

struct Buff_Box {
    SDL_FRect rect;
    SDL_Color color;
    enum class Type { FIRE_RATE, BULLET_SPEED, HEAL, FIRE_MODE, BULLET_DAMAGE } type;
    bool alive = true;
};

// helper: normalize vector
static Vec2 normalize(const Vec2& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y);
    if (len <= 0.0001f) return { 0,0 };
    return { v.x / len, v.y / len };
}

void drawRectGL(const SDL_FRect& r, const SDL_Color& c) {
    glColor4f(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
    glBegin(GL_QUADS);
    glVertex2f(r.x, r.y);
    glVertex2f(r.x + r.w, r.y);
    glVertex2f(r.x + r.w, r.y + r.h);
    glVertex2f(r.x, r.y + r.h);
    glEnd();
}

bool aabb(const SDL_FRect& a, const SDL_FRect& b) {
    return (a.x < b.x + b.w &&
        a.x + a.w > b.x &&
        a.y < b.y + b.h &&
        a.y + a.h > b.y);
}

int main(int argc, char** argv)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL init failed: %s\n", SDL_GetError());
        return -1;
    }

    const int WIN_W = 1800;
    const int WIN_H = 1000;

    SDL_Window* window = SDL_CreateWindow("Brotato Wannabe", WIN_W, WIN_H, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        printf("CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        printf("GL context create failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    // ImGui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    
    std::srand((unsigned)std::time(nullptr));

    // Game objects
    Player player;
    player.rect = { WIN_W * 0.5f - 16.0f, WIN_H * 0.5f - 16.0f, 32.0f, 32.0f };
    player.color = { 200, 200, 60, 255 };
    player.hp = 100;

    Mob enemy;
    enemy.hp = 10;
	enemy.dmg = 5;

    std::vector<Mob> enemies;
    std::vector<Entity> bullets;
	std::vector<Buff_Box> buffs;

    // Parametrii de start
    float enemySpawnInterval = 1.0f;
    float enemySpeed = 80.0f;
    float bulletSpeed = 600.0f;
    bool autoShoot = true;
    float fireRate = 6.0f;
    int maxEnemies = 500;
    float enemySpawnTimer = 0.0f;
    float fireTimer = 0.0f;
    float enemySize = 24.0f;
    int lastSpawnSecond = -1;
	int lastSpawnSecondEn = -1;

    bool running = true;
    SDL_Event e;
    
    Uint64 startTime = SDL_GetTicks();

    Uint64 last = SDL_GetPerformanceCounter();
    double perfFreq = (double)SDL_GetPerformanceFrequency();

    while (running) {
        Uint64 now = SDL_GetPerformanceCounter();
        float deltaTime = (float)((now - last) / perfFreq);

        Uint64 currentTime = SDL_GetTicks();
        float gameTime = (currentTime - startTime) / 1000.0f;
        //printf("Delta Time: %.4f s, Game Time: %.2f s\r", deltaTime, gameTime);

        last = now;
        if (deltaTime > 0.1f) deltaTime = 0.1f;

        while (SDL_PollEvent(&e)) {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT) running = false;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) running = false;
        }

        // Input
        const bool* kb = SDL_GetKeyboardState(NULL);
        Vec2 move{ 0,0 };
        if (kb[SDL_SCANCODE_W]) move.y -= 1;
        if (kb[SDL_SCANCODE_S]) move.y += 1;
        if (kb[SDL_SCANCODE_A]) move.x -= 1;
        if (kb[SDL_SCANCODE_D]) move.x += 1;
        Vec2 moveN = normalize(move);
        player.rect.x += moveN.x * player.speed * deltaTime;
        player.rect.y += moveN.y * player.speed * deltaTime;
        
        if (player.rect.x < 0) player.rect.x = 0;
        if (player.rect.y < 0) player.rect.y = 0;
        if (player.rect.x + player.rect.w > WIN_W) player.rect.x = WIN_W - player.rect.w;
        if (player.rect.y + player.rect.h > WIN_H) player.rect.y = WIN_H - player.rect.h;

        // Mouse
        float mx = 0.0f, my = 0.0f;
        SDL_GetMouseState(&mx, &my);

        // Auto Shooting
        fireTimer += deltaTime;
        if (autoShoot && fireTimer >= 1.0f / fireRate) {
            fireTimer = 0.0f;
            
            Vec2 dir{ (float)mx - (player.rect.x + player.rect.w * 0.5f),
                       (float)my - (player.rect.y + player.rect.h * 0.5f) };
            dir = normalize(dir);
            Entity b;
            b.rect = { player.rect.x + player.rect.w * 0.5f - 4.0f,
                       player.rect.y + player.rect.h * 0.5f - 4.0f,
                       8.0f, 8.0f };
            b.color = { 255, 255, 120, 255 };
            b.velocity = { dir.x * bulletSpeed, dir.y * bulletSpeed };
            bullets.push_back(b);
        }

        // space - spawn manual de mobi
        static bool spacePrev = false;
        const bool* kb2 = SDL_GetKeyboardState(NULL);
        bool spaceNow = kb2[SDL_SCANCODE_SPACE];
        if (spaceNow && !spacePrev) {
            // burst spawn de mobi
            for (int i = 0; i < 5 && enemies.size() < (size_t)maxEnemies; ++i) {
				// spawn de mobi la margini
                Mob en;
                float s = enemySize;
                int edge = std::rand() % 4;
                if (edge == 0) { // top
                    en.rect.x = float(std::rand() % WIN_W);
                    en.rect.y = -s - 1;
                }
                else if (edge == 1) { // bottom
                    en.rect.x = float(std::rand() % WIN_W);
                    en.rect.y = WIN_H + 1;
                }
                else if (edge == 2) { // left
                    en.rect.x = -s - 1;
                    en.rect.y = float(std::rand() % WIN_H);
                }
                else { // right
                    en.rect.x = WIN_W + 1;
                    en.rect.y = float(std::rand() % WIN_H);
                }
                en.rect.w = s; en.rect.h = s;
                en.color = { 200, 80, 80, 255 };
                Vec2 dir{ player.rect.x + player.rect.w * 0.5f - (en.rect.x + s * 0.5f),
                           player.rect.y + player.rect.h * 0.5f - (en.rect.y + s * 0.5f) };
                dir = normalize(dir);
                en.velocity = { dir.x * enemySpeed, dir.y * enemySpeed };
                en.alive = true;
                enemies.push_back(en);
            }
        }
        spacePrev = spaceNow;

        // spawn automat de mobi
        enemySpawnTimer += deltaTime;
        if (enemySpawnTimer >= enemySpawnInterval) {
            enemySpawnTimer = 0.0f;
            if (enemies.size() < (size_t)maxEnemies) {
                Mob en;
                float s = enemySize;
                int edge = std::rand() % 4;
                if (edge == 0) { // top
                    en.rect.x = float(std::rand() % WIN_W);
                    en.rect.y = -s - 1;
                }
                else if (edge == 1) { // bottom
                    en.rect.x = float(std::rand() % WIN_W);
                    en.rect.y = WIN_H + 1;
                }
                else if (edge == 2) { // left
                    en.rect.x = -s - 1;
                    en.rect.y = float(std::rand() % WIN_H);
                }
                else { // right
                    en.rect.x = WIN_W + 1;
                    en.rect.y = float(std::rand() % WIN_H);
                }
                en.rect.w = s; en.rect.h = s;
                en.color = { 200, 80, 80, 255 };
                Vec2 dir{ player.rect.x + player.rect.w * 0.5f - (en.rect.x + s * 0.5f),
                           player.rect.y + player.rect.h * 0.5f - (en.rect.y + s * 0.5f) };
                dir = normalize(dir);
                en.velocity = { dir.x * enemySpeed, dir.y * enemySpeed };
                en.alive = true;
                enemies.push_back(en);
            }
        }

        // Update bullets
        for (size_t i = 0; i < bullets.size(); ++i) {
            bullets[i].rect.x += bullets[i].velocity.x * deltaTime;
            bullets[i].rect.y += bullets[i].velocity.y * deltaTime;
			// dispar daca ies din ecran
            if (bullets[i].rect.x < -50 || bullets[i].rect.x > WIN_W + 50 ||
                bullets[i].rect.y < -50 || bullets[i].rect.y > WIN_H + 50) {
                bullets[i].alive = false;
            }
        }

        // Update enemies
        for (size_t i = 0; i < enemies.size(); ++i) {
			// urmarirea playerului de catre mobi
            Vec2 toPlayer{ (player.rect.x + player.rect.w * 0.5f) - (enemies[i].rect.x + enemies[i].rect.w * 0.5f),
                           (player.rect.y + player.rect.h * 0.5f) - (enemies[i].rect.y + enemies[i].rect.h * 0.5f) };
            Vec2 dir = normalize(toPlayer);
            enemies[i].velocity.x = dir.x * enemySpeed;
            enemies[i].velocity.y = dir.y * enemySpeed;

            enemies[i].rect.x += enemies[i].velocity.x * deltaTime;
            enemies[i].rect.y += enemies[i].velocity.y * deltaTime;
        }

		// Collision intre mobi si gloante
        for (size_t bi = 0; bi < bullets.size(); ++bi) {
            if (!bullets[bi].alive) continue;
            for (size_t ei = 0; ei < enemies.size(); ++ei) {
                if (!enemies[ei].alive) continue;
                if (aabb(bullets[bi].rect, enemies[ei].rect)) {
                    bullets[bi].alive = false;
                    enemies[ei].hp -= player.dmg;
                    break;
                }
                if (enemies[ei].hp <= 0) {
                    enemies[ei].alive = false;
				}
            }
        }

        // Enemy vs player
        int p_r = 0, p_g = 0, p_b = 0;
        for (size_t ei = 0; ei < enemies.size(); ++ei) {
            if (!enemies[ei].alive) continue;
            if (aabb(enemies[ei].rect, player.rect)) {
                enemies[ei].alive = false;
                p_r += 20;
                p_g += 20;
                p_b += 20;
                player.hp -= enemies[ei].dmg;
                player.color.r -= p_r;
				player.color.g -= p_g;
                player.color.b += p_b;
            }
        }

        // Buffs
        int currentSecond = (int)gameTime;
        if (currentSecond % 45 == 0 && currentSecond != lastSpawnSecond) {
            Buff_Box buff;
            buff.rect = { (float)(std::rand() % (WIN_W - 32)),
                          (float)(std::rand() % (WIN_H - 32)),
                          32.0f, 32.0f };
            buff.type = static_cast<Buff_Box::Type>(std::rand() % 4);
            buff.alive = true;

			// culoare diferita in functie de tip
            switch (buff.type) {
                case Buff_Box::Type::FIRE_RATE:    buff.color = { 255, 0, 0, 255 }; break;
                case Buff_Box::Type::BULLET_SPEED: buff.color = { 0, 255, 0, 255 }; break;
                case Buff_Box::Type::HEAL:         buff.color = { 0, 0, 255, 255 }; break;
                case Buff_Box::Type::FIRE_MODE:    buff.color = { 255, 255, 0, 255 }; break;
				case Buff_Box::Type::BULLET_DAMAGE: buff.color = { 255, 0, 255, 255 }; break;
            }

            buffs.push_back(buff);
            lastSpawnSecond = currentSecond;
        }

        // Update si coliziuni
        for (auto& buff : buffs) {
            if (buff.alive && aabb(player.rect, buff.rect)) {
                // efect in functie de tip
                switch (buff.type) {
                    case Buff_Box::Type::HEAL: player.hp += 10; break;
                    case Buff_Box::Type::FIRE_RATE: fireRate *= 1.2f; break;
                    case Buff_Box::Type::BULLET_SPEED: bulletSpeed *= 1.2f; break;
                    case Buff_Box::Type::FIRE_MODE: autoShoot = !autoShoot; break;
					case Buff_Box::Type::BULLET_DAMAGE: player.dmg += 2; break;
                }
                buff.alive = false;
            }
        }

        // Update GamePlay
        int currentSecondEn = (int)gameTime;
        if (currentSecondEn % 60 == 0 && currentSecondEn != 0 && currentSecondEn != lastSpawnSecondEn ) {
            enemySpawnInterval = std::max(0.5f, (enemySpawnInterval - 0.05f));
            enemySize = std::min(200.0f, enemySize + 2.0f);
			enemy.dmg += 2;
			enemy.hp += 5;
            lastSpawnSecondEn = currentSecondEn;
        }

        // cleanup
        auto compactEntities = [](auto& vec) {
            size_t dst = 0;
            for (size_t i = 0; i < vec.size(); ++i) {
                if (vec[i].alive) {
                    if (dst != i) vec[dst] = vec[i];
                    ++dst;
                }
            }
            vec.resize(dst);
        };
        compactEntities(bullets);
        compactEntities(enemies);

		// Resetam culoarea jucatorului treptat dupa ce nu mai e lovit
        if (player.color.r > 200) {
            player.color.r = (Uint8)std::max(200, (int)player.color.r - 150 * (int)std::round(deltaTime));
            if (player.color.r < 200) player.color.r = 200;
        }

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Debug / Controls");
        ImGui::Separator();
		ImGui::Text("Game Time: %.2f s", gameTime);
        ImGui::Separator();
        ImGui::Text("Player HP: %d", player.hp);
        ImGui::Text("Player HP: %d", player.dmg);
        ImGui::Checkbox("Auto Shoot", &autoShoot);
        ImGui::SliderFloat("Fire Rate (shots/s)", &fireRate, 0.5f, 20.0f);
        ImGui::SliderFloat("Bullet Speed", &bulletSpeed, 100.0f, 1200.0f);
        ImGui::Separator();
        ImGui::Text("Mob HP: %d", enemy.hp);
        ImGui::Text("Mob Size: %d", int(enemySize));
        ImGui::Text("Enemies: %zu", enemies.size());
        ImGui::SliderFloat("Enemy Spawn Interval (s)", &enemySpawnInterval, 0.05f, 3.0f);
        ImGui::SliderFloat("Enemy Speed", &enemySpeed, 10.0f, 500.0f);
        ImGui::SliderInt("Max Enemies", &maxEnemies, 10, 5000);
        if (ImGui::Button("Clear Enemies")) enemies.clear();
        if (ImGui::Button("Clear Bullets")) bullets.clear();
        if (ImGui::Button("Spawn 10 Enemies")) {
            for (int i = 0; i < 10 && enemies.size() < (size_t)maxEnemies; i++) {
                Mob en;
                float s = enemySize;
                int edge = std::rand() % 4;
                if (edge == 0) { en.rect.x = float(std::rand() % WIN_W); en.rect.y = -s - 1; }
                else if (edge == 1) { en.rect.x = float(std::rand() % WIN_W); en.rect.y = WIN_H + 1; }
                else if (edge == 2) { en.rect.x = -s - 1; en.rect.y = float(std::rand() % WIN_H); }
                else { en.rect.x = WIN_W + 1; en.rect.y = float(std::rand() % WIN_H); }
                en.rect.w = s; en.rect.h = s;
                en.color = { 200, 80, 80, 255 };
                Vec2 dir{ player.rect.x + player.rect.w * 0.5f - (en.rect.x + s * 0.5f),
                           player.rect.y + player.rect.h * 0.5f - (en.rect.y + s * 0.5f) };
                dir = normalize(dir);
                en.velocity = { dir.x * enemySpeed, dir.y * enemySpeed };
                en.alive = true;
                enemies.push_back(en);
            }
        }
        ImGui::End();

        // Render
        glViewport(0, 0, WIN_W, WIN_H);
        glClearColor(0.07f, 0.07f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0, WIN_W, WIN_H, 0.0, -1.0, 1.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        drawRectGL(player.rect, player.color);

        for (auto& en : enemies) drawRectGL(en.rect, en.color);

        for (auto& b : bullets) drawRectGL(b.rect, b.color);

        for (auto& buff : buffs) {
            if (buff.alive) drawRectGL(buff.rect, buff.color);
        }

        // UI - ImGui
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);

		// verificare daca jucatorul a murit
        if (player.hp <= 0) {
            player.hp = 100;
			player.color = { 200, 200, 60, 255 };
            player.rect.x = WIN_W * 0.5f - 16.0f;
            player.rect.y = WIN_H * 0.5f - 16.0f;
            enemies.clear();
            bullets.clear();
        }
    }

    // cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
