#include <iostream>
using namespace std;

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

class PongMockup
{
public:
	int run() {
		if (!SDL_Init(SDL_INIT_VIDEO)) {
			cout << SDL_GetError() << endl;
			return -1;
		}

		SDL_Window *win;
		SDL_Renderer *ren;

		if (!SDL_CreateWindowAndRenderer(
			"Pong!", 800, 600, 0, &win, &ren)) {
			cout << SDL_GetError() << endl;
			return -1;
		}

		SDL_Surface *surf = IMG_Load("res/pong.png");
		if (surf == nullptr) {
			cout << SDL_GetError() << endl;
			return -1;
		}

		SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
		if (tex == nullptr) {
			cout << SDL_GetError() << endl;
			return -1;
		}
		SDL_DestroySurface(surf);

		// position & width for ball & paddles
		SDL_FRect ball = {400-19./2, 300-19./2, 19, 19};
		SDL_FRect pad1 = {42,300-133./2,16,133};
		SDL_FRect pad2 = {742,300-133./2,16,133};

		// direction of ball & paddles
		SDL_FPoint ball_dir = {0,0};
		SDL_FPoint pad1_dir = {0,0};
		SDL_FPoint pad2_dir = {0,.8};

		// score position & texture (i.e., number)
		SDL_FRect score1 = {370-17./2,10,17,25};
		SDL_FRect score2 = {430-17./2,10,17,25};
		SDL_FRect score1_tex = ZERO_TEX;
		SDL_FRect score2_tex = ZERO_TEX;

		int counter = 30;

		for (int i = 0; i < 1000; ++i) {
			if (counter > 0) {
				if (--counter == 0) {
					ball_dir.x = 1.4;
					ball_dir.y = 1.4;
				}
			}

			// move ball
			ball.x += ball_dir.x;
			ball.y += ball_dir.y;

			// top bounds for ball
			if (ball.y <= 0) {
				ball_dir.y = -ball_dir.y;
				ball.y = 0;
			}
			// bottom bounds for ball
			if (ball.y + ball.h >= 600) {
				ball_dir.y = -ball_dir.y;
				ball.y = 600 - ball.h;
			}

			// did ball hit right paddle?
			if (ball.x + ball.w >= pad2.x) {
				pad2_dir.y = 0;
				ball_dir.x = -ball_dir.x;
				pad1_dir.y = -.5;
			}
			if (pad1.y <= 0) {
				pad1.y = 0;
				pad1_dir.y = 0;
			}

			// did ball cross left bound? (score)
			if (ball.x+ball.w <= 0) {
				score2_tex = ONE_TEX;
				ball.x = 400-19./2;
				ball.y = 300-19./2;
				counter = 30;
				ball_dir.x = 0;
				ball_dir.y = 0;
			}


			pad1.x += pad1_dir.x;
			pad1.y += pad1_dir.y;
			pad2.x += pad2_dir.x;
			pad2.y += pad2_dir.y;

			SDL_RenderClear(ren);

			SDL_RenderTexture(ren, tex, &BALL_TEX, &ball);
			SDL_RenderTexture(ren, tex, &PAD1_TEX, &pad1);
			SDL_RenderTexture(ren, tex, &PAD2_TEX, &pad2);

			for (int j = 0; j < 25; ++j) {
				SDL_FRect pos = {397, 10+j*24.f, 6, 6};
				SDL_RenderTexture(ren, tex, &DOTS_TEX, &pos);
			}

			SDL_RenderTexture(ren, tex, &score1_tex, &score1);
			SDL_RenderTexture(ren, tex, &score2_tex, &score2);

			SDL_RenderPresent(ren);

			SDL_Delay(10);
		}

		SDL_Quit();
		return 0;
	}
private:
	static constexpr SDL_FRect BALL_TEX = {404, 580, 76, 76};
	static constexpr SDL_FRect PAD1_TEX = {360, 4, 64, 532};
	static constexpr SDL_FRect PAD2_TEX = {456, 4, 64, 532};
	static constexpr SDL_FRect DOTS_TEX = {296, 20, 24, 24};
	static constexpr SDL_FRect ZERO_TEX = {192, 628, 68, 100};
	static constexpr SDL_FRect ONE_TEX = {40, 20, 64, 100};
};
