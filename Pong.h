#pragma once
#include <SDL3/SDL.h>
#include <box2d/box2d.h>
#include "bagel.h"

namespace mta
{
	using Transform = struct { SDL_FPoint p; float a; };
	using Drawable = struct { SDL_FRect part; SDL_FPoint size; };
	using Intent = struct { bool up, down; };
	using Keys = struct { SDL_Scancode up, down; };
	using Collider = struct { b2BodyId b; };
	using Scorer = struct { b2ShapeId s; };
}

template <> struct bagel::Storage<mta::Keys> final : NoInstance {
	using type = PackedStorage<mta::Keys>;
};
template <> struct bagel::Storage<mta::Intent> final : NoInstance {
	using type = PackedStorage<mta::Intent>;
};

namespace mta {
	class Pong
	{
	public:
		Pong();
		~Pong(); // not virtual

		void run();
		bool valid() const { return b2World_IsValid(box); }
	private:
		static constexpr int	WIN_W = 800;
		static constexpr int	WIN_H = 600;

		static constexpr int	FPS = 60;
		static constexpr Uint64	GAME_FRAME = 1000/FPS;
		static constexpr float	RAD_TO_DEG = 57.2958f;

		static constexpr float TEX_SCALE = 0.25f;
		static constexpr float BOX_SCALE = 10.f;

		void box_system() const;
		void input_system() const;
		void move_system() const;
		void score_system() const;
		void draw_system() const;
		void resetBall(b2BodyId) const;

		static constexpr Drawable makeDrawable(SDL_FRect part);

		SDL_Texture*		tex = nullptr;
		SDL_Renderer*		ren = nullptr;
		SDL_Window*			win = nullptr;
		b2WorldId			box = b2_nullWorldId;
	};
}
