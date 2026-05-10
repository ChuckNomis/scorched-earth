#include <cmath>
#include <iostream>
using namespace std;

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

class ScorchedEarthMockup {
public:
    int run() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            cout << SDL_GetError() << endl;
            return -1;
        }

        SDL_Window*   win;
        SDL_Renderer* ren;
        if (!SDL_CreateWindowAndRenderer("Scorched Earth", 800, 600, 0, &win, &ren)) {
            cout << SDL_GetError() << endl;
            return -1;
        }

        SDL_Texture* tankTex = IMG_LoadTexture(ren, "../res/tank.png");
        SDL_Texture* explTex = IMG_LoadTexture(ren, "../res/explosion.png");
        if (!tankTex || !explTex) {
            cout << SDL_GetError() << endl;
            return -1;
        }

        for (int i = 0; i < 2000; ++i) {
            const int frame = i % 400;

            SDL_SetRenderDrawColor(ren, 20, 60, 120, 255);
            SDL_RenderClear(ren);

            SDL_SetRenderDrawColor(ren, 60, 110, 30, 255);
            SDL_FRect ground = {0, 530, 800, 70};
            SDL_RenderFillRect(ren, &ground);

            // tank1 – blue tint, facing right
            SDL_SetTextureColorMod(tankTex, 100, 140, 255);
            SDL_RenderTexture(ren, tankTex, nullptr, &TANK1_POS);

            // tank2 – red tint, facing left (flipped)
            SDL_SetTextureColorMod(tankTex, 255, 80, 80);
            SDL_RenderTextureRotated(ren, tankTex, nullptr, &TANK2_POS,
                                     0.0, nullptr, SDL_FLIP_HORIZONTAL);

            if (frame >= 80 && frame < 220) {
                float t  = (frame - 80) / 140.f;
                float px = 148.f + 500.f * t;
                float py = 490.f + 5.f * t - 200.f * sinf(3.14159265f * t);
                SDL_SetRenderDrawColor(ren, 255, 220, 0, 255);
                SDL_FRect proj = {px, py, 10, 10};
                SDL_RenderFillRect(ren, &proj);
            }

            if (frame >= 220) {
                float t    = (frame - 220) / 180.f;
                float size = 80.f * (t < 0.5f ? t * 2.f : 1.f);
                Uint8 alpha = t < 0.7f ? 255
                            : (Uint8)((1.f - (t - 0.7f) / 0.3f) * 255);
                SDL_SetTextureAlphaMod(explTex, alpha);
                SDL_FRect exp = {665.f - size / 2, 495.f - size / 2, size, size};
                SDL_RenderTexture(ren, explTex, nullptr, &exp);
            }

            SDL_RenderPresent(ren);
            SDL_Delay(10);
        }

        SDL_Quit();
        return 0;
    }

private:
    static constexpr SDL_FRect TANK1_POS = {80,  495, 70, 35};
    static constexpr SDL_FRect TANK2_POS = {630, 495, 70, 35};
};

int main() {
    ScorchedEarthMockup se;
    return se.run();
}
