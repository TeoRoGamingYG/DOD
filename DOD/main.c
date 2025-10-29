#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    SDL_FRect rect;
    SDL_Color color;
    Vec2 velocity;
} Entity;

void updateEntity(Entity* e, float deltaTime) {
    e->rect.x += e->velocity.x * deltaTime;
    e->rect.y += e->velocity.y * deltaTime;
}

void drawEntity(SDL_Renderer* renderer, Entity* e) {
    SDL_SetRenderDrawColor(renderer, e->color.r, e->color.g, e->color.b, 255);
    SDL_RenderFillRect(renderer, &e->rect);
}

int checkCollision(const Entity* a, const Entity* b) {
    return (a->rect.x < b->rect.x + b->rect.w &&
        a->rect.x + a->rect.w > b->rect.x &&
        a->rect.y < b->rect.y + b->rect.h &&
        a->rect.y + a->rect.h > b->rect.y);
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL_Init error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_Window* window = SDL_CreateWindow("Game Engine", 1200, 800, SDL_WINDOW_RESIZABLE);;
    if (!window) {
        printf("Window create error: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!renderer) {
        printf("Renderer create error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    srand((unsigned int)time(NULL));

    
    Entity entities[100];
    int entityCount = 0;

    float spawnTimer = 0.0f;
    const float spawnInterval = 1.0f;

    Uint64 now = SDL_GetPerformanceCounter();
    Uint64 last = 0;
    double deltaTime = 0.0;
    int running = 1;

    while (running) {
        last = now;
        now = SDL_GetPerformanceCounter();
        deltaTime = (double)(now - last) / (double)SDL_GetPerformanceFrequency();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT)
                running = 0;
        }

        spawnTimer += (float)deltaTime;
        if (spawnTimer >= spawnInterval && entityCount < 100) {
            spawnTimer = 0.0f;

            Entity e;
            e.rect.x = (float)(rand() % 760 + 20);
            e.rect.y = (float)(rand() % 560 + 20);
            e.rect.w = 30;
            e.rect.h = 30;

            e.color.r = rand() % 255;
            e.color.g = rand() % 255;
            e.color.b = rand() % 255;
            e.color.a = 255;

            e.velocity.x = ((rand() % 2) ? 1 : -1) * (50.0f + rand() % 100);
            e.velocity.y = ((rand() % 2) ? 1 : -1) * (50.0f + rand() % 100);

            entities[entityCount++] = e;
        }

        for (int i = 0; i < entityCount; i++) {
            updateEntity(&entities[i], (float)deltaTime);
            if (entities[i].rect.x < 0 || entities[i].rect.x + entities[i].rect.w > 1200)
                entities[i].velocity.x *= -1;
            if (entities[i].rect.y < 0 || entities[i].rect.y + entities[i].rect.h > 800)
                entities[i].velocity.y *= -1;
        }

        for (int i = 0; i < entityCount; i++) {
            for (int j = i + 1; j < entityCount; j++) {
                if (checkCollision(&entities[i], &entities[j])) {
                    entities[i].color.r = 255;
                    entities[i].color.g = 0;
                    entities[i].color.b = 0;

                    entities[j].color.r = 255;
                    entities[j].color.g = 0;
                    entities[j].color.b = 0;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255);
        SDL_RenderClear(renderer);

        for (int i = 0; i < entityCount; i++)
            drawEntity(renderer, &entities[i]);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
