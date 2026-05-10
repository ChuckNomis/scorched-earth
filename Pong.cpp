#include "Pong.h"
#include <SDL3_image/SDL_image.h>
#include <iostream>
using namespace std;

#include "bagel.h"
using namespace bagel;

namespace mta
{
	constexpr Drawable Pong::makeDrawable(SDL_FRect part) {
		return Drawable{{part}, {part.w*TEX_SCALE, part.h*TEX_SCALE}};
	}

	void Pong::resetBall(b2BodyId ballBody) const
	{
		float xs = 0.5f + SDL_randf()/2;
		float ys = SDL_randf() - 0.5f;

		b2Body_SetTransform(ballBody, {WIN_W/2/BOX_SCALE,WIN_H/2/BOX_SCALE}, b2MakeRot(0));
		b2Body_SetLinearVelocity(ballBody, {xs*30, ys*30});
	}
	void Pong::score_system() const
	{
		const auto se = b2World_GetSensorEvents(box);
		for (int i = 0; i < se.endCount; ++i) {
			// score, recreate ball
			const b2BodyId b = b2Shape_GetBody(se.endEvents[i].visitorShapeId);
			resetBall(b);
		}
	}
	void Pong::move_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<Intent>()
			.set<Collider>()
			.build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(mask)) {
				const auto& i = e.get<Intent>();
				const auto& c = e.get<Collider>();

				const float f = i.up ? -30 : i.down ? 30 : 0;
				b2Body_SetLinearVelocity(c.b, {0,f});
			}
		}
	}
	void Pong::input_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<Keys>()
			.set<Intent>()
			.build();

		SDL_PumpEvents();
		const bool* keys = SDL_GetKeyboardState(nullptr);

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(mask)) {
				const auto& k = e.get<Keys>();
				auto& i = e.get<Intent>();

				i.up = keys[k.up];
				i.down = keys[k.down];
			}
		}
	}
	void Pong::box_system() const
	{
		static constexpr float BOX_STEP = 1.f/FPS;
		static const Mask mask = MaskBuilder()
			.set<Transform>()
			.set<Collider>()
			.build();

		b2World_Step(box, BOX_STEP, 4);

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(mask)) {
				const auto t = b2Body_GetTransform(e.get<Collider>().b);
				e.get<Transform>() = {
					{t.p.x*BOX_SCALE, t.p.y*BOX_SCALE},
					RAD_TO_DEG * b2Rot_GetAngle(t.q)
				};
			}
		}
	}

	void Pong::draw_system() const
	{
		static const Mask mask = MaskBuilder()
			.set<Transform>()
			.set<Drawable>()
			.build();

		for (Entity e = Entity::first(); !e.eof(); e.next()) {
			if (e.test(mask)) {
				const auto& t = e.get<Transform>();
				const auto& d = e.get<Drawable>();

				SDL_FRect dest = {
					t.p.x-d.size.x/2,
					t.p.y-d.size.y/2,
					d.size.x, d.size.y };

				SDL_RenderTextureRotated(
					ren, tex, &d.part, &dest, t.a,
					nullptr, SDL_FLIP_NONE);
			}
		}
	}

	Pong::Pong() {
		constexpr float WIN_H_MID = WIN_H/2.f;
		constexpr float WIN_W_MID = WIN_W/2.f;

		// initialize SDL for video (drawing to screen)
		if (!SDL_Init(SDL_INIT_VIDEO)) {
			cout << SDL_GetError() << endl;
			return;
		}

		// create game window
		if (!SDL_CreateWindowAndRenderer(
			"Pong!", WIN_W, WIN_H, 0, &win, &ren)) {
			cout << SDL_GetError() << endl;
			return;
			}

		// load spritesheet image
		SDL_Surface *surf = IMG_Load("res/pong.png");
		if (surf == nullptr) {
			cout << SDL_GetError() << endl;
			return;
		}

		// create texture from spritesheet and destroy orig image
		tex = SDL_CreateTextureFromSurface(ren, surf);
		if (tex == nullptr) {
			cout << SDL_GetError() << endl;
			return;
		}
		SDL_DestroySurface(surf);

		// seed randomizer and set default clear color
		SDL_srand(time(nullptr));
		SDL_SetRenderDrawColor(ren, 0,0,0,255);

		// create Box2D world
		b2WorldDef worldDef = b2DefaultWorldDef();
		worldDef.gravity = {0,0};
		box = b2CreateWorld(&worldDef);
		if (!b2World_IsValid(box)) {
			cout << "Failed creating Box2D world" << endl;
			return;
		}

		// create center dots (only drawings)
		for (float i = 10; i < WIN_H; i += 24) {
			Entity::create().addAll(
				Transform{{WIN_W_MID, i},0},
				makeDrawable({296,20,24,24}));
		}

		// create top & bottom walls (no entities - they just block movement)
		b2BodyDef wallDef = b2DefaultBodyDef();
		wallDef.type = b2_staticBody;
		wallDef.position = {WIN_W_MID/BOX_SCALE,-1};
		// top wall
		b2ShapeDef wallShapeDef = b2DefaultShapeDef();
		wallShapeDef.density = 1;
		b2Polygon wallPoly = b2MakeBox(WIN_W_MID/BOX_SCALE, 1);
		b2BodyId wallBody = b2CreateBody(box, &wallDef);
		b2CreatePolygonShape(wallBody, &wallShapeDef, &wallPoly);
		// bottom wall
		wallDef.position.y = WIN_H/BOX_SCALE + 1;
		wallBody = b2CreateBody(box, &wallDef);
		b2CreatePolygonShape(wallBody, &wallShapeDef, &wallPoly);

		// create left and right score sensors (entities for detecting collisions)
		wallShapeDef.isSensor = true;
		wallShapeDef.enableSensorEvents = true;
		// left "wall" score sensor
		wallPoly = b2MakeBox(5, WIN_H_MID/BOX_SCALE);
		wallDef.position = {-5, WIN_H_MID/BOX_SCALE};
		wallBody = b2CreateBody(box, &wallDef);
		b2ShapeId wallShape = b2CreatePolygonShape(wallBody, &wallShapeDef, &wallPoly);
		Entity::create().add(Scorer{wallShape});
		// right "wall" score sensor
		wallDef.position = {WIN_W/BOX_SCALE+5, WIN_H_MID/BOX_SCALE};
		wallBody = b2CreateBody(box, &wallDef);
		wallShape = b2CreatePolygonShape(wallBody, &wallShapeDef, &wallPoly);
		Entity::create().add(Scorer{wallShape});

		// create ball Box2D body def & circle shape
		b2BodyDef ballDef = b2DefaultBodyDef();
		ballDef.type = b2_dynamicBody;
		ballDef.position = {WIN_W_MID/BOX_SCALE,WIN_H_MID/BOX_SCALE};
		b2ShapeDef ballShapeDef = b2DefaultShapeDef();
		ballShapeDef.enableSensorEvents = true;
		ballShapeDef.density = 1;
		ballShapeDef.material.friction = 0;
		ballShapeDef.material.restitution = 1.2f;
		b2Circle ballCircle = {{0,0},76*TEX_SCALE/BOX_SCALE/2};
		// create ball Box2D body and add to entity
		b2BodyId ballBody = b2CreateBody(box, &ballDef);
		b2CreateCircleShape(ballBody, &ballShapeDef, &ballCircle);
		resetBall(ballBody);

		// create ball entity
		Entity::create().addAll(
			Transform{{WIN_W_MID,WIN_H_MID}, 0},
			makeDrawable({404, 580, 76, 76}),
			Collider{ballBody});
		// set link from ball body to ball entity (user-data)
		//b2Body_SetUserData(ballBody, new ent_type{ball});

		// create paddles for both users (no AI)
		b2BodyDef padDef = b2DefaultBodyDef();
		padDef.type = b2_kinematicBody; // only moves by input
		padDef.position = { 50/BOX_SCALE, WIN_H_MID/BOX_SCALE };
		b2BodyId padBody = b2CreateBody(box, &padDef);

		b2ShapeDef padShapeDef = b2DefaultShapeDef();
		padShapeDef.density = 1;
		b2Polygon padPoly = b2MakeBox(64*TEX_SCALE/BOX_SCALE/2, 532*TEX_SCALE/BOX_SCALE/2);
		b2CreatePolygonShape(padBody, &padShapeDef, &padPoly);

		// first paddle (left player)
		Entity::create().addAll(
			Transform{{42,WIN_H_MID}, 0},
			makeDrawable({360, 4, 64, 532}),
			Collider{padBody},
			Intent{},
			Keys{SDL_SCANCODE_W, SDL_SCANCODE_S});

		// second paddle (right player)
		padDef.position = { (WIN_W-50)/BOX_SCALE, WIN_H_MID/BOX_SCALE };
		padBody = b2CreateBody(box, &padDef);
		padPoly = b2MakeBox(64*TEX_SCALE/BOX_SCALE/2, 532*TEX_SCALE/BOX_SCALE/2);
		b2CreatePolygonShape(padBody, &padShapeDef, &padPoly);
		// entity & components for 2nd paddle
		Entity::create().addAll(
			Transform{{WIN_W-42,WIN_H_MID}, 0},
			makeDrawable({456, 4, 64, 532}),
			Collider{padBody},
			Intent{},
			Keys{SDL_SCANCODE_UP, SDL_SCANCODE_DOWN});
	}

	Pong::~Pong()
	{
		if (b2World_IsValid(box))
			b2DestroyWorld(box);
		if (tex != nullptr)
			SDL_DestroyTexture(tex);
		if (ren != nullptr)
			SDL_DestroyRenderer(ren);
		if (win != nullptr)
			SDL_DestroyWindow(win);

		SDL_Quit();
	}

	void Pong::run()
	{
		auto start = SDL_GetTicks();
		bool quit = false;

		while (!quit) {
			// update all systems (except draw)
			input_system();
			move_system();
			box_system();
			score_system();

			SDL_RenderClear(ren);
			draw_system();
			SDL_RenderPresent(ren);

			const auto end = SDL_GetTicks();
			if (end-start < GAME_FRAME) {
				SDL_Delay(GAME_FRAME - (end-start));
			}
			start += GAME_FRAME;

			SDL_Event e;
			while (SDL_PollEvent(&e)) {
				if ((e.type==SDL_EVENT_QUIT) ||
					((e.type == SDL_EVENT_KEY_DOWN) && (e.key.scancode == SDL_SCANCODE_ESCAPE))) {
					quit = true;
				}
			}
		}
	}
}
