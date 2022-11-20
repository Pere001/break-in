/*
*  2 player Breakout game made in Raylib.
*
*
*  Code overview:
*
*  - The code was structured in a way that served the game design process. I tried to keep
*  things simple and to minimize the amount of state required, so things are easy to
*  change and hard to break.
*
*  - If something isn't "elegant", it's because I didn't start with the final idea.
*
*  - All global state is in one global_state struct.
*
*  - I do operations in my own math types, then convert to Raylib's to call its functions.
*  
*  - For dynamic effects I tend to do some inline math with my high-level math utilities
*  that allow me to keep the code quite concise and quite free from comments. But to be
*  able to read it you might need a good intuition about Lerp(), Clamp(), %, and such
*  functions, as well as general linear relationships.
*
*  - I rely a bit on initialization to 0.
*
*  - I don't tend to put anything into a function that would only be called once.
*
*  - I don't use stupid C++ features.
*
*  - All the game code is in this file. Math utilites are at bi_math.h and basic utilities
*  are at bi_base.h.
*
*/


#ifndef PLATFORM_WEB
   #define PLATFORM_WEB // already defined in .bat
#endif

#include "raylib.h"
#include "rshapes.c"

#include "bi_base.h"
#include "bi_math.h"

#include <stdio.h>
#include <emscripten/emscripten.h>




//
// Raylib types conversions
//
inline v2 V2(Vector2 a){
	v2 result = {a.x, a.y};
	return result;
}
inline Vector2 Vector2_(v2 a){
	Vector2 result = {a.x, a.y};
	return result;
}
inline Vector2 Vector2_(f32 x, f32 y){
	Vector2 result = {x, y};
	return result;
}
inline Vector2 Vector2_(f32 xy){
	Vector2 result = {xy, xy};
	return result;
}
inline Rectangle Rectangle_(f32 x, f32 y, f32 w, f32 h){
	Rectangle result = {x, y, w, h};
	return result;
}
inline Rectangle Rectangle_(v2 pos, v2 dim){
	Rectangle result = {pos.x, pos.y, dim.x, dim.y};
	return result;
}
inline Rectangle Rectangle_(v2s pos, v2s dim){
	Rectangle result = {(f32)pos.x, (f32)pos.y, (f32)dim.x, (f32)dim.y};
	return result;
}
inline Rectangle Rectangle_(Vector2 pos, Vector2 dim){
	Rectangle result = {pos.x, pos.y, dim.x, dim.y};
	return result;
}
inline Color Color_(v4 a){
	Color result = {(u8)(Clamp01(a.r)*255), (u8)(Clamp01(a.g)*255), (u8)(Clamp01(a.b)*255), (u8)(Clamp01(a.a)*255)};
	return result;
}


//
// Game types
//

// Aka "screen"
enum meta_state{
	MetaState_MainMenu = 0,
	MetaState_Tutorial,
	MetaState_Options,
	MetaState_Game,
};

enum special_brick_type : u8{
	SpecialBrick_None = 0,
	SpecialBrick_Powerup,
	SpecialBrick_BadPowerup,
	SpecialBrick_Arrow,
	SpecialBrick_Spawner,
};
#define TILE_DRAW_MARGIN 3.f
struct tile_state{
	b32 occupied;
	special_brick_type specialType;
	f32 specialTypeTimer; // in seconds
	f32 specialAlpha; // Fade-in used to avoid unfairly placing a bad powerup right in front of the ball.
	v4 color;
};

#define DEFAULT_BALL_RADIUS 6.f
#define DEFAULT_BALL_SPEED 5.f
enum ball_flags{
	BallFlags_OnPaddle = 0x1, // Ball stuck on the paddle, waiting for player to press space.
	BallFlags_StuckShootRandomly = 0x2, // If set, stuck ball should start in a random direction.
	BallFlags_InRandomizer = 0x4, // Used to only count the randomizer collision once.
};
struct ball_state{
	v2 pos;
	v2 speed; // (pixels per frame)
	s32 flags;
	f32 positionOnPaddle; // [-1, 1] Only used when on paddle.
	f32 r; // radius.
};

struct brick_shape{
	// Each u8 represents a row, with each bit representing a position in that row.
	// The first subscript is 0 for occupied brick and 1 for "special".
	// E.g. ((rows[0][2] >> (7 - 3)) & 0x1) Evaluates to 1 if there is a solid brick at x=3, y=2.
	//      ((rows[1][2] >> (7 - 3)) & 0x1) Evaluates to 1 if that brick is a special brick (type dictated by specialType).
	u8 rows[2][8];
	special_brick_type specialType;
	v4 color;
};
struct brick_shape_slot{
	brick_shape shape;
	b32 occupied;
	v2s shapeDim; // In tiles
};

enum drop_type{
	// Good
	Drop_Life,
	Drop_ExtraBall,
	Drop_BigPaddle,
	Drop_Magnet,
	Drop_BigBalls,
	Drop_Barrier,
	Drop_TwoExtraBalls,

	// Bad
	Drop_FastBalls,
	Drop_SlowBalls,
	Drop_SmallPaddle,
	Drop_ReverseControls,
	Drop_SlipperyControls,
	Drop_Randomizer,
};
#define FIRST_GOOD_DROP Drop_Life
#define LAST_GOOD_DROP Drop_TwoExtraBalls
#define FIRST_BAD_DROP Drop_FastBalls
#define LAST_BAD_DROP Drop_Randomizer
#define DROP_RADIUS 15.f

#define DEFAULT_POWERUP_TIME 25.f
#define POWERUP_TIME_BIG_PADDLE        DEFAULT_POWERUP_TIME
#define POWERUP_TIME_MAGNET            30.f
#define POWERUP_TIME_BIG_BALLS         30.f
#define POWERUP_TIME_BARRIER           DEFAULT_POWERUP_TIME
#define POWERUP_TIME_FAST_BALLS        DEFAULT_POWERUP_TIME
#define POWERUP_TIME_SLOW_BALLS        DEFAULT_POWERUP_TIME
#define POWERUP_TIME_SMALL_PADDLE      DEFAULT_POWERUP_TIME
#define POWERUP_TIME_REVERSE_CONTROLS  DEFAULT_POWERUP_TIME
#define POWERUP_TIME_SLIPPERY_CONTROLS DEFAULT_POWERUP_TIME
#define POWERUP_TIME_RANDOMIZER        30.f

#define PADDLE_WIDTH_SMALL 50
#define PADDLE_WIDTH_NORMAL 90
#define PADDLE_WIDTH_BIG 130

struct drop_state{
	v2 pos;
	drop_type type;
	f32 ySpeed;
};

#define MAX_GAME_SPEED 2.f

#define DEFAULT_SAME_COLOR_COMBO_MAX 4
#define DEFAULT_MASTER_VOLUME .8f
#define DEFAULT_SPAWN_SHAPE_TIME 8.f
#define DEFAULT_DO_SPEED_UP false
#define DEFAULT_INITIAL_PADDLE_LIFES 3
#define DEFAULT_SPECIAL_BRICK_CHANCE .5f

struct global_state {
	Texture2D texMain;
	
	Sound sndBallHit1;
	Sound sndBallHit2;
	Sound sndHeavyBallHit1;
	Sound sndHeavyBallHit2;
	Sound sndBounce;
	Sound sndBreak;
	Sound sndPlace;
	Sound sndCantPlace;
	Sound sndPaddleHitsBall;
	Sound sndHurt;
	Sound sndWoot;
	Sound sndWreerp;
	Sound sndPreerw;
	Sound sndCombo[5];
	Sound sndPaddle;
	Sound sndWinPaddle;
	Sound sndWinBricks;

	f32 masterVolume;

	meta_state metaState;
	s32 tutorialPage;
	v2 winDim;
	v2 mousePos;

	u64 guiActiveId;
	u64 guiHoveredId;
	b32 guiKeepActive;


	v2s gridDim;
	v2 tileDim;
	v2 viewDim;
	v2 viewPos;
	tile_state *tiles;


	brick_shape shapeCatalog[7];
	f32 spawnShapeTime; // Seconds

	f32 randomizerY;
	f32 barrierTopY;
	f32 barrierHeight;

	b32 doSpeedUp;
	s32 sameColorComboMax;
	s32 initialPaddleLifes;
	f32 specialBrickChance;

	b32 autoPlaceShapes;

	////////////////////////////////////////////////////////////////////////////
	u8 membersBelowThisGetZeroedOnEveryNewGame;

	v2 paddleDim;
	f32 gameTime;
	f32 gameSpeed;
	f32 speedUpMessageTimer; // 0 for inactive. Non-zero shows "Speed up!" message.
	f32 speedUpMessageTime; 

	v2 paddlePos; // Center position
	f32 paddleXSpeed; // in pixels per frame (16.66ms frame)
	s32 paddleLastInputDir;

	ball_state balls[10];
	s32 numBalls;
	s32 paddleLifes;

	drop_state drops[20];
	s32 numDrops;

	brick_shape_slot availableSlots[2];
	brick_shape_slot nextSlots[2];
	f32 spawnShapeTimer; // Seconds

	s32 draggingShapeIndex; // -1 for default

	b32 gameEnded;
	b32 paddleWon; // (when gameEnded)  true: attacker won;  false: defender won
	b32 pause;

	// Powerups
	f32 powerupCountdownBigPaddle;
	f32 powerupCountdownMagnet;
	f32 powerupCountdownBigBalls;
	f32 powerupCountdownBarrier;
	// Bad Powerups
	f32 powerupCountdownFastBalls;
	f32 powerupCountdownSlowBalls;
	f32 powerupCountdownSmallPaddle;
	f32 powerupCountdownReverseControls;
	f32 powerupCountdownSlipperyControls;
	f32 powerupCountdownRandomizer;

	s32 sameColorCombo;
	v4 sameColorComboLastColor;

};
static global_state globalState;


void DrawSprite(v2 texPos, v2 texDim, v2 pos, v2 scale = V2(1.f), v4 color = V4_White()){
	Rectangle src = Rectangle_(texPos, texDim);
	Rectangle dest = Rectangle_(pos, Hadamard(texDim, scale));
	DrawTexturePro(globalState.texMain, src, dest, Vector2_(0), 0, Color_(color));
}
void DrawSpriteFit(v2 texPos, v2 texDim, v2 pos, v2 dim, v4 color = V4_White()){
	Rectangle src = Rectangle_(texPos, texDim);
	Rectangle dest = Rectangle_(pos, dim);
	DrawTexturePro(globalState.texMain, src, dest, Vector2_(0), 0, Color_(color));
}

void DrawRotateArrow(v2 pos, v2 dim, b32 flipX, v4 color){
#if 0
	v2 rectAPos = {.1f, .1f};
	v2 rectADim = {.7f, .2f};
	v2 rectBPos = {.6f, .3f};
	v2 rectBDim = {.2f, .4f};
	f32 triR = .3f;
	v2 tri0 = {.7f + triR, .7f};
	v2 tri1 = {.7f - triR, .7f};
	v2 tri2 = {.7f, .7f + triR};
	if (flipX){
		rectAPos.x = 1.f - rectAPos.x - rectADim.x;
		rectBPos.x = 1.f - rectBPos.x - rectBDim.x;
		tri0.x = 1.f - tri0.x;
		tri1.x = 1.f - tri1.x;
		tri2.x = 1.f - tri2.x;
		SWAP(tri0, tri1);
	}
	Color col = Color_(color);
	DrawRectangleV(Vector2_(pos + Hadamard(dim, rectAPos)), Vector2_(Hadamard(dim, rectADim)), col);
	DrawRectangleV(Vector2_(pos + Hadamard(dim, rectBPos)), Vector2_(Hadamard(dim, rectBDim)), col);
	DrawTriangle(Vector2_(pos + Hadamard(dim, tri0)), Vector2_(pos + Hadamard(dim, tri1)), Vector2_(pos + Hadamard(dim, tri2)), col);
#endif
	DrawSpriteFit(V2(32, 0), V2(48*(flipX ? -1 : 1), 48), pos, dim, color);
}

// style=0: Flat color
// style=1: Light outline and black inner outline.
// style=2: Rotate arrow CW
// style=3: Rotate arrow CCW
b32 DoButton(u64 id, v2 pos, v2 dim, char *text, v4 color, s32 style = 0){
	auto gs = &globalState;

	// Logic
	b32 result = false;
	b32 hovering = PointInRectangle(gs->mousePos, pos, pos + dim);
	if (gs->guiActiveId == id){
		if (IsMouseButtonDown(0)){
			gs->guiKeepActive = true;
		}else{
			gs->guiActiveId = 0;
			result = hovering;
		}	
	}
	if (!gs->guiActiveId){
		if (!gs->guiHoveredId && hovering){
			gs->guiHoveredId = id;
			if (IsMouseButtonPressed(0)){
				gs->guiActiveId = id;
				gs->guiKeepActive = true;
			}
		}
	}

	// Drawing
	if (gs->guiHoveredId == id){
		color = Clamp01V4(color + V4_Grey(.2f, 0));
	}else if (gs->guiActiveId == id){
		color = Clamp01V4(color + V4_Grey(-.2f, 0));
	}
	if (style == 1){
		DrawRectangleV(Vector2_(pos), Vector2_(dim), Color_(Clamp01V4(color*1.1f)));
		v2 m = {3.f, 3.f};
		DrawRectangleV(Vector2_(pos + m), Vector2_(dim - 2*m), BLACK);
		m += V2(3.f, 3.f);
		DrawRectangleV(Vector2_(pos + m), Vector2_(dim - 2*m), Color_(color));
	}else if (style == 2 || style == 3){
		DrawRectangleV(Vector2_(pos), Vector2_(dim), WHITE);
		v2 m = V2(3.f, 3.f);
		DrawRectangleV(Vector2_(pos + m), Vector2_(dim - 2*m), Color_(color));
		// Arrow
		v2 arrowDim = V2(24.f);
		v4 arrowColor = V4_Grey(.88f);
		if (gs->guiHoveredId == id){
			arrowColor = Clamp01V4(arrowColor + V4_Grey(.2f, 0));
		}else if (gs->guiActiveId == id){
			arrowColor = Clamp01V4(arrowColor + V4_Grey(-.2f, 0));
		}
		DrawRotateArrow(pos + dim/2 - arrowDim/2, arrowDim, (style == 3), arrowColor);
	}else{
		DrawRectangleV(Vector2_(pos), Vector2_(dim), Color_(color));
	}

	if (text){
		f32 fontSize = 20.f;
		f32 textWidth = (f32)MeasureText(text, (s32)fontSize); 
		v2 textPos = pos + dim/2 - V2(textWidth, fontSize)/2;
		DrawText(text, (s32)textPos.x, (s32)textPos.y, fontSize, BLACK);
	}
	return result;
}

// style=3: Rotate arrow CCW
s32 DoSliderS32(u64 id, v2 pos, v2 dim, char *text, s32 value, s32 min, s32 max, v4 color, s32 style = 0){
	auto gs = &globalState;

	// Logic
	DoButton(id, pos, dim, "", color, style);
	
	f32 m = 6.f;
	f32 barWidth = 12.f;
	s32 result = value;
	if (gs->guiActiveId == id){
		f32 t = Clamp01(SafeDivide1(gs->mousePos.x - (pos.x + m + barWidth/2), dim.x - 2*m - barWidth/2));
		result = ClampS32(min + (s32)Round(t*(max - min)), min, max);
	}

	// Draw
	f32 t = (value - min)/(f32)(max - min);
	f32 barX = pos.x + m + barWidth/2 + t*(dim.x - 2*m - barWidth);
	v4 barColor = V4(.2f, .66f, .7f, 1.f);
	DrawRectangleV(Vector2_(barX - barWidth/2, pos.y + m), Vector2_(barWidth, dim.y - 2*m), Color_(barColor));
	
	if (text){
		f32 fontSize = 20.f;
		f32 textWidth = (f32)MeasureText(text, (s32)fontSize); 
		v2 textPos = pos + dim/2 - V2(textWidth, fontSize)/2;
		DrawText(text, (s32)textPos.x, (s32)textPos.y, fontSize, BLACK);
	}

	return result;
}

void FinishFrameForGui(){
	auto gs = &globalState;
	if (gs->guiActiveId && !gs->guiKeepActive){
		gs->guiActiveId = 0;
	}
	gs->guiHoveredId = 0;
}            

void UpdateDrawFrame(void);     // Update and Draw one frame

#define MIN_PADDLE_BOUNCE_ANGLE (.3f*PI/2.f)
v2 BallPaddleBounceDir(ball_state *ball){
	auto gs = &globalState;

	f32 relativeX = ball->pos.x - gs->paddlePos.x;
	f32 t = Clamp(relativeX/(gs->paddleDim.x/2), -1.f, 1.f);
	t = Lerp(t, -1.f + Map01ToArcSin(.5f + t*.5f)*2.f, .4f); // Gives less angles to the center and more to the edges.
	v2 result = V2LengthDir(1.f, Lerp(PI + MIN_PADDLE_BOUNCE_ANGLE, 2*PI - MIN_PADDLE_BOUNCE_ANGLE, .5f + t*.5f));
	return result;
}


#define TILE_COLOR_RED    V4(1.f, .2f, .2f)
#define TILE_COLOR_ORANGE V4(1.f, .5f, .1f)
#define TILE_COLOR_YELLOW V4(.92f, .85f, .06f)
#define TILE_COLOR_GREEN  V4(.25f, .85f, .1f)
#define TILE_COLOR_BLUE   V4(.4f, .3f, 1.f)
#define TILE_COLOR_PURPLE V4(.8f, .4f, 1.f)

static double globalDebugLastGetTime;


//
// Main ,,
//
int main(void)
{
	auto gs = &globalState;
	ZeroStruct(gs);
	gs->winDim = V2(800, 450);
	gs->metaState = MetaState_MainMenu;

    
	InitWindow(gs->winDim.x, gs->winDim.y, "Break-In");
	InitAudioDevice();
	double randomSeed1 = GetTime();

	//
	// Load Assets
	//
	Image imgMain = LoadImage("resources/main.png");
	double randomSeed2 = GetTime();
	gs->texMain = LoadTextureFromImage(imgMain);
	double randomSeed3 = GetTime();
	
	gs->sndBallHit1 = LoadSound("resources/sounds/ping_pong_1.wav");
	gs->sndBallHit2 = LoadSound("resources/sounds/ping_pong_2.wav");
	gs->sndHeavyBallHit1 = LoadSound("resources/sounds/cricket_ball_1.wav");
	gs->sndHeavyBallHit2 = LoadSound("resources/sounds/cricket_ball_2.wav");
	gs->sndBounce = LoadSound("resources/sounds/bounce.wav");
	gs->sndBreak = LoadSound("resources/sounds/break_1.wav");
	gs->sndPlace = LoadSound("resources/sounds/place.ogg");
	SetSoundVolume(gs->sndPlace, .6f);
	gs->sndCantPlace = LoadSound("resources/sounds/cant_place.ogg");
	SetSoundVolume(gs->sndPlace, .8f);
	gs->sndPaddleHitsBall = LoadSound("resources/sounds/paddle_hits_ball.wav");
	gs->sndHurt = LoadSound("resources/sounds/raspy_hurt.wav");
	gs->sndWoot = LoadSound("resources/sounds/woot.wav");
	gs->sndPreerw = LoadSound("resources/sounds/preerw.wav");
	SetSoundVolume(gs->sndPreerw, .45f);
	gs->sndWreerp = LoadSound("resources/sounds/wreerp.wav");
	SetSoundVolume(gs->sndWreerp, .45f);
	gs->sndCombo[0] = LoadSound("resources/sounds/combo_1.ogg");
	gs->sndCombo[1] = LoadSound("resources/sounds/combo_2.ogg");
	gs->sndCombo[2] = LoadSound("resources/sounds/combo_3.ogg");
	gs->sndCombo[3] = LoadSound("resources/sounds/combo_4.ogg");
	gs->sndCombo[4] = LoadSound("resources/sounds/combo_5.ogg");
	gs->sndPaddle = LoadSound("resources/sounds/poing.ogg");
	SetSoundVolume(gs->sndPaddle, .6f);
	gs->sndWinPaddle = LoadSound("resources/sounds/win_paddle.ogg");
	SetSoundVolume(gs->sndWinPaddle, .6f);
	gs->sndWinBricks = LoadSound("resources/sounds/win_bricks.ogg");
	SetSoundVolume(gs->sndWinBricks, .6f);

	gs->masterVolume = DEFAULT_MASTER_VOLUME;
	SetMasterVolume(gs->masterVolume);

	// Set up RNGs
	u64 finalSeed1 = (*(u64 *)&randomSeed1) ^ (*(u64 *)&randomSeed2); 
	u64 finalSeed2 = (*(u64 *)&randomSeed1) ^ (*(u64 *)&randomSeed3); 
	PcgRandomSeed(finalSeed1, finalSeed2);
	SetRandomSeed((s32)finalSeed1);

	// Set up global state
	gs->gridDim = V2S(12, 12); // 20)
	gs->tileDim = V2(37, 19);
	gs->viewDim = V2(gs->tileDim.x*gs->gridDim.x, 440);
	gs->viewPos = (gs->winDim - gs->viewDim)/2;

	gs->tiles = (tile_state *)malloc(sizeof(tile_state)*gs->gridDim.x*gs->gridDim.y);
	
	gs->sameColorComboMax = DEFAULT_SAME_COLOR_COMBO_MAX;
	
	// Set up shapes 
	// Line
	gs->shapeCatalog[0].color = TILE_COLOR_YELLOW;
	gs->shapeCatalog[0].rows[0][0] = 0xF0; // @@@@ 
	
	// Square
	gs->shapeCatalog[1].color = TILE_COLOR_BLUE;
	gs->shapeCatalog[1].rows[0][0] = 0xC0; // @@ 
	gs->shapeCatalog[1].rows[0][1] = 0xC0; // @@ 
	
	// Stairs
	gs->shapeCatalog[2].color = TILE_COLOR_GREEN;
	gs->shapeCatalog[2].rows[0][0] = 0xC0; // @@ 
	gs->shapeCatalog[2].rows[0][1] = 0x60; //  @@ 

	// Triangle
	gs->shapeCatalog[3].color = TILE_COLOR_ORANGE;
	gs->shapeCatalog[3].rows[0][0] = 0x40; //  @ 
	gs->shapeCatalog[3].rows[0][1] = 0xE0; // @@@ 
	
	// L
	gs->shapeCatalog[4].color = TILE_COLOR_PURPLE;
	gs->shapeCatalog[4].rows[0][0] = 0xF0; // @@@@ 
	gs->shapeCatalog[4].rows[0][1] = 0x10; //    @ 
 
	// 2 Blocks
	gs->shapeCatalog[5].color = TILE_COLOR_RED;
	gs->shapeCatalog[5].rows[0][0] = 0xC0; // @@

	// 1 Blocks
	gs->shapeCatalog[6].color = TILE_COLOR_YELLOW;
	gs->shapeCatalog[6].rows[0][0] = 0x80; // @

	gs->spawnShapeTime = DEFAULT_SPAWN_SHAPE_TIME;
	gs->doSpeedUp = DEFAULT_DO_SPEED_UP;
	gs->initialPaddleLifes = DEFAULT_INITIAL_PADDLE_LIFES;
	gs->specialBrickChance = DEFAULT_SPECIAL_BRICK_CHANCE;

	globalDebugLastGetTime = GetTime();

    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);

    CloseWindow();
	CloseAudioDevice();
    return 0;
}

void DrawBrickSpecial(v2 brickPos, v2 brickDim, special_brick_type type, v4 brickColor, f32 alpha = 1.f){
	if (type == SpecialBrick_Powerup || type == SpecialBrick_BadPowerup){
		v4 color = (type == SpecialBrick_Powerup ? V4_White(alpha) : V4_Black(alpha));

		// Draw 4-point star sprite
		v2 texPos = V2(0);
		v2 texDim = V2(32);
		DrawSpriteFit(texPos, texDim, brickPos, brickDim, color);

	}else if (type == SpecialBrick_Arrow){
		Color color = Color_(V4_White(alpha));
		f32 t = (f32)fmod(GetTime(), .8)/.8f;

		v2 triDim = {Min(brickDim.x, 2*brickDim.y), brickDim.y};
		v2 triPos = brickPos + (brickDim - triDim)/2 + V2(0, brickDim.y*(-1.f + 2*t));
		v2 p0 = triPos + V2(triDim.x/2, triDim.y); // Bottom
		v2 p1 = triPos + V2(triDim.x, 0); // Top Right
		v2 p2 = triPos; // Top Left

		if (p0.y <= brickPos.y + brickDim.y && p1.y >= brickPos.y){
			DrawTriangle(Vector2_(p0), Vector2_(p1), Vector2_(p2), color);
		}else if (p1.y < brickPos.y){
			f32 l = Min(1.f, (p0.y - brickPos.y)/brickDim.y);
			DrawTriangle(Vector2_(p0), Vector2_(LerpV2(p0, p1, l)), Vector2_(LerpV2(p0, p2, l)), color);
		}else{
			f32 l = Min(1.f, (brickPos.y + brickDim.y - p1.y)/brickDim.y);
			DrawTriangle(Vector2_(LerpV2(p2, p0, l)), Vector2_(p1), Vector2_(p2), color);
			DrawTriangle(Vector2_(LerpV2(p2, p0, l)), Vector2_(LerpV2(p1, p0, l)), Vector2_(p1), color);
		}
	}else if (type == SpecialBrick_Spawner){
		DrawRectangleV(Vector2_(brickPos), Vector2_(brickDim), BLACK);

		v2 c = brickPos + brickDim/2;
		v2 r = brickDim/2;

		v4 colorInner = brickColor;
		v4 colorCorners = V4_White(alpha);
		double flickerPeriod = 1.0;
		if (fmod(GetTime(), flickerPeriod) > flickerPeriod/2){
			SWAP(colorInner, colorCorners);
		}

		// Inner diamond
		r *= .4f;
		DrawTriangle(Vector2_(c + V2(0, -r.y)), Vector2_(c + V2(-r.x, 0)), Vector2_(c + V2(0, r.y)), Color_(colorInner));
		DrawTriangle(Vector2_(c + V2(0, -r.y)), Vector2_(c + V2(0, r.y)), Vector2_(c + V2(r.x, 0 )), Color_(colorInner));

		// Corners
		r = brickDim/2;
		f32 m = .2f;
		DrawTriangle(Vector2_(c + -r), Vector2_(c + V2(-r.x, -m*r.y)), Vector2_(c + V2(-m*r.x, -r.y)), Color_(colorCorners));
		DrawTriangle(Vector2_(c + V2(-r.x, r.y)), Vector2_(c + V2(-m*r.x, r.y)), Vector2_(c + V2(-r.x, m*r.y)), Color_(colorCorners));
		DrawTriangle(Vector2_(c + r), Vector2_(c + V2(r.x, m*r.y)), Vector2_(c + V2(m*r.x, r.y)), Color_(colorCorners));
		DrawTriangle(Vector2_(c + V2(r.x, -r.y)), Vector2_(c + V2(m*r.x, -r.y)), Vector2_(c + V2(r.x, -m*r.y)), Color_(colorCorners));

	}
}

// Draws it centered.
void DrawBrickShape(brick_shape_slot *slot, v2 centerPos, f32 scale, f32 alpha = 1.f){
	auto gs = &globalState;
	v2 p = centerPos - Hadamard(V2(slot->shapeDim), gs->tileDim*scale)/2;
	v4 col = slot->shape.color;
	col.a = alpha;
	f32 m = TILE_DRAW_MARGIN;
	for(s32 y = 0; y < slot->shapeDim.y; y++){
		for(s32 x = 0; x < slot->shapeDim.x; x++){
			if (slot->shape.rows[0][y] & (0x1 << (7 - x))){
				DrawRectangleV(Vector2_(p + scale*V2(x*gs->tileDim.x + m, y*gs->tileDim.y + m)), Vector2_(scale*(gs->tileDim - V2(2*m))), Color_(col));
			}
			if (slot->shape.rows[1][y] & (0x1 << (7 - x))){
				DrawBrickSpecial(p + scale*V2(x*gs->tileDim.x + m, y*gs->tileDim.y + m), scale*(gs->tileDim - V2(2*m)), slot->shape.specialType, col);
			}
		}
	}
}



void FlipShapeSlot(brick_shape_slot *slot, b32 flipX, b32 flipY, b32 swapXY){
	auto *gs = &globalState;
	auto shape = &slot->shape;

	// Flip horizontally
	if (flipX){
		for(s32 i = 0; i < ArrayCount(shape->rows); i++){
			for(s32 j = 0; j < ArrayCount(shape->rows[i]); j++){
				u8 bits = shape->rows[i][j];
				shape->rows[i][j] = ((bits >> 7) & 0x01) |
									((bits >> 5) & 0x02) |
									((bits >> 3) & 0x04) |
									((bits >> 1) & 0x08) |
									((bits << 1) & 0x10) |
									((bits << 3) & 0x20) |
									((bits << 5) & 0x40) |
									((bits << 7) & 0x80);
			}
		}
	}
	// Flip vertically
	if (flipY){
		for(s32 i = 0; i < ArrayCount(shape->rows); i++){
			for(s32 j = 0; j < ArrayCount(shape->rows[i])/2; j++){
				SWAP(shape->rows[i][j], shape->rows[i][ArrayCount(shape->rows[i]) - 1 - j]);
			}
		}
	}
	// Rotate (Swap x and y)
	if (swapXY){
		for(s32 i = 0; i < ArrayCount(shape->rows); i++){
			Assert(ArrayCount(shape->rows[i]) == 8);
			for(s32 y = 0; y < 8; y++){
				for(s32 x = y + 1; x < 8; x++){
					u8 bitNE = (shape->rows[i][y] >> (7 - x)) & 0x1;
					u8 bitSW = (shape->rows[i][x] >> (7 - y)) & 0x1;
					SetOrUnsetFlag(shape->rows[i][y], 1 << (7 - x), bitSW);
					SetOrUnsetFlag(shape->rows[i][x], 1 << (7 - y), bitNE);
				}
			}
		}
	}

	// Move to top-left
	v2s offset = {0};
	for(s32 y = 0; y < ArrayCount(shape->rows[0]); y++){
		if (shape->rows[0][y])
			break;
		offset.y++;
	}
	offset.x = 8;
	for(s32 y = 0; y < ArrayCount(shape->rows[0]); y++){
		for(s32 x = 0; x < offset.x; x++){
			if (shape->rows[0][y] & (0x1 << (7 - x))){
				offset.x = x;
				break;
			}
		}
	}
	Assert(offset.x < 8 && offset.y < 8);
	for(s32 i = 0; i < ArrayCount(shape->rows); i++){
		for(s32 y = 0; y < ArrayCount(shape->rows[i]); y++){
			s32 ySrc = y + offset.y;
			shape->rows[i][y] = (ySrc < 8 ? (shape->rows[i][ySrc] << offset.x) : 0);
		}
	}

	// Get dim
	slot->shapeDim = V2S(0);
	for(s32 y = 0; y < ArrayCount(shape->rows[0]); y++){
		if (shape->rows[0][y]){
			slot->shapeDim.y = y + 1;
			for(s32 x = slot->shapeDim.x; x < 8; x++){
				if (shape->rows[0][y] & (1 << (7 - x)))
					slot->shapeDim.x = x + 1;
			}
		}
	}
}
void FillShapeSlot(brick_shape_slot *slot){
	auto gs = &globalState;
	slot->occupied = true;
	// Choose random shape
	slot->shape = gs->shapeCatalog[RandomS32(ArrayCount(gs->shapeCatalog) - 1)];

	FlipShapeSlot(slot, RandomU32(1), RandomU32(1), RandomU32(1));

	// Place powerup
	if (RandomChance(gs->specialBrickChance)){
		s32 numBricks = 0;
		for(s32 y = 0; y < slot->shapeDim.y; y++){
			for(s32 x = 0; x < slot->shapeDim.x; x++){
				numBricks += (s32)((slot->shape.rows[0][y] >> (7 - x)) & 0x1);
			}
		}
		if (numBricks){
			// Decide special
			s32 numSpecial = 1;
			if (RandomChance(.25f)){
				slot->shape.specialType = SpecialBrick_Arrow;
				numSpecial = RandomRangeS32(2, 3);
			}else if (RandomChance(.15f)){
				slot->shape.specialType = SpecialBrick_Spawner;
			}else{
				slot->shape.specialType = (RandomS32(1) ? SpecialBrick_Powerup : SpecialBrick_BadPowerup);
			}
			s32 numPlaced = 0;
			while(numPlaced < numSpecial && numPlaced < numBricks){
				s32 chosenBrick = RandomS32(numBricks - numPlaced - 1);
				for(s32 y = 0; y < slot->shapeDim.y; y++){
					for(s32 x = 0; x < slot->shapeDim.x; x++){
						if (slot->shape.rows[0][y] & (1 << (7 - x)) && !(slot->shape.rows[1][y] & (1 << (7 - x)))){
							if (chosenBrick == 0){
								SetFlag(slot->shape.rows[1][y], 1 << (7 - x));
								numPlaced++;
								goto LABEL_SpecialLoopEnd;
							}
							chosenBrick--;
						}
					}
				}
			LABEL_SpecialLoopEnd:
				u8 apparentlyWeNeedThisToMakeTheLabelOrSomething = 69;
			}
		}
	}
}
void RotateShape90Degrees(brick_shape_slot *slot, b32 clockwise){
	// The idea here is that combining the 3 easy operations we can do to the bits
	// (flip x, flip y, and swap x by y) we can accomplish a 90 degree CW and CCW rotations.
	// For 90 degree CCW we'll first flip the X and then swap X by Y.
	// For 90 degree CW we'll first flip the Y and then swap X by Y.

	if (clockwise){
		FlipShapeSlot(slot, 0, 1, 1);
	}else{
		FlipShapeSlot(slot, 1, 0, 1);
	}
}


drop_state *CreateDrop(v2 pos, drop_type type){
	auto gs = &globalState;
	drop_state *result = 0;
	if (gs->numDrops < ArrayCount(gs->drops)){
		result = &gs->drops[gs->numDrops];
		gs->numDrops++;

		ZeroStruct(result);
		result->pos = pos;
		result->type = type;
		if (type >= FIRST_GOOD_DROP && type <= LAST_GOOD_DROP){
			result->ySpeed = 2.f + RandomBilateral(.3f);
		}else{
			result->ySpeed = 1.5f + RandomBilateral(.2f);
		}
	}
	return result;
}

void DrawTextColorful(char *text, s32 x, s32 y, s32 fontSize, v4 *colors, s32 numColors){
	s32 xIt = x;
	s32 colorIndex = 0;
	for(char *it = text; *it; it++){
		char tempText[2] = {*it, '\0'};
		DrawText(tempText, xIt, y, fontSize, Color_(colors[colorIndex]));
		if (*it != ' ' && *it != '\n' && *it != '\r' && *it != '\t')
			colorIndex = (colorIndex + 1) % numColors;
		xIt += MeasureText(tempText, fontSize) + MeasureText("i", fontSize);
	}
}

v2 MeasureTextV2(char *text, s32 fontSize){
	s32 defaultSpacing = MaxS32(1, fontSize/10);
	v2 result = V2(MeasureTextEx(GetFontDefault(), text, fontSize, defaultSpacing));
	return result;
}

f32 BallRadius(){
	f32 result = DEFAULT_BALL_RADIUS;
	if (globalState.powerupCountdownBigBalls){
		result *= 2.f;
	}
	return result;
}
f32 BallSpeed(){
	f32 result = DEFAULT_BALL_SPEED;
	if (globalState.powerupCountdownFastBalls > globalState.powerupCountdownSlowBalls){
		result *= 1.5f;
	}else if (globalState.powerupCountdownSlowBalls > globalState.powerupCountdownFastBalls){
		result *= .66f;
	}
	return result;
}

void UpdateDrawFrame(void)
{

	f32 getFrameTime = GetFrameTime();
	f32 getTimeDelta;
	{
		double getTime = GetTime();
		getTimeDelta = (f32)(getTime - globalDebugLastGetTime);
		globalDebugLastGetTime = getTime;
	}

	// We need to use delta time because different browsers could update at different rates.
	// dt is used in places where we count seconds, as it's the time in seconds for this frame.
	// dtMul is used in places we originally assumed a frame was always 1/60 seconds. To easily fix
	// those incremental speeds and accelerations, we just multiply them by dtMul. Also thinking in
	// pixels per frame is sometimes easier than pixels per second...
	f32 dt = Clamp(getFrameTime, 1.f/144.f, 1.f/20.f);
	f32 dtMul = dt*60.f;



	auto gs = &globalState;
	gs->mousePos = V2(GetMousePosition());

	v4 defaultButtonColor = V4_Grey(.86f);
	v2 defaultButtonDim = V2(194.f, 40.f);

    // Update and Draw:
    BeginDrawing();

	if (gs->metaState == MetaState_MainMenu || gs->metaState == MetaState_Tutorial || gs->metaState == MetaState_Options){
		ClearBackground(BLACK);


		// Draw background effect
		{
			// Init random
			pcg_random_state random = { 0x853c49e6748fea9bULL, 0xda3e39cb94b95bdbULL };
			// Init colors
			v4 originalColors[] = { TILE_COLOR_RED, TILE_COLOR_ORANGE, TILE_COLOR_YELLOW, TILE_COLOR_GREEN, TILE_COLOR_BLUE, TILE_COLOR_PURPLE };
			v4 colors[40];
			for(s32 i = 0; i < ArrayCount(colors); i++){
				f32 t = (f32)(PcgRandomU32(&random) % 1000)/1000.f;
				f32 t2 = (f32)(PcgRandomU32(&random) % 1000)/1000.f;
				colors[i] = LerpV4(originalColors[i % ArrayCount(originalColors)], V4_Grey(.6f + t2*.2f, t), .2f + t*.3f);
			}
			u8 columns[13][30];
			for(s32 col = 0; col < ArrayCount(columns); col++){
				for(s32 i = 0; i < ArrayCount(columns[col]); i++){
					columns[col][i] = PcgRandomU32(&random) % ArrayCount(colors);
				}
			}

			f32 loopTime = (f32)fmod(GetTime(), 60.0*3.0)/(60.f*3.f);
			s32 iterationsPerColumn[] = { 4, -8, 6, -4, -7, 2, -3, 7, 8, 3, -9, -6, 2, 6, -5}; // negative for reverse speed
			f32 initialPosPerColumn[ArrayCount(columns)];
			for(s32 col = 0; col < ArrayCount(initialPosPerColumn); col++){
				initialPosPerColumn[col] = (f32)(PcgRandomU32(&random) % 1000)/1000.f;
			}
			// Draw
			v2 tileDim = {gs->winDim.x/(f32)ArrayCount(columns), 28.f};
			s32 tilesSeenPerColumn = (s32)Ceil(gs->winDim.y/tileDim.y) + 1;
			for(s32 col = 0; col < ArrayCount(columns); col++){
				s32 numIterations = iterationsPerColumn[col % ArrayCount(iterationsPerColumn)];
				f32 a = Clamp(Lerp(.05f, 1.f, Abs((col/(f32)(ArrayCount(columns) - 1))*2.f - 1.f)), .15f, .88f);

				if (gs->metaState == MetaState_Tutorial || gs->metaState == MetaState_Options){
					if (col > 0 && col < ArrayCount(columns) - 1)
						continue;
					numIterations = (col == 0 ? -4 : 4)*(gs->metaState == MetaState_Tutorial ? 1 : -1);
					a = .8f;
				}

				f32 t = Frac(loopTime*(f32)numIterations + initialPosPerColumn[col]);
				if (t < 0)
					t += 1.f;
				s32 numTiles = ArrayCount(columns[col]);
				s32 firstTile = (s32)(t*numTiles);
				f32 firstTileY = -Frac(t*numTiles)*tileDim.y;
				for(s32 i = 0; i < tilesSeenPerColumn; i++){
					s32 tileIndex = (firstTile + i) % numTiles;
					v2 tilePos = {col*tileDim.x, firstTileY + i*tileDim.y};
					f32 m = 5.f;
					DrawRectangleV(Vector2_(tilePos + V2(m)), Vector2_(tileDim - V2(2*m)), Fade(Color_(colors[columns[col][tileIndex]]), a));
				}
			}
		}
			
		if (gs->metaState == MetaState_MainMenu){
			// Draw Title
			char *titleText = "Break-In";
			f32 titleFontSize = 40;
			DrawText(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, 104, titleFontSize, BLACK);
			DrawText(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, 100, titleFontSize, WHITE);

			v2 buttonDim = defaultButtonDim;
			v2 buttonPos = (gs->winDim - buttonDim)/2 - V2(0, 20.f);
			if (DoButton(101, buttonPos, buttonDim, "Play", defaultButtonColor, 1) || IsKeyPressed(KEY_ENTER)){//V4(.8f, .6f, .5f))){
				gs->metaState = MetaState_Game;

				// Zero game variables region of global state.
				memset(&gs->membersBelowThisGetZeroedOnEveryNewGame, 0, sizeof(global_state) - (umm)&((global_state *)0)->membersBelowThisGetZeroedOnEveryNewGame);

				//
				// Init game state ,,
				//

				memset(gs->tiles, 0, sizeof(tile_state)*gs->gridDim.x*gs->gridDim.y);
				v4 colors[] = { TILE_COLOR_RED, TILE_COLOR_ORANGE, TILE_COLOR_YELLOW, TILE_COLOR_GREEN, TILE_COLOR_BLUE, TILE_COLOR_PURPLE };
				for(s32 y = 0; y < ArrayCount(colors); y++){
					for(s32 x = 0; x < gs->gridDim.x; x++){
						tile_state *tile = &gs->tiles[y*gs->gridDim.x + x];
						tile->color = colors[y];
						tile->occupied = true;
					}
				}
				gs->gameSpeed = 1.f;
				gs->speedUpMessageTime = 2.f;

				gs->paddleDim = V2(PADDLE_WIDTH_NORMAL, 10);
				gs->paddlePos = V2(gs->viewDim.x/2, gs->viewDim.y - 30);
				gs->paddleLifes = gs->initialPaddleLifes;

				gs->randomizerY = gs->paddlePos.y*.6f;
				gs->barrierTopY = gs->paddlePos.y + 16.f;
				gs->barrierHeight = 8.f;

				gs->numBalls = 1;
				gs->balls[0].flags = BallFlags_OnPaddle | BallFlags_StuckShootRandomly;
				gs->balls[0].r = DEFAULT_BALL_RADIUS;

				for(s32 i = 0; i < ArrayCount(gs->nextSlots); i++){
					FillShapeSlot(&gs->nextSlots[i]);
				}

				gs->draggingShapeIndex = -1;
			}
			buttonPos.y += 60.f;
			if (DoButton(102, buttonPos, buttonDim, "Options", defaultButtonColor, 1)){// V4(.8f, .6f, .5f))){
				gs->metaState = MetaState_Options;
			}
			buttonPos.y += 60.f;
			if (DoButton(103, buttonPos, buttonDim, "How to Play", defaultButtonColor, 1)){// V4(.8f, .6f, .5f))){
				gs->metaState = MetaState_Tutorial;
			}

		}else if (gs->metaState == MetaState_Tutorial){
			char *titleText = "How to Play";
			f32 titleFontSize = 30;
			DrawText(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, 26, titleFontSize, Color_(V4_Grey(.66f)));

			const s32 numTabs = 4;
			char *tabNames[numTabs] = {"Basics", "Bricks", "Combo", "Powerups"};
			f32 tabSepX = 40.f;
			f32 tabMarginX = 90.f;
			v2 tabDim = { (gs->winDim.x - 2*tabMarginX - (numTabs - 1)*tabSepX)/numTabs, defaultButtonDim.y };
			f32 tabY = 70;
			
			v4 colorTabActive = defaultButtonColor;
			v4 colorTabInactive = defaultButtonColor*.5f;
			colorTabInactive.a = 1.f;
			
			f32 lineMarginX = 64.f;
			{ // Tab line
				f32 m = 9.f; // y offset
				f32 h = 3.f; // line height
				DrawRectangleV(Vector2_(lineMarginX, tabY + tabDim.y - m), Vector2_(gs->winDim.x - 2*lineMarginX, h), Color_(colorTabActive));
			}
			// Draw tabs
			s32 id = 250;
			for(s32 tabIndex = 0; tabIndex < numTabs; tabIndex++){
				v2 tabPos = { tabMarginX + tabIndex*(tabDim.x + tabSepX), tabY };
				v4 color = (tabIndex == gs->tutorialPage ? colorTabActive : colorTabInactive);
				if (DoButton(++id, tabPos, tabDim, tabNames[tabIndex], color, 1)){
					gs->tutorialPage = tabIndex;
				}
				if (tabIndex == gs->tutorialPage){
					f32 m = 6.f;
					f32 h = m + 1;
					DrawRectangleV(Vector2_(tabPos + V2(0, tabDim.y - m)), Vector2_(tabDim.x, h), BLACK);
				}else{
					f32 m = 12.f;
					f32 h = m + 1;
					DrawRectangleV(Vector2_(tabPos + V2(0, tabDim.y - m)), Vector2_(tabDim.x, h), BLACK);
					m = 9.f;
					h = 3.f;
					DrawRectangleV(Vector2_(tabPos + V2(0, tabDim.y - m)), Vector2_(tabDim.x, h), Color_(colorTabActive));
				}
			}
			{ // Tab line
				//f32 m = 12.f;//6.f; // y offset
				//f32 h = m + 1; // line height
				//DrawRectangleV(Vector2_(lineMarginX, tabY + tabDim.y - m), Vector2_(gs->winDim.x - 2*lineMarginX, h), BLACK);
			}
			f32 pageY = tabY + tabDim.y + 10.f;
			f32 pageMarginX = 80;
			v4 textColor = V4_Grey(.96f);
			if (gs->tutorialPage == 0){ // Basics
				char *text[] = {"Player 1 controls the paddle with Arrow Keys + Space and\n\
has to break bricks to reach the top of the screen.",
								"Player 2 has to defend by placing brick shapes with the\n\
mouse, which appear periodically on the right side bar.",
								"If a ball reaches the top of the screen Player 1 wins.",
								"If Player 1 loses all balls Player 2 wins." };
				f32 fontSize = 20.f;
				f32 y = pageY;
				f32 x = 200;
				for(s32 i = 0; i < ArrayCount(text); i++){
					x = Min(x, gs->winDim.x/2 - MeasureText(text[i], fontSize)/2);
				}
				for(s32 i = 0; i < ArrayCount(text); i++){
					DrawText(text[i], x, y, fontSize, Color_(textColor));
					y += MeasureTextV2(text[i], fontSize).y + 24.f;
				}

			}else if (gs->tutorialPage == 1){ // Bricks
				char *text = "\
Some bricks have special effects. Most fade out after a\n\
while if they haven't been broken, becoming normal bricks."; 
				f32 fontSize = 20.f;
				f32 textX = (gs->winDim.x - MeasureText(text, fontSize))/2;
				DrawText(text, textX, pageY + 0, fontSize, Color_(textColor));
			
				special_brick_type specialTypes[] = {SpecialBrick_Powerup, SpecialBrick_BadPowerup, SpecialBrick_Arrow, SpecialBrick_Spawner};
				for(s32 i = 0; i < ArrayCount(specialTypes); i++){
					v2 p = {textX, pageY + 78 + 40*i};
					v2 brickDim = gs->tileDim;
					v2 brickPos = p + V2(0, (fontSize - brickDim.y)/2);
					v4 brickColor = TILE_COLOR_ORANGE;
					DrawRectangleV(Vector2_(brickPos), Vector2_(brickDim), Color_(brickColor));
					DrawBrickSpecial(brickPos, brickDim, specialTypes[i], brickColor, 1.f);

					char *description = "";
					if (specialTypes[i] == SpecialBrick_Powerup){
						description = "Drops a powerup if broken.";
					}else if (specialTypes[i] == SpecialBrick_BadPowerup){
						description = "Drops a powerdown if broken.";
					}else if (specialTypes[i] == SpecialBrick_Arrow){
						description = "Pushes the ball down. Can only be broken from above.";
					}else if (specialTypes[i] == SpecialBrick_Spawner){
						description = "Spawns bricks around it. Doesn't fade out.";
					}
					v2 textPos = p + V2(brickDim.x + 20.f, 0);
					DrawText(description, textPos.x, textPos.y, fontSize, WHITE);
				}
			}else if (gs->tutorialPage == 2){
				char *text = "\
Breaking 2 bricks of the same color consecutively will\n\
activate a combo. Every 4th brick will drop a powerup.\n\
But if you break a brick of another color, the combo\n\
will break and the count will start over. The pitch of\n\
the breaking sound shows the progress of the combo.\n\
\n\
You can configure this and other features to your\n\
liking in the options menu.";
				f32 fontSize = 20.f;
				DrawText(text, (gs->winDim.x - MeasureText(text, fontSize))/2, pageY, fontSize, Color_(textColor));
			
			}else if (gs->tutorialPage == 3){
				f32 fontSize = 20.f;
				char *text = "Powerups";
				DrawText(text, pageMarginX + (gs->winDim.x - 2*pageMarginX)*.25f - MeasureText(text, fontSize)/2, pageY, fontSize, WHITE);
				text = "Powerdowns";
				DrawText(text, pageMarginX + (gs->winDim.x - 2*pageMarginX)*.75f - MeasureText(text, fontSize)/2, pageY, fontSize, Color_(V4(1.f, .2f, .2f)));

				char *powerupTexts[] = {"Extra life", "Extra ball", "Bigger paddle", "Ball sticks to paddle", "Bigger balls", "Barrier"};
				char *powerdownTexts[] = {"Faster balls", "Slower balls", "Smaller paddle", "Reversed controls", "Slippery movement", "Speed randomizer"};
				f32 powerupY = pageY + 24.f;
				for(s32 i = 0; i < 6; i++){
					f32 xMargin = 12.f;
					f32 y = powerupY + i*35.f;
					// Good
					drop_type type = (drop_type)((s32)FIRST_GOOD_DROP + i);
					if (type <= LAST_GOOD_DROP){
						v2 pos = {pageMarginX + xMargin, y};
						v2 texDim = V2(32);
						v2 texPos = V2(i*texDim.x, 48.f);
						f32 spriteScale = 1.f;
						DrawSprite(texPos, texDim, pos, V2(spriteScale));

						v2 textPos = pos + V2(texDim.x*spriteScale + 7.f, (texDim.y*spriteScale - fontSize)/2);
						DrawText(powerupTexts[(s32)(type - FIRST_GOOD_DROP)], textPos.x, textPos.y, fontSize, Color_(textColor));
					}
					// Bad
					type = (drop_type)((s32)FIRST_BAD_DROP + i);
					if (type <= LAST_BAD_DROP){
						v2 pos = {gs->winDim.x/2 + xMargin, y};
						v2 texDim = V2(32);
						v2 texPos = V2(i*texDim.x, 80.f);
						f32 spriteScale = 1.f;
						DrawSprite(texPos, texDim, pos, V2(spriteScale));
						
						v2 textPos = pos + V2(texDim.x*spriteScale + 7.f, (texDim.y*spriteScale - fontSize)/2);
						DrawText(powerdownTexts[(s32)(type - FIRST_BAD_DROP)], textPos.x, textPos.y, fontSize, Color_(textColor));
					}
				}
			}
		
			v2 buttonDim = defaultButtonDim;
			v2 buttonPos = {(gs->winDim.x - buttonDim.x)/2, gs->winDim.y - 40 - buttonDim.y};
			if (DoButton(201, buttonPos, buttonDim, "Back to Menu", defaultButtonColor, 1) || IsKeyPressed(KEY_ESCAPE)){
				gs->metaState = MetaState_MainMenu;
			}

			
		}else if (gs->metaState == MetaState_Options){
			char *titleText = "Options";
			f32 titleFontSize = 30;
			v4 titleColor = V4_Grey(.66f);
			DrawText(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, 30, titleFontSize, Color_(titleColor));

			char hint[100] = "";
		
			v2 widgetDim = {260.f, defaultButtonDim.y};
			f32 xSep = 48.f;
			f32 ySep = 20.f;
			v2 widgetPos = {gs->winDim.x/2 - widgetDim.x - xSep/2, 124.f};
			u64 id = 231;
			{ // Lifes
				char text[50];
				sprintf(text, "Initial Lifes: %i", gs->initialPaddleLifes);
				gs->initialPaddleLifes = DoSliderS32(++id, widgetPos, widgetDim, text, gs->initialPaddleLifes, 0, 10, defaultButtonColor, 1);
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Initial paddle lifes.");
				}
				
			}
			widgetPos.x = gs->winDim.x/2 + xSep/2;
			{ // Shape frequency
				s32 prevValue = (s32)Round(gs->spawnShapeTime);
				char text[50];
				sprintf(text, "Shape Period: %is", prevValue);
				s32 newValue = DoSliderS32(++id, widgetPos, widgetDim, text, prevValue, 3, 13, defaultButtonColor, 1);
				if (newValue != prevValue){
					gs->spawnShapeTime = (f32)newValue;
				}
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "How often new shapes appear.");
				}
			}
			widgetPos = V2(gs->winDim.x/2 - widgetDim.x - xSep/2, widgetPos.y + widgetDim.y + ySep);
			{ // Color combo max
				char text[50];
				if (gs->sameColorComboMax){
					sprintf(text, "Color Combo: %i", gs->sameColorComboMax);
				}else{
					sprintf(text, "Color Combo: NO");
				}
				gs->sameColorComboMax = DoSliderS32(++id, widgetPos, widgetDim, text, gs->sameColorComboMax, 0, 5, defaultButtonColor, 1);
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Number of same-colored bricks to break to complete a combo.");
				}
			}
			widgetPos.x = gs->winDim.x/2 + xSep/2;
			{ // Special chance
				s32 prevValue = Round(Clamp01(gs->specialBrickChance)*10);
				char text[50];
				sprintf(text, "Special Brick: %i%%", prevValue*10);
				s32 newValue = DoSliderS32(++id, widgetPos, widgetDim, text, prevValue, 0, 10, defaultButtonColor, 1);
				if (newValue != prevValue){
					gs->specialBrickChance = Clamp01(newValue/10.f);
				}
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Chance of a special brick appearing in a shape.");
				}
			}
			widgetPos = V2(gs->winDim.x/2 - widgetDim.x - xSep/2, widgetPos.y + widgetDim.y + ySep);
			{ // Speed up
				char text[50];
				sprintf(text, "Speed Up: %s", (gs->doSpeedUp ? "YES" : "NO"));
				if (DoButton(++id, widgetPos, widgetDim, text, defaultButtonColor, 1)){
					gs->doSpeedUp = !gs->doSpeedUp;
				}
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Speed up all mechanics every minute.");
				}
			}
			widgetPos.x = gs->winDim.x/2 + xSep/2;
			{ // Adversarial AI
				char text[50];
				sprintf(text, "Bricks AI: %s", (gs->autoPlaceShapes ? "YES" : "NO"));
				if (DoButton(++id, widgetPos, widgetDim, text, defaultButtonColor, 1)){
					gs->autoPlaceShapes = !gs->autoPlaceShapes;
				}
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Place shapes automatically (i.e. single-player mode).");
				}
			}
			widgetPos = V2(gs->winDim.x/2 - widgetDim.x - xSep/2, widgetPos.y + widgetDim.y + ySep);
			{ // Volume
				s32 prevVolume = Round(Clamp01(gs->masterVolume)*10);
				char text[50];
				sprintf(text, "Sound Volume: %i%%", prevVolume*10);
				b32 wasActive = (gs->guiActiveId == id + 1);
				s32 newVolume = DoSliderS32(++id, widgetPos, widgetDim, text, prevVolume, 0, 10, defaultButtonColor, 1);
				if (newVolume != prevVolume){
					gs->masterVolume = Clamp01(newVolume/10.f);
					SetMasterVolume(gs->masterVolume);
					PlaySoundMulti(gs->sndCombo[RandomRangeS32(0, ArrayCount(gs->sndCombo) - 2)]);
				}
				if (gs->guiActiveId != id && wasActive){
					PlaySoundMulti(gs->sndCombo[RandomRangeS32(0, ArrayCount(gs->sndCombo) - 2)]);
				}
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Sound and music volume.");
				}
			}
			widgetPos.x = gs->winDim.x/2 + xSep/2;
			{ // Defaults
				if (DoButton(++id, widgetPos, widgetDim, "Revert to Defaults", defaultButtonColor, 1)){
					gs->sameColorComboMax = DEFAULT_SAME_COLOR_COMBO_MAX;
					gs->masterVolume = DEFAULT_MASTER_VOLUME;
					gs->spawnShapeTime = DEFAULT_SPAWN_SHAPE_TIME;
					gs->doSpeedUp = DEFAULT_DO_SPEED_UP;
					gs->initialPaddleLifes = DEFAULT_INITIAL_PADDLE_LIFES;
					gs->specialBrickChance = DEFAULT_SPECIAL_BRICK_CHANCE;
					gs->autoPlaceShapes = false;
				}
				if (gs->guiHoveredId == id || gs->guiActiveId == id){
					strcpy(hint, "Reset all configuration.");
				}
			}
			
			// Hint
			v4 hintColor = V4_Grey(.96f);
			if (!hint[0]){
				strcpy(hint, "Configure the game as you wish.");
				hintColor = titleColor;
			}
			f32 fontSize = 20.f;
			DrawText(hint, (gs->winDim.x - MeasureText(hint, fontSize))/2, 80, fontSize, Color_(hintColor));


			v2 buttonDim = defaultButtonDim;
			v2 buttonPos = {(gs->winDim.x - buttonDim.x)/2, gs->winDim.y - 40 - buttonDim.y};
			if (DoButton(221, buttonPos, buttonDim, "Back to Menu", defaultButtonColor, 1) || IsKeyPressed(KEY_ESCAPE)){
				gs->metaState = MetaState_MainMenu;
			}
		}
	}else if (gs->metaState == MetaState_Game){

		v2 slotDim = V2(75.f);
		v2 slotPos0;
		f32 slotPosOffsetY = slotDim.y + 10.f;
		{
			v2 regionPos = {gs->viewPos.x + gs->viewDim.x, 0};
			v2 regionDim = MaxV2(V2(0), gs->winDim - regionPos);
			slotPos0 = regionPos + V2(regionDim.x/2 - slotDim.x/2, 30.f);
		}

		// Pause
		if (IsKeyPressed(KEY_ESCAPE)){
			if (gs->pause){
				gs->pause = false;
			}else if (!gs->gameEnded){
				gs->pause = true;
			}
		}

		//
		// Update
		//
		b32 freeze = (gs->pause || gs->gameEnded);

		// Drag shape
		v2s draggingShapeTilePos = {};
		b32 isDraggingShapeOnWorld = false;
		b32 isDraggingShapePosValid = false;
		f32 gameTimePrev = gs->gameTime;
		if (freeze){
			gs->draggingShapeIndex = -1;
		}else{ // Update frame normally
			gs->gameTime += dt;

			// Speed up game speed every minute.
			if (gs->doSpeedUp){
				f32 gameSpeedPrev = gs->gameSpeed;
				gs->gameSpeed = Min(MAX_GAME_SPEED, 1.f + .1f*Floor(gs->gameTime/60));
				if (gs->gameSpeed != gameSpeedPrev){
					gs->speedUpMessageTimer += dt;
				}else if (gs->speedUpMessageTimer){
					gs->speedUpMessageTimer += dt;
					if (gs->speedUpMessageTimer > gs->speedUpMessageTime)
						gs->speedUpMessageTimer = 0;
				}
			}

			// Reduce powerup timers
			gs->powerupCountdownBigPaddle        = Max(0, gs->powerupCountdownBigPaddle        - dt*gs->gameSpeed);
			gs->powerupCountdownMagnet           = Max(0, gs->powerupCountdownMagnet           - dt*gs->gameSpeed);
			gs->powerupCountdownBigBalls         = Max(0, gs->powerupCountdownBigBalls         - dt*gs->gameSpeed);
			gs->powerupCountdownBarrier          = Max(0, gs->powerupCountdownBarrier          - dt*gs->gameSpeed);
			gs->powerupCountdownFastBalls        = Max(0, gs->powerupCountdownFastBalls        - dt*gs->gameSpeed);
			gs->powerupCountdownSlowBalls        = Max(0, gs->powerupCountdownSlowBalls        - dt*gs->gameSpeed);
			gs->powerupCountdownSmallPaddle      = Max(0, gs->powerupCountdownSmallPaddle      - dt*gs->gameSpeed);
			gs->powerupCountdownReverseControls  = Max(0, gs->powerupCountdownReverseControls  - dt*gs->gameSpeed);
			gs->powerupCountdownSlipperyControls = Max(0, gs->powerupCountdownSlipperyControls - dt*gs->gameSpeed);
			gs->powerupCountdownRandomizer       = Max(0, gs->powerupCountdownRandomizer       - dt*gs->gameSpeed);

			if (gs->powerupCountdownSmallPaddle > gs->powerupCountdownBigPaddle){
				gs->paddleDim.x = PADDLE_WIDTH_SMALL;
			}else if (gs->powerupCountdownBigPaddle > gs->powerupCountdownSmallPaddle){
				gs->paddleDim.x = PADDLE_WIDTH_BIG;
			}else{
				gs->paddleDim.x = PADDLE_WIDTH_NORMAL;
			}

			if (gs->draggingShapeIndex == -1){
				if (IsMouseButtonPressed(0)){
					for(s32 i = 0; i < ArrayCount(gs->availableSlots); i++){
						if (!gs->availableSlots[i].occupied)
							continue;
						v2 slotPos = slotPos0 + V2(0, slotPosOffsetY*i);
						if (PointInRectangle(gs->mousePos, slotPos, slotPos + slotDim)){
							gs->draggingShapeIndex = i;
							break;
						}
					}
				}else if (gs->autoPlaceShapes){
					//
					// AI that places shapes automatically
					//
					brick_shape_slot *slot = 0;
					s32 preferredSlotIndex = RandomS32(ArrayCount(gs->availableSlots) - 1);
					for(s32 i = 0; i < ArrayCount(gs->availableSlots); i++){
						s32 index = (preferredSlotIndex + i) % ArrayCount(gs->availableSlots);
						if (gs->availableSlots[index].occupied){
							slot = &gs->availableSlots[index];
							break;
						}
					}
					if (slot){
						// Figure out if there's a deep hole exposing the top of the screen.
						// In that case we'll try harder to cover that region.
						b32 emergency = false;
						s32 emergencyX = 0;
						s32 xStart = RandomS32(gs->gridDim.x - 1); // Start the loop at a random x to avoid always focusing on the hole in the lowest x, because we break when we find the first hole.
						for(s32 initialY = 0; initialY < 2; initialY++){
							for(s32 i = 0; i < gs->gridDim.x; i++){
								s32 x = (xStart + i) % gs->gridDim.x;
								emergency = true;
								for(s32 y = initialY; y < gs->gridDim.y; y++){
									if (gs->tiles[y*gs->gridDim.x + x].occupied){
										emergency = false;
										break;
									}
								}
								if (emergency){
									emergencyX = x;
									break;
								}
							}
							if (emergency)
								break;
						}

						brick_shape_slot slotBest = *slot;
						v2s bestShapePos = {0};
						f32 bestHeuristic = 0;
						special_brick_type special = slot->shape.specialType;
						for(s32 tries = 0; tries < 25; tries++){
							// Try to place the shape in a random position and random rotation.
							// We do that multiple times and take the best position (higher heuristic).
							brick_shape_slot slotTry = *slot;
							for(s32 rotations = RandomS32(3); rotations > 0; rotations--){
								RotateShape90Degrees(&slotTry, 0); 
							}
							v2s shapePosMin = V2S(0);
							v2s shapePosMax = gs->gridDim - slotTry.shapeDim;
							v2s shapePos = {RandomRangeS32(shapePosMin.x, shapePosMax.x), RandomRangeS32(shapePosMin.y, shapePosMax.y)};
							f32 emergencyHeuristic = 0;
							if (emergency){
								if (RandomChance(.6f)){ // Increase probability of spawning with a brick at x=emergencyX
									shapePos.x = ClampS32(emergencyX - RandomS32(slotTry.shapeDim.x - 1), shapePosMin.x, shapePosMax.x);
									emergencyHeuristic += .7f;
								}
								if (RandomChance(.4f)){ // Increase probability of spawning close to y=0
									shapePos.y = ClampS32((s32)(5*Square(Square(Random01()))), shapePosMin.y, shapePosMax.y);
								}
								// In an emergency, count being close to 0 more.
								emergencyHeuristic += .3f*Square(1.f - Clamp01(shapePos.y/4.f));
							}
							// Check collision
							b32 free = true;
							for(s32 y = 0; y < slotTry.shapeDim.y; y++){
								for(s32 x = 0; x < slotTry.shapeDim.x; x++){
									if (slotTry.shape.rows[0][y] & (1 << (7 - x))){
										v2s tilePos = shapePos + V2S(x, y);
										if (gs->tiles[tilePos.y*gs->gridDim.x + tilePos.x].occupied){
											free = false;
										}
									}
								}
							}
							if (free){
								// Find fraction of empty adjancent tiles
								s32 numFreeAdjacentTiles = 0;
								s32 numFullAdjacentTiles = 0;
								f32 heuristicSpecialPlacement = 0;
								f32 specialHeuristicStrength = 0;
								for(s32 y = 0; y < slotTry.shapeDim.y; y++){
									for(s32 x = 0; x < slotTry.shapeDim.x; x++){
										if (slotTry.shape.rows[0][y] & (1 << (7 - x))){ // Tile in shape
											s32 numFreeAdjacent = 0;
											s32 numFullAdjacent = 0;
											for(s32 i = 0; i < 4; i++){
												// Test the 4 adjacent tiles
												v2s offsets[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

												b32 inShape = false;
												v2s inShapePos = V2S(x, y) + offsets[i];
												if (inShapePos.x >= 0 && inShapePos.y >= 0 && inShapePos.x < slotTry.shapeDim.x && inShapePos.y < slotTry.shapeDim.y){
													inShape = (slotTry.shape.rows[0][inShapePos.y] & (1 << (7 - inShapePos.x)));
												}
												if (inShape)
													continue; // Ignore tiles in the same shape.

												// Check if it's full or free
												v2s tilePos = shapePos + inShapePos;
												b32 full = true;
												if (tilePos.y >= gs->gridDim.y){
													full = false;
												}else if (tilePos.x >= 0 && tilePos.y >= 0 && tilePos.x < gs->gridDim.x && tilePos.y < gs->gridDim.y){
													full = gs->tiles[tilePos.y*gs->gridDim.x + tilePos.x].occupied;
												}
												if (full){
													numFullAdjacent++;
												}else{
													numFreeAdjacent++;
												}
											}
											numFreeAdjacentTiles += numFreeAdjacent;
											numFullAdjacentTiles += numFullAdjacent;

											// Special brick heuristic.
											if (special && slotTry.shape.rows[1][y] & (1 << (7 - x))){
												b32 tileBelowIsFree = true;
												v2s tileBelow = shapePos + V2S(x, y + 1);
												if (tileBelow.y < gs->gridDim.y){
													tileBelowIsFree = !gs->tiles[tileBelow.y*gs->gridDim.x + tileBelow.x].occupied;
													if (tileBelowIsFree && y + 1 < slotTry.shapeDim.y) // Check if tile below is occupied by a tile in the same shape
														tileBelowIsFree = !(slotTry.shape.rows[0][y + 1] & (1 << (7 - x)));
												}
												b32 tileAboveIsFree = false;
												v2s tileAbove = shapePos + V2S(x, y - 1);
												if (tileAbove.y >= 0){
													tileAboveIsFree = !gs->tiles[tileAbove.y*gs->gridDim.x + tileAbove.x].occupied;
													if (tileAboveIsFree && y - 1 >= 0) // Check if tile above is occupied by a tile in the same shape
														tileAboveIsFree = !(slotTry.shape.rows[0][y - 1] & (1 << (7 - x)));
												}

												// Set custom heuristics for each special
												if (special == SpecialBrick_Spawner){
													specialHeuristicStrength = .5f;
													heuristicSpecialPlacement = .6f*Square(SafeDivide1((f32)numFullAdjacent, (f32)(numFreeAdjacent + numFullAdjacent))) + .4f*(tileBelowIsFree ? 0 : 1.f); 
												}else if (special == SpecialBrick_Powerup){
													specialHeuristicStrength = .3f;
													heuristicSpecialPlacement = .7f*Square(SafeDivide1((f32)numFullAdjacent, (f32)(numFreeAdjacent + numFullAdjacent))) + .3f*(tileBelowIsFree ? 0 : 1.f); 
												}else if (special == SpecialBrick_BadPowerup){
													specialHeuristicStrength = .3f;
													f32 howCenteredItIs = 1.f - ((f32)(shapePos.x + x) - (f32)gs->gridDim.x/2.f)/(f32)(gs->gridDim.x/2.f);
													b32 howCoveredItIs = SafeDivide1((f32)numFreeAdjacent, (f32)(numFreeAdjacent + numFullAdjacent));
													heuristicSpecialPlacement = .5f*howCoveredItIs + .3f*howCenteredItIs + .2f*(tileBelowIsFree ? 1.f : 0); 
												}else if (special == SpecialBrick_Arrow){
													specialHeuristicStrength = .4f;
													heuristicSpecialPlacement = (tileAboveIsFree ? 0 : .66f) + (tileBelowIsFree ? .33f : 0);
												}
											}
										}
									}
								}
								f32 fractionAdjacent = SafeDivide1((f32)numFullAdjacentTiles, (f32)(numFreeAdjacentTiles + numFullAdjacentTiles));
								f32 heuristicAdjacent = Square(fractionAdjacent);

								f32 heuristicYPos = Square(MapRangeTo01((f32)shapePos.y, (f32)shapePosMax.y, (f32)shapePosMin.y));
								f32 heuristicTime = Square(gs->spawnShapeTimer/gs->spawnShapeTime);

								// Bring all different heuristics together with different weights
								f32 heuristic = heuristicAdjacent*.4f + heuristicYPos*.3f + heuristicTime*.1f + emergencyHeuristic*.8f;
								heuristic = Lerp(heuristic, heuristicSpecialPlacement, specialHeuristicStrength);

								if (heuristic > bestHeuristic){
									bestHeuristic = heuristic;
									bestShapePos = shapePos;
									slotBest = slotTry;
								}
							}
						}
						if (RandomChance(Square(bestHeuristic))){
							slot->occupied = false;
							// Place it down
							for(s32 y = 0; y < slotBest.shapeDim.y; y++){
								for(s32 x = 0; x < slotBest.shapeDim.x; x++){
									// @CopyPaste
									if (slotBest.shape.rows[0][y] & (1 << (7 - x))){
										v2s tilePos = bestShapePos + V2S(x, y);
										tile_state *dest = &gs->tiles[tilePos.y*gs->gridDim.x + tilePos.x];
										ZeroStruct(dest);
										dest->occupied = true;
										dest->color = slotBest.shape.color;
										if (slotBest.shape.rows[1][y] & (1 << (7 - x))){
											dest->specialType = slotBest.shape.specialType;
											if (dest->specialType != SpecialBrick_BadPowerup)
												dest->specialAlpha = 1.f;
										}
									}
								}
							}
							PlaySound(gs->sndPlace);
						}
					}
				}
			}else{
				auto slot = &gs->availableSlots[gs->draggingShapeIndex];
				// Rotate
				f32 mouseWheel = GetMouseWheelMove();
				if (mouseWheel){
					RotateShape90Degrees(slot, (mouseWheel < 0));
				}
				// Find dragging shape tile pos.
				b32 mouseOnView = PointInRectangle(gs->mousePos, gs->viewPos, gs->viewPos + gs->viewDim); 
				if (mouseOnView){
					v2 temp = Hadamard(gs->mousePos - gs->viewPos + gs->tileDim/2 - Hadamard(V2(slot->shapeDim), gs->tileDim)/2, V2(1.f/gs->tileDim.x, 1.f/gs->tileDim.y));
					v2s tilePos0 = {(s32)temp.x, (s32)temp.y};
					v2s tilePos1 = tilePos0 + slot->shapeDim - V2S(1);
					// Version 1: you can place anywhere that fits.
					if (tilePos0.x >= 0 && tilePos0.y >= 0 && tilePos1.x < gs->gridDim.x && tilePos1.y < gs->gridDim.y){
						draggingShapeTilePos = tilePos0;
						isDraggingShapeOnWorld = true;
						// Determine if it collides other tiles
						isDraggingShapePosValid = true;
						for(s32 y = 0; y < slot->shapeDim.y; y++){
							for(s32 x = 0; x < slot->shapeDim.x; x++){
								if (slot->shape.rows[0][y] & (1 << (7 - x))){
									v2s tilePos = tilePos0 + V2S(x, y);
									if (gs->tiles[tilePos.y*gs->gridDim.x + tilePos.x].occupied){
										isDraggingShapePosValid = false;
									}
								}
							}
						}
					}

					// Version 2: Blocks are placed bottom up (you only choose the X)
					//tilePos1.y = gs->gridDim.y - 1;
					//tilePos0.y = tilePos1.y - slot->shapeDim.y + 1;
					//if (tilePos0.x >= 0 && tilePos0.y >= 0 && tilePos1.x < gs->gridDim.x && tilePos1.y < gs->gridDim.y){
					//	while(tilePos0.y >= 0){
					//		b32 valid = true;
					//		for(s32 y = 0; y < slot->shapeDim.y; y++){
					//			for(s32 x = 0; x < slot->shapeDim.x; x++){
					//				if (slot->shape.rows[0][y] & (1 << (7 - x))){
					//					v2s tilePos = tilePos0 + V2S(x, y);
					//					if (gs->tiles[tilePos.y*gs->gridDim.x + tilePos.x].occupied){
					//						valid = false;
					//					}
					//				}
					//			}
					//		}
					//		if (valid){
					//			isDraggingShapeOnWorld = true;
					//			isDraggingShapePosValid = true;
					//			draggingShapeTilePos = tilePos0;
					//		}else{
					//			break;
					//		}
					//		tilePos0.y -= 1;
					//		tilePos1.y -= 1;
					//	}
					//}
				}
				if (!IsMouseButtonDown(0)){
					gs->draggingShapeIndex = -1;

					if (isDraggingShapePosValid){
						slot->occupied = false;
						isDraggingShapeOnWorld = isDraggingShapePosValid = false;
						// Place it down
						for(s32 y = 0; y < slot->shapeDim.y; y++){
							for(s32 x = 0; x < slot->shapeDim.x; x++){
								if (slot->shape.rows[0][y] & (1 << (7 - x))){
									v2s tilePos = draggingShapeTilePos + V2S(x, y);
									tile_state *dest = &gs->tiles[tilePos.y*gs->gridDim.x + tilePos.x];
									ZeroStruct(dest);
									dest->occupied = true;
									dest->color = slot->shape.color;
									if (slot->shape.rows[1][y] & (1 << (7 - x))){
										dest->specialType = slot->shape.specialType;
										if (dest->specialType != SpecialBrick_BadPowerup)
											dest->specialAlpha = 1.f;
									}
								}
							}
						}
						PlaySound(gs->sndPlace);
					}else if (mouseOnView){
						PlaySound(gs->sndCantPlace);
					}
				}
			}

			// Update available slots
			gs->spawnShapeTimer = Min(gs->spawnShapeTime, gs->spawnShapeTimer + dt*gs->gameSpeed);
			if (gs->spawnShapeTimer == gs->spawnShapeTime){
				brick_shape_slot *firstFreeSlot = 0;
				for(s32 i = 0; i < ArrayCount(gs->availableSlots); i++){
					if (!gs->availableSlots[i].occupied){
						firstFreeSlot = &gs->availableSlots[i];
						break;
					}
				}
				if (firstFreeSlot){
					gs->spawnShapeTimer = 0;
					*firstFreeSlot = gs->nextSlots[0];
					for(s32 i = 0; i < ArrayCount(gs->nextSlots) - 1; i++){
						gs->nextSlots[i] = gs->nextSlots[i + 1];
					}
					FillShapeSlot(&gs->nextSlots[ArrayCount(gs->nextSlots) - 1]);
				}
			}

			// Paddle Movement
			f32 maxSpeed = (gs->powerupCountdownSlipperyControls ? 10.f : 7.f); // in pix/frame
			b32 keyRight = IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D);
			b32 keyLeft = IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A);
			b32 keyRightPressed = IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D);
			b32 keyLeftPressed = IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A);
			if (gs->powerupCountdownReverseControls){
				SWAP(keyRight, keyLeft);
				SWAP(keyRightPressed, keyLeftPressed);
			}

			if (!gs->paddleLastInputDir)
				gs->paddleLastInputDir = 1;
		
			if (!keyRight && !keyLeft){
				// Decel
				if (gs->powerupCountdownSlipperyControls){
					gs->paddleXSpeed = Lerp(MoveTowards(gs->paddleXSpeed, 0, .01f*dtMul*gs->gameSpeed), 0, .02f*dtMul*gs->gameSpeed);
				}else{
					gs->paddleXSpeed = Lerp(MoveTowards(gs->paddleXSpeed, 0, 1.f*dtMul*gs->gameSpeed), 0, .3f*dtMul*gs->gameSpeed);
				}
			}else{
				if (keyRight && keyLeft){
					if (keyRightPressed){
						gs->paddleLastInputDir = 1;
					}else if (keyLeftPressed){
						gs->paddleLastInputDir = -1;
					}
				}else if (keyRight){
					gs->paddleLastInputDir = 1;
				}else{
					gs->paddleLastInputDir = -1;
				}
				f32 target = maxSpeed*gs->paddleLastInputDir;
				if (gs->powerupCountdownSlipperyControls){
					f32 spd = LerpClamp(.025f, .5f, gs->paddleXSpeed/target);
					gs->paddleXSpeed = Lerp(MoveTowards(gs->paddleXSpeed, target, spd*dtMul*gs->gameSpeed), target, .03f*dtMul*gs->gameSpeed);
				}else{
					gs->paddleXSpeed = Lerp(MoveTowards(gs->paddleXSpeed, target, .05f*dtMul*gs->gameSpeed), target, .1f*dtMul*gs->gameSpeed);
				}
			}

			f32 paddleXMin = gs->paddleDim.x/2;
			f32 paddleXMax = gs->viewDim.x - gs->paddleDim.x/2;
			gs->paddlePos.x = Clamp(gs->paddlePos.x + gs->paddleXSpeed*dtMul*gs->gameSpeed, paddleXMin, paddleXMax);
			if (gs->paddlePos.x == paddleXMin){
				if (gs->paddleXSpeed < 0){
					if (gs->powerupCountdownSlipperyControls){
						gs->paddleXSpeed *= -.5f;
					}else{
						gs->paddleXSpeed = 0;
					}
				}
			}else if (gs->paddlePos.x == paddleXMax){
				if (gs->paddleXSpeed > 0){
					if (gs->powerupCountdownSlipperyControls){
						gs->paddleXSpeed *= -.5f;
					}else{
						gs->paddleXSpeed = 0;
					}
				}
			}

			// Set position of balls on the paddle and shoot them out.
			for(s32 i = 0; i < gs->numBalls; i++){
				auto b = &gs->balls[i];
				if (b->flags & BallFlags_OnPaddle){
					b->pos = gs->paddlePos + V2(b->positionOnPaddle*gs->paddleDim.x/2, -gs->paddleDim.y/2 - b->r);
					b->pos.x = Clamp(b->pos.x, b->r, gs->viewDim.x - b->r);

					if (IsKeyDown(KEY_SPACE)){
						if (CheckFlag(b->flags, BallFlags_StuckShootRandomly)){
							f32 minAngle = (.5f*PI/2.f);
							b->speed = V2LengthDir(BallSpeed(), Lerp(PI + minAngle, 2*PI - minAngle, Random01()));
						}else{
							b->speed = BallPaddleBounceDir(b)*BallSpeed();
						}
						UnsetFlags(b->flags, BallFlags_OnPaddle | BallFlags_StuckShootRandomly);
						PlaySound(gs->sndPaddle);
					}else{
						b->speed = V2(0);
					}
				}
			}
	
			// Update Drops
			for(s32 i = 0; i < gs->numDrops;){
				gs->drops[i].pos.y += gs->drops[i].ySpeed*dtMul*gs->gameSpeed;

				b32 remove = (gs->drops[i].pos.y - DROP_RADIUS > gs->winDim.y);
				if (CircleInRectangle(gs->drops[i].pos, DROP_RADIUS, gs->paddlePos - gs->paddleDim/2, gs->paddleDim)){
					remove = true;
					switch(gs->drops[i].type){
					case Drop_Life: { gs->paddleLifes++; } break;
					case Drop_ExtraBall:
					case Drop_TwoExtraBalls: 
					{
						s32 num = (gs->drops[i].type == Drop_TwoExtraBalls ? 2 : 1);
						for(s32 index = 0; index < num; index++){
							if (gs->numBalls < ArrayCount(gs->balls)){
								auto newBall = &gs->balls[gs->numBalls];
								ZeroStruct(newBall);
								newBall->r = BallRadius();
								newBall->pos = gs->paddlePos + V2(0, -gs->paddleDim.y - newBall->r);
								f32 originalBallAngle = Random(2*PI);
								for(s32 i = 0; i < gs->numBalls; i++){
									if (!CheckFlag(gs->balls[i].flags, BallFlags_OnPaddle)){
										newBall->pos = gs->balls[i].pos;
										originalBallAngle = AngleOf(gs->balls[i].speed);
										break;
									}
								}
								// Random angle
								// Basically we get a random angle except we don't allow it to be too close to the original ball's, because then it's confusing.
								// We also don't allow it to be too horizontal.
								f32 minAngle = MIN_PADDLE_BOUNCE_ANGLE*.5f;
								f32 minAngleDifference = PI*.1f;
								f32 angle = NormalizeAngle(originalBallAngle + RandomRange(minAngleDifference, PI - minAngleDifference) + (RandomChance(.5f) ? PI : 0));
								if (angle > 0){
									angle = ClampAngle(angle, minAngle, PI - minAngle);
								}else{
									angle = ClampAngle(angle, PI + minAngle, 2*PI - minAngle);
								}
								newBall->speed = V2LengthDir(BallSpeed(), angle);
								if (newBall->pos.y > gs->paddlePos.y - 130) // If it's too low we force it to go upwards to be fair to the player.
									newBall->speed.y = -Abs(newBall->speed.y);
								gs->numBalls++;
							}
						}
					} break;
					case Drop_BigPaddle:        { gs->powerupCountdownBigPaddle        = POWERUP_TIME_BIG_PADDLE; } break;
					case Drop_Magnet:           { gs->powerupCountdownMagnet           = POWERUP_TIME_MAGNET; } break;
					case Drop_BigBalls:         { gs->powerupCountdownBigBalls         = POWERUP_TIME_BIG_BALLS; } break;
					case Drop_Barrier:          { gs->powerupCountdownBarrier          = POWERUP_TIME_BARRIER; } break;
					case Drop_FastBalls:        { gs->powerupCountdownFastBalls        = POWERUP_TIME_FAST_BALLS; } break;
					case Drop_SlowBalls:        { gs->powerupCountdownSlowBalls        = POWERUP_TIME_SLOW_BALLS; } break;
					case Drop_SmallPaddle:      { gs->powerupCountdownSmallPaddle      = POWERUP_TIME_SMALL_PADDLE; } break;
					case Drop_ReverseControls:  { gs->powerupCountdownReverseControls  = POWERUP_TIME_REVERSE_CONTROLS; } break;
					case Drop_SlipperyControls: { gs->powerupCountdownSlipperyControls = POWERUP_TIME_SLIPPERY_CONTROLS; } break;
					case Drop_Randomizer:       { gs->powerupCountdownRandomizer       = POWERUP_TIME_RANDOMIZER; } break;
					}
					if (gs->drops[i].type >= FIRST_GOOD_DROP && gs->drops[i].type <= LAST_GOOD_DROP){
						// TODO Powerup Sound
					}else{
						// TODO Powerdown Sound
					}
				}
				if (remove){
					gs->numDrops--;
					if (i < gs->numDrops){
						gs->drops[i] = gs->drops[gs->numDrops]; // Swap by last
					}
				}else{
					i++;
				}
			}

			// Update Balls
			for(s32 i = 0; i < gs->numBalls;){
				auto b = &gs->balls[i];
				v2 prevPos = b->pos;
				b->pos += b->speed*dtMul*gs->gameSpeed;
				b->r = BallRadius();
				if (b->speed != V2(0)){
					b->speed = Normalize(b->speed)*BallSpeed();
				}
				b32 playBallHitSound = false;
				// Bounce off walls
				v2 minPos = V2(b->r);
				v2 maxPos = gs->viewDim - V2(b->r);
				if (b->pos.x < minPos.x){
					b->pos.x = minPos.x;
					b->speed.x *= -1;
					playBallHitSound = true;
				}else if (b->pos.x > maxPos.x){
					b->pos.x = maxPos.x;
					b->speed.x *= -1;
					playBallHitSound = true;
				}
				if (b->pos.y < 0){
					gs->gameEnded = true;
					gs->paddleWon = true;
					PlaySound(gs->sndWinPaddle);
				}
				// Barrier
				if (gs->powerupCountdownBarrier && CircleInRectangle(b->pos, b->r, V2(0, gs->barrierTopY), V2(gs->winDim.x, gs->barrierHeight))){
					b->speed.y = -Abs(b->speed.y);
					playBallHitSound = true;
				}
				// Randomizer
				if (gs->powerupCountdownRandomizer && CircleInRectangle(b->pos, b->r, V2(0, gs->randomizerY - 10.f), V2(gs->winDim.x, 20.f))){
					if (!CheckFlag(b->flags, BallFlags_InRandomizer)){
						SetFlag(b->flags, BallFlags_InRandomizer);
						if (b->speed != V2(0)){
							f32 angleIncrement = RandomRange(PI*.1f, PI*.18f)*(RandomS32(1) ? -1.f : 1.f);
							f32 angle = NormalizeAngle(AngleOf(b->speed) + angleIncrement);
							f32 minAngle = MIN_PADDLE_BOUNCE_ANGLE*.5f;
							if (angle > 0){
								angle = ClampAngle(angle, minAngle, PI - minAngle);
							}else{
								angle = ClampAngle(angle, PI + minAngle, 2*PI - minAngle);
							}

							b->speed = V2LengthDir(Length(b->speed), angle);

							if (RandomChance(.4f))
								b->speed.y *= -1;

							PlaySound(gs->sndWoot);
						}
					}
				}else{
					UnsetFlag(b->flags, BallFlags_InRandomizer);
				}
				b32 incrementI = true;
				if (b->pos.y - b->r > gs->winDim.y){ // Ball was lost downscreen
					gs->numBalls--;
					if (gs->numBalls == 0){
						PlaySound(gs->sndHurt);
						if (gs->paddleLifes){
							gs->paddleLifes--;
							gs->numBalls++;
							// Spawn new ball
							ZeroStruct(b);
							b->r = DEFAULT_BALL_RADIUS;
							SetFlags(b->flags, BallFlags_OnPaddle | BallFlags_StuckShootRandomly);
						}else{
							gs->gameEnded = true;
							gs->paddleWon = false;
							PlaySound(gs->sndWinBricks);
						}
					}else{
						*b = gs->balls[gs->numBalls]; // Fill the hole in the array with the last element
					}
				}else{ // (Ball wasn't removed)
					// Collide paddle
					if (b->speed.y > 0 && CircleInRectangle(b->pos, b->r, gs->paddlePos - gs->paddleDim/2, gs->paddleDim)){
						if (gs->powerupCountdownMagnet){
							// Stick to paddle.
							SetFlag(b->flags, BallFlags_OnPaddle);
							b->positionOnPaddle = Clamp((b->pos.x - gs->paddlePos.x)/(gs->paddleDim.x/2), -1.f, 1.f);
							b->pos = gs->paddlePos + V2(b->positionOnPaddle*gs->paddleDim.x/2, -gs->paddleDim.y/2 - b->r);
							b->pos.x = Clamp(b->pos.x, b->r, gs->viewDim.x - b->r);
							b->speed = V2(0);
						}else{
							// Bounce normally
							v2 paddleCornerTopLeft = gs->paddlePos + V2(-gs->paddleDim.x/2, -gs->paddleDim.y/2);
							v2 paddleCornerTopRight = gs->paddlePos + V2(gs->paddleDim.x/2, -gs->paddleDim.y/2);
							v2 speedN = RotateMinus90Degrees(Normalize(b->speed));
							f32 leftProj = Dot(speedN, paddleCornerTopLeft - b->pos);
							f32 rightProj = Dot(speedN, paddleCornerTopRight - b->pos);
							if (leftProj - b->r < 0 && rightProj + b->r > 0){ // Ball speed line intersects paddle's top edge.
								v2 newPos = b->pos;
								newPos.y = gs->paddlePos.y - gs->paddleDim.y/2 - b->r;
								if (b->speed.y){
									newPos.x = b->pos.x + (newPos.y - b->pos.y)*b->speed.x/b->speed.y;
								}
								b->pos = b->pos + LimitLengthV2(newPos - b->pos, Length(b->speed*dtMul*gs->gameSpeed));
								b->pos.x = Clamp(b->pos.x, minPos.x, maxPos.x);

								b->speed = BallPaddleBounceDir(b)*BallSpeed();
							}
							PlaySound(gs->sndPaddle);
							PlaySound(gs->sndPaddleHitsBall);
						}
					}

					// Collide tiles
					v2s tileMin = {(s32)Clamp((b->pos.x - b->r)/gs->tileDim.x, 0, gs->gridDim.x - 1),
								   (s32)Clamp((b->pos.y - b->r)/gs->tileDim.y, 0, gs->gridDim.y - 1)};
					v2s tileMax = {(s32)Clamp((b->pos.x + b->r)/gs->tileDim.x, 0, gs->gridDim.x - 1),
								   (s32)Clamp((b->pos.y + b->r)/gs->tileDim.y, 0, gs->gridDim.y - 1)};

					// Get the ball very close to the edge
					f32 numerator = 1;
					f32 denominator = 1;
					v2 p = prevPos;
					v2s collidedTiles[6];
					s32 numCollidedTiles = 0;
					Assert((tileMax.x - tileMin.x)*(tileMax.y - tileMin.y) <= ArrayCount(collidedTiles));
					f32 m = 0;//3.f;
					for(s32 j = 0; j < 20; j++){
						f32 t = numerator/denominator;
						v2 checkPos = LerpV2(prevPos, b->pos, t);
						numerator *= 2;
						denominator *= 2;
						b32 collided = false;
						for(s32 y = tileMin.y; y <= tileMax.y; y++){
							for(s32 x = tileMin.x; x <= tileMax.x; x++){
								if (!gs->tiles[y*gs->gridDim.x + x].occupied)
									continue;
								v2 tilePos = {x*gs->tileDim.x, y*gs->tileDim.y};
								if (CircleInRectangle(checkPos, b->r, tilePos + V2(m), gs->tileDim - V2(2*m))){
									if (!collided){
										numCollidedTiles = 0; // Clear collision data from previous iterations
										collided = true;
									}
									collidedTiles[numCollidedTiles] = V2S(x, y);
									numCollidedTiles++;
								}
							}
						}
						if (collided){
							numerator -= 1;
						}else{
							p = checkPos;
							if (numerator == denominator)
								break; // No collisions at first iteration.
							numerator += 1;
						}
					}
					b->pos = p;
					if (numCollidedTiles){
						// Bounce

						// Find closest edge to bounce against.
						v2 n = Normalize(-b->speed);
						f32 bestDistance = MAX_F32;
						for(s32 j = 0; j < numCollidedTiles; j++){
							v2 tilePos = Hadamard(V2(collidedTiles[j]), gs->tileDim) + V2(m);
							v2 brickPos = tilePos + V2(m);
							v2 brickDim = gs->tileDim - V2(2*m);
							v2 dis = {0, 0};
							if (b->speed.x > 0 && p.x < brickPos.x){
								dis.x = p.x - brickPos.x;
							}else if (b->speed.x < 0 && p.x > brickPos.x + brickDim.x){
								dis.x = p.x - (brickPos.x + brickDim.x);
							}
							if (b->speed.y > 0 && p.y < brickPos.y){
								dis.y = p.y - brickPos.y;
							}else if (b->speed.y < 0 && p.y > brickPos.y + brickDim.y){
								dis.y = p.y - (brickPos.y + brickDim.y);
							}
							f32 length = Length(dis);
							if (length < bestDistance){
								bestDistance = length;

								if (Abs(dis.x) > Abs(dis.y)){
									n = V2(SignNonZero(dis.x), 0);
								}else{
									n = V2(0, SignNonZero(dis.y));
								}
							}

							b32 startedInside = CircleInRectangle(b->pos, b->r, tilePos + V2(m), gs->tileDim - V2(2*m));

							auto tile = &gs->tiles[collidedTiles[j].y*gs->gridDim.x + collidedTiles[j].x];
							if (tile->specialType == SpecialBrick_Arrow && n != V2(0, -1.f) && !startedInside){
								// Bounce arrow brick
								if (b->speed.y < 0 && n.x){
									b->speed.y *= -1.f;
								}
								PlaySound(gs->sndBounce);
							}else{ // Break brick normally
								// Drop
								if (tile->specialType != SpecialBrick_None && tile->specialAlpha > .5f){
									if (gs->numDrops < ArrayCount(gs->drops)){
										gs->drops[gs->numDrops].pos = tilePos + gs->tileDim/2;
										if (tile->specialType == SpecialBrick_Spawner){
											drop_type type;
											if (RandomChance(.5f)){
												type = (drop_type)RandomRangeS32((s32)FIRST_GOOD_DROP, (s32)LAST_GOOD_DROP);
											}else{
												type = (drop_type)RandomRangeS32((s32)FIRST_BAD_DROP, (s32)LAST_BAD_DROP);
											}
											PlaySound(gs->sndPreerw);

											CreateDrop(tilePos + gs->tileDim/2, type);
										}else if (tile->specialType == SpecialBrick_Powerup){
											CreateDrop(tilePos + gs->tileDim/2, (drop_type)RandomRangeS32((s32)FIRST_GOOD_DROP, (s32)LAST_GOOD_DROP));
										}else if (tile->specialType == SpecialBrick_BadPowerup){
											CreateDrop(tilePos + gs->tileDim/2, (drop_type)RandomRangeS32((s32)FIRST_BAD_DROP, (s32)LAST_BAD_DROP));
										}
									}
								}
								s32 soundIndex = 1;
								if (gs->sameColorComboMax){ // Combo is enabled
									// Combo
									if (tile->color == gs->sameColorComboLastColor){
										gs->sameColorCombo++;
									}else{
										gs->sameColorComboLastColor = tile->color;
										gs->sameColorCombo = 1;
									}
									// Sound
									if (gs->sameColorComboMax == ArrayCount(gs->sndCombo)){
										soundIndex = (gs->sameColorCombo - 1) % ArrayCount(gs->sndCombo);
									}else{
										soundIndex = 1 + ((gs->sameColorCombo - 1) % gs->sameColorComboMax);
										soundIndex = ClampS32(soundIndex, 0, ArrayCount(gs->sndCombo) - 1);
									}
										
									if (gs->sameColorCombo % gs->sameColorComboMax == 0){
										// Finished combo: drop powerup.
										CreateDrop(tilePos + gs->tileDim/2, (drop_type)RandomRangeS32((s32)FIRST_GOOD_DROP, (s32)LAST_GOOD_DROP));
										soundIndex = ArrayCount(gs->sndCombo) - 1; // Chord sound
									}
								}
								PlaySound(gs->sndCombo[soundIndex]);
								ZeroStruct(tile); // (tile->occupied = false;)
							}
						}

						b->speed = b->speed - 2*Dot(b->speed, n)*n; // Bounce
					}
				}
				if (playBallHitSound){
					if (RandomS32(1)){
						PlaySound((gs->powerupCountdownBigBalls ? gs->sndHeavyBallHit1 : gs->sndBallHit1));
					}else{
						PlaySound((gs->powerupCountdownBigBalls ? gs->sndHeavyBallHit2 : gs->sndBallHit2));
					}
				}

				if (incrementI)
					i++;
			}
		}

		//
		// Draw
		//
		
		v4 guiBackgroundColor = V4(.6f, .3f, .4f);
		v4 backgroundColor = V4(.1f, .02f, .12f);
		// View background
		DrawRectangleV(Vector2_(gs->viewPos.x, 0), Vector2_(gs->viewDim.x, gs->winDim.y), Color_(backgroundColor)); 

		// Draw drops
		for(s32 i = 0; i < gs->numDrops; i++){
			v2 texDim = V2(32.f);
			v2 texPos = V2(((s32)gs->drops[i].type)*texDim.x, 48.f);
			if (gs->drops[i].type >= FIRST_BAD_DROP){
				texPos = V2(((s32)gs->drops[i].type - (s32)FIRST_BAD_DROP)*texDim.x, 80.f);
			}
			f32 scale = 1.f;
			DrawSprite(texPos, texDim, gs->viewPos + gs->drops[i].pos - texDim*scale/2, V2(scale));
		}

		// Draw (and update) bricks
		for(s32 y = 0; y < gs->gridDim.y; y++){
			for(s32 x = 0; x < gs->gridDim.x; x++){
				tile_state *tile = &gs->tiles[y*gs->gridDim.x + x];
				if (tile->occupied){
					f32 m = TILE_DRAW_MARGIN;
					v2 p = gs->viewPos + V2(x*gs->tileDim.x, y*gs->tileDim.y);
					DrawRectangleV(Vector2_(p + V2(m)), Vector2_(gs->tileDim - V2(m*2)), Color_(tile->color));
					if (tile->specialType != SpecialBrick_None){
						if (tile->specialType == SpecialBrick_Spawner){
							f32 spawnRate = 5.f;
							if ((s32)(gs->gameTime/spawnRate) != (s32)(gameTimePrev/spawnRate)){
								// Spawn
								s32 emptyCount = 0;
								s32 r = 2;
								v2s minTile = MaxV2S(V2S(0), V2S(x - r, y - r));
								v2s maxTile = MinV2S(gs->gridDim - V2S(1), V2S(x + r, y + r));
								for(s32 ty = minTile.y; ty <= maxTile.y; ty++){
									for(s32 tx = minTile.x; tx <= maxTile.x; tx++){
										if (!gs->tiles[ty*gs->gridDim.x + tx].occupied){
											emptyCount++;
										}
									}
								}
								if (emptyCount){
									s32 chosenTile = RandomS32(emptyCount - 1);
									for(s32 ty = minTile.y; ty <= maxTile.y; ty++){
										for(s32 tx = minTile.x; tx <= maxTile.x; tx++){
											tile_state *emptyTile = &gs->tiles[ty*gs->gridDim.x + tx];
											if (!emptyTile->occupied){
												if (chosenTile == 0){
													ZeroStruct(emptyTile);
													emptyTile->occupied = true;
													emptyTile->color = tile->color;
												}
												chosenTile--;
											}
										}
									}
								}
							}
							DrawBrickSpecial(p + V2(m), gs->tileDim - V2(m*2), tile->specialType, tile->color, tile->specialAlpha);
						}else{ // Momentary Special Bricks
							if (!freeze){
								tile->specialTypeTimer += dt;
							}
							tile->specialAlpha = Min(1.f, tile->specialAlpha + 1.3f*dt*gs->gameSpeed);
							f32 destroyTime = 60.f;
							f32 fadeoutTime = 15.f;
							f32 alpha = MapRangeToRangeClamp(tile->specialTypeTimer, destroyTime, destroyTime - fadeoutTime, .15f, 1.f);
							if (tile->specialTypeTimer >= destroyTime){
								tile->specialType = SpecialBrick_None;
								tile->specialTypeTimer = 0;
							}else{
								DrawBrickSpecial(p + V2(m), gs->tileDim - V2(m*2), tile->specialType, tile->color, tile->specialAlpha*alpha);
							}
						}
					}
				}
			}
		}
		// Draw Paddle
		{
			v4 paddleColor = V4_White();
			if (gs->powerupCountdownReverseControls){
				paddleColor = V4(.98f, .9f, .12f);
			}else if (gs->powerupCountdownSlipperyControls){
				paddleColor = V4(.25f, 1.f, 1.f);
			}
			//DrawRectangleV(Vector2_(gs->viewPos + gs->paddlePos - gs->paddleDim/2), Vector2_(gs->paddleDim), Color_(paddleColor)); // Draws rectangular paddle instead of rounded
			DrawRectangleV(Vector2_(gs->viewPos + gs->paddlePos - gs->paddleDim/2 + V2(gs->paddleDim.y/2, 0)), Vector2_(gs->paddleDim - V2(gs->paddleDim.y, 0)), Color_(paddleColor));
			DrawCircleV(Vector2_(gs->viewPos + gs->paddlePos + V2(-gs->paddleDim.x/2 + gs->paddleDim.y/2, 0)), gs->paddleDim.y/2, Color_(paddleColor));
			DrawCircleV(Vector2_(gs->viewPos + gs->paddlePos + V2( gs->paddleDim.x/2 - gs->paddleDim.y/2, 0)), gs->paddleDim.y/2, Color_(paddleColor));

			if (gs->powerupCountdownMagnet){
				for(s32 i = 0; i < gs->numBalls; i++){
					if (CheckFlag(gs->balls[i].flags, BallFlags_OnPaddle) && !CheckFlag(gs->balls[i].flags, BallFlags_StuckShootRandomly)){
						v2 rd = BallPaddleBounceDir(&gs->balls[i]); // Ray dir
						v2 ro = gs->balls[i].pos + rd*gs->balls[i].r; // Ray origin
						if (!rd.y)
							break;
						// Collide walls
						if (rd.x){
							// Equation of left wall: x=0
							// Equation of ray:  pos = ro + rd*t
							// 0 = ro.x + rd.x*t
							// t = -ro.x/rd.x
							f32 leftT = -ro.x/rd.x;
							v2 leftP = ro + rd*leftT;
							// Similar thing for right wall...
							f32 rightT = (gs->viewDim.x - ro.x)/rd.x;
							v2 rightP = ro + rd*rightT;

							f32 maxT = gs->balls[i].pos.y - gs->tileDim.y*gs->gridDim.y;
							f32 t = maxT;
							if (leftT > 0){
								t = Min(t, leftT);
							}else if (rightT > 0){
								t = Min(t, rightT);
							}
							s32 numSegments = (s32)Max(1.f, t/5.f);
							for(s32 j = 0; j < numSegments; j++){
								f32 t0 = j*t/(f32)numSegments;
								f32 t1 = (j + 1)*t/(f32)numSegments;
								f32 alpha = Square(1.f - (j/(f32)numSegments)*(t/maxT))*.8f;
								DrawLineEx(Vector2_(gs->viewPos + ro + rd*t0), Vector2_(gs->viewPos + ro + rd*t1), 3.f, Color_(V4(1.f, .3f, .5f, alpha)));
							}

						}
					}
				}
			}
		}
		
		// Draw Barrier
		if (gs->powerupCountdownBarrier){
			v2 barrierPos = gs->viewPos + V2(0, gs->barrierTopY);
			DrawRectangleV(Vector2_(barrierPos), Vector2_(gs->viewDim.x, gs->barrierHeight), WHITE);

			f32 d = gs->barrierHeight; // diagonal width and height (becauseangle is 45 deg)
			s32 num = (s32)(gs->viewDim.x/(d*4) + .5f);
			f32 lineWidth = gs->viewDim.x/(num*2);
			v4 color = V4(1.f, .4f, .96f);
			for(s32 i = 0; i < num; i++){
				v2 p = barrierPos + V2(lineWidth*2*i, 0);
				Vector2 vertices[4] = { Vector2_(p + V2(d, 0)),
										Vector2_(p + V2(0, d)),
										Vector2_(p + V2(lineWidth, d)),
										Vector2_(p + V2(d + lineWidth, 0)) };
				DrawTriangleFan(vertices, 4, Color_(color));
			}
		}
		
		// Draw Randomizer
		if (gs->powerupCountdownRandomizer){
			s32 num = (s32)Round(gs->viewDim.x/50.f);
			for(s32 i = 0; i < num; i++){
				f32 sep = gs->viewDim.x/(2.f*num);
				v2 pos = gs->viewPos + V2((1.f + 2.f*i)*sep, gs->randomizerY);
				v2 dim = {(f32)MeasureText("?", 20), 20.f};
				DrawText("?", (s32)(pos.x - dim.x/2), (s32)(pos.y - dim.y/2), 20, Color_(V4(1.f,  .4f, .97f)));
			}
		}

		// Draw Balls
		for(s32 i = 0; i < gs->numBalls; i++){
			DrawCircleV(Vector2_(gs->viewPos + gs->balls[i].pos), gs->balls[i].r, Color_(V4_Grey((gs->powerupCountdownMagnet ? .55f : 1.f))));
		}


		//
		// GUI
		//
		
		// "Speed up" message
		char speedStr[50];
		sprintf(speedStr, (Abs(Frac(gs->gameSpeed + .5f) - .5f) < .001f ? "%.0fx" : "%.01fx"), gs->gameSpeed);
			
		if (gs->speedUpMessageTimer){
			f32 t = Clamp01(gs->speedUpMessageTimer/gs->speedUpMessageTime);
			f32 h = 100.f;

			f32 a = Min(1.f, gs->speedUpMessageTimer/.3f); // Fade in
			f32 fadeout = Clamp01((gs->speedUpMessageTime - gs->speedUpMessageTimer)/.33f); // Fade out
			a = Min(a, fadeout);
			f32 alpha = a*(.4f + Map01ToBellSin(t)*.3f);

			DrawRectangleV(Vector2_(gs->viewPos.x, gs->winDim.y/2 - h/2), Vector2_(gs->viewDim.x, h), Fade(WHITE, alpha));
			char *str = "Speed up!";
			f32 fontSize = 40.f;
			f32 xOff = Lerp(-60.f, 50.f, t*.1f + Map01ToArcSin(t)*.9f) + (t*t*t)*100.f + (1.f - fadeout)*30.f;
			f32 textWidth = MeasureText(str, fontSize);
			v2 textPos = V2(gs->viewPos.x + gs->viewDim.x/2 - textWidth/2 + xOff, gs->winDim.y/2 - fontSize/2 - 20.f);
			// I think the "Ex" version gives better anti-aliasing/stuttering results.
			DrawTextEx(GetFontDefault(), str, Vector2_(textPos), fontSize, 2.f, Fade(BLACK, Min(1.f, alpha + .3f)));
			//DrawText(str, textPos.x, textPos.y, fontSize, Fade(BLACK, Min(1.f, alpha + .3f)));

			// Secondary text
			fontSize = 20;
			xOff = xOff*.7f;//Lerp(-60.f, 60.f, t*.1f + Map01ToArcSin(t)*.9f);
			v2 text2Pos = V2(gs->viewPos.x + gs->viewDim.x/2 - MeasureText(speedStr, 20)/2 + xOff, gs->winDim.y/2 + 15.f);
			DrawTextEx(GetFontDefault(), speedStr, Vector2_(text2Pos), fontSize, 2.f, Color_(V4_Grey(.15f, Min(1.f, alpha + .0f))));
		}

		// GUI background (side bars)
		DrawRectangleV(Vector2_(0), Vector2_(gs->viewPos.x, gs->winDim.y), Color_(guiBackgroundColor));
		DrawRectangleV(Vector2_(gs->viewPos.x + gs->viewDim.x, 0), Vector2_(gs->winDim.x - gs->viewPos.x - gs->viewDim.x, gs->winDim.y), Color_(guiBackgroundColor));
		
		// Draw game timer
		{
			char str[50];
			sprintf(str, "%02i:%02i", (s32)(gs->gameTime/60), ((s32)gs->gameTime) % 60);
			DrawText(str, gs->viewPos.x/2 - MeasureText(str, 20)/2 + 2.f, 20 + 2.f, 20, Fade(BLACK, .3f));
			DrawText(str, gs->viewPos.x/2 - MeasureText(str, 20)/2, 20, 20, WHITE);
		}
		// Draw game speed
		if (gs->doSpeedUp){
			f32 speedStrWidth = MeasureText(speedStr, 20);
			DrawText(speedStr, gs->viewPos.x/2 - speedStrWidth/2 + 2.f, 50 + 2.f, 20, Fade(BLACK, .3f));
			DrawText(speedStr, gs->viewPos.x/2 - speedStrWidth/2, 50, 20, WHITE);

		}

		{
			// Lifes left
			s32 lifesPerRow = 4;
			f32 r = 7.f;
			f32 sep = 2*r + 8.f;
			f32 ySep = 20.f;
			for(s32 i = 0; i < gs->paddleLifes; i++){
				v2 p = V2((gs->viewPos.x - sep*lifesPerRow)/2 + r + (i % lifesPerRow)*sep, gs->winDim.y - 35 - ySep*(i/lifesPerRow));
				DrawCircleV(Vector2_(p + V2(2.f)), r, Fade(BLACK, .3f));
				DrawCircleV(Vector2_(p), r, WHITE);
			}
			// Combo message
			if (gs->sameColorCombo > 1){
				char text[50];
				sprintf(text, "Combo x%i!", gs->sameColorCombo);
				s32 fontSize = 20;
				v2 pos = {(gs->viewPos.x - MeasureText(text, fontSize))/2, gs->winDim.y - 80 - ySep*MaxS32(0, (gs->paddleLifes - 1)/lifesPerRow)};
				DrawText(text, pos.x + 2, pos.y + 2, fontSize, Fade(BLACK, .3f));
				DrawText(text, pos.x, pos.y, fontSize, Color_(LerpV4(gs->sameColorComboLastColor, V4_White(), .1f)));
			}
		}

		// Draw powerups
		{ // Good powerups
			drop_type powerups[4] = {};
			f32 powerupTimes[ArrayCount(powerups)] = {};
			s32 numPowerups = 0;
			if (gs->powerupCountdownBigPaddle){
				powerups[numPowerups] = Drop_BigPaddle;
				powerupTimes[numPowerups++] = (gs->powerupCountdownBigPaddle/POWERUP_TIME_BIG_PADDLE);
			}
			if (gs->powerupCountdownMagnet){
				powerups[numPowerups] = Drop_Magnet;
				powerupTimes[numPowerups++] = (gs->powerupCountdownMagnet/POWERUP_TIME_MAGNET);
			}
			if (gs->powerupCountdownBigBalls){
				powerups[numPowerups] = Drop_BigBalls;
				powerupTimes[numPowerups++] = (gs->powerupCountdownBigBalls/POWERUP_TIME_BIG_BALLS);
			}
			if (gs->powerupCountdownBarrier){
				powerups[numPowerups] = Drop_Barrier;
				powerupTimes[numPowerups++] = (gs->powerupCountdownBarrier/POWERUP_TIME_BARRIER);
			}
			// Sort
			for(s32 i = 0; i < numPowerups; i++){
				for(s32 j = i; j < numPowerups - 1; j++){
					if (powerupTimes[j] < powerupTimes[j + 1]){
						SWAP(powerupTimes[j], powerupTimes[j + 1]);
						SWAP(powerups[j], powerups[j + 1]);
					}
				}
			}
			// Draw
			for(s32 i = 0; i < numPowerups; i++){
				v2 texDim = V2(32.f);
				v2 texPos = V2(((s32)powerups[i] - (s32)FIRST_GOOD_DROP)*texDim.x, 48.f);
				f32 scale = 1.f;
				v2 p = {20.f + (texDim.x*scale + 11.f)*i, 120};
				f32 r1 = texDim.y*scale/2 - 2.f;
				f32 r2 = texDim.y*scale/2 + 4.f;
				DrawRing(Vector2_(p + texDim*scale/2), r1, r2, 180 + 360*powerupTimes[i], 540, 48, Color_(V4(.1f, .1f, .1f, .95f)));
				DrawRing(Vector2_(p + texDim*scale/2), r1, r2, 180, 180 + 360*powerupTimes[i], 48, Color_(V4(1.f, 1.f, 1.f, .95f)));
				DrawSprite(texPos, texDim, p, V2(scale), V4_White());
			}
		}
		{ // Bad powerups
			drop_type powerups[7] = {};
			f32 powerupTimes[ArrayCount(powerups)] = {};
			s32 numPowerups = 0;
			if (gs->powerupCountdownFastBalls){
				powerups[numPowerups] = Drop_FastBalls;
				powerupTimes[numPowerups++] = (gs->powerupCountdownFastBalls/POWERUP_TIME_FAST_BALLS);
			}
			if (gs->powerupCountdownSlowBalls){
				powerups[numPowerups] = Drop_SlowBalls;
				powerupTimes[numPowerups++] = (gs->powerupCountdownSlowBalls/POWERUP_TIME_SLOW_BALLS);
			}
			if (gs->powerupCountdownSmallPaddle){
				powerups[numPowerups] = Drop_SmallPaddle;
				powerupTimes[numPowerups++] = (gs->powerupCountdownSmallPaddle/POWERUP_TIME_SMALL_PADDLE);
			}
			if (gs->powerupCountdownReverseControls){
				powerups[numPowerups] = Drop_ReverseControls;
				powerupTimes[numPowerups++] = (gs->powerupCountdownReverseControls/POWERUP_TIME_REVERSE_CONTROLS);
			}
			if (gs->powerupCountdownSlipperyControls){
				powerups[numPowerups] = Drop_SlipperyControls;
				powerupTimes[numPowerups++] = (gs->powerupCountdownSlipperyControls/POWERUP_TIME_SLIPPERY_CONTROLS);
			}
			if (gs->powerupCountdownRandomizer){
				powerups[numPowerups] = Drop_Randomizer;
				powerupTimes[numPowerups++] = (gs->powerupCountdownRandomizer/POWERUP_TIME_RANDOMIZER);
			}
			// Sort
			for(s32 i = 0; i < numPowerups; i++){
				for(s32 j = i; j < numPowerups - 1; j++){
					if (powerupTimes[j] < powerupTimes[j + 1]){
						SWAP(powerupTimes[j], powerupTimes[j + 1]);
						SWAP(powerups[j], powerups[j + 1]);
					}
				}
			}
			// Draw
			for(s32 i = 0; i < numPowerups; i++){
				v2 texDim = V2(32.f);
				v2 texPos = V2(((s32)powerups[i] - (s32)FIRST_BAD_DROP)*texDim.x, 80.f);
				f32 scale = 1.f;
				s32 columns = 3;
				v2 p = {20.f + (texDim.x*scale + 11.f)*(i % columns), 170 + (i/columns)*50.f};
				f32 r1 = texDim.y*scale/2 - 2.f;
				f32 r2 = texDim.y*scale/2 + 4.f;
				DrawRing(Vector2_(p + texDim*scale/2), r1, r2, 180 + 360*powerupTimes[i], 540, 48, Color_(V4(.1f, .1f, .1f, .95f)));
				DrawRing(Vector2_(p + texDim*scale/2), r1, r2, 180, 180 + 360*powerupTimes[i], 48, Color_(V4(1.f, 1.f, 1.f, .95f)));
				DrawSprite(texPos, texDim, p, V2(scale), V4_White());
			}
		}

		// Draw shape slots
		for(s32 i = 0; i < ArrayCount(gs->availableSlots) + ArrayCount(gs->nextSlots); i++){
			brick_shape_slot *slot = (i < ArrayCount(gs->availableSlots) ? &gs->availableSlots[i] : &gs->nextSlots[i - ArrayCount(gs->availableSlots)]);
			
			v2 slotPos = slotPos0 + V2(0, slotPosOffsetY*i);
			Color outlineColor = LIGHTGRAY;
			Color fillColor = (slot->occupied ? Color_(backgroundColor) : BLACK);
			if (i < ArrayCount(gs->availableSlots)) {
				if (slot->occupied)
					outlineColor = WHITE;
			}else{
				slotPos.y += 60.f; 
				outlineColor = GRAY;
				fillColor = Color_(V4(.2f, .14f, .19f, 1.f));
			}
			// Draw frame
			f32 m = 3.f;

			DrawRectangleV(Vector2_(slotPos + V2(m)), Vector2_(slotDim - V2(2*m)), fillColor);
			DrawRectangleV(Vector2_(slotPos), Vector2_(V2(slotDim.x - m, m)), outlineColor);
			DrawRectangleV(Vector2_(V2(slotPos.x + slotDim.x - m, slotPos.y)), Vector2_(V2(m, slotDim.y - m)), outlineColor);
			DrawRectangleV(Vector2_(V2(slotPos.x + m, slotPos.y + slotDim.y - m)), Vector2_(V2(slotDim.x - m, m)), outlineColor);
			DrawRectangleV(Vector2_(V2(slotPos.x, slotPos.y + m)), Vector2_(V2(m, slotDim.y - m)), outlineColor);

			// Draw shape
			if (slot->occupied){
				v2 space = slotDim - V2(2*(m + 5.f));
				f32 scale = Min(.6f, space.x/(Max(slot->shapeDim.x, slot->shapeDim.y)*gs->tileDim.x)); // Avoids choppy rotations
				
				DrawBrickShape(slot, slotPos + slotDim/2, scale);
			}
		}
		
		// Rotate shape buttons
		for(s32 i = 0; i < ArrayCount(gs->availableSlots); i++){
			auto slot = &gs->availableSlots[i];
			v2 slotPos = slotPos0 + V2(0, slotPosOffsetY*i);
			f32 m = 7.f;
			v2 buttonDim = V2(34.f);
			v2 buttonPos = slotPos + V2(slotDim.x + m, (slotDim.y - buttonDim.y)/2);
			if (DoButton(311 + i*2, buttonPos, buttonDim, "", V4_Grey(.25f), 2)){
				if (slot->occupied && !freeze){
					RotateShape90Degrees(slot, true); // CW
				}
			}
			buttonPos.x = slotPos.x - m - buttonDim.x;
			if (DoButton(312 + i*2, buttonPos, buttonDim, "", V4_Grey(.25f), 3)){
				if (slot->occupied && !freeze){
					RotateShape90Degrees(slot, false); // CCW
				}
			}
		}

		// Loading bar arrow
		{
			f32 regionX = gs->viewPos.x + gs->viewDim.x;
			f32 regionW = gs->winDim.x - regionX;
			
			Color emptyColor = BLACK;
			Color fillColor = WHITE;//GREEN;
			v2 triDim = {46.f, 23.f};
			v2 rectDim = {14.f, 32.f};
			v2 triPos  = {regionX + regionW/2 - triDim.x/2, gs->winDim.y/2 - (triDim.y + rectDim.y)/2};
			v2 rectPos = {regionX + regionW/2 - rectDim.x/2, triPos.y + triDim.y};
			f32 t = Clamp01(gs->spawnShapeTimer/gs->spawnShapeTime);
			f32 rectT = Clamp01(t*(rectDim.y + triDim.y)/(rectDim.y));
			f32 triT = Clamp01((t*(rectDim.y + triDim.y) - rectDim.y)/(triDim.y));
			// Arrow triangle
			v2 tri0 = triPos + V2(0, triDim.y);
			v2 tri1 = triPos + triDim;
			v2 tri2 = triPos + V2(triDim.x/2, 0);
			if (triT == 0){
				DrawTriangle(Vector2_(tri0), Vector2_(tri1), Vector2_(tri2), emptyColor);
			}else if (triT == 1.f){
				DrawTriangle(Vector2_(tri0), Vector2_(tri1), Vector2_(tri2), fillColor);
			}else{
				v2 tri02 = LerpV2(tri0, tri2, triT);
				v2 tri12 = LerpV2(tri1, tri2, triT);
				DrawTriangle(Vector2_(tri0), Vector2_(tri1), Vector2_(tri12), fillColor);
				DrawTriangle(Vector2_(tri0), Vector2_(tri12), Vector2_(tri02), fillColor);
				DrawTriangle(Vector2_(tri02), Vector2_(tri12), Vector2_(tri2), emptyColor);
			}
			// Arrow rectangle
			if (rectT == 0){
				DrawRectangleV(Vector2_(rectPos), Vector2_(rectDim), emptyColor);
			}else if (rectT == 1.f){
				DrawRectangleV(Vector2_(rectPos), Vector2_(rectDim), fillColor);
			}else{
				DrawRectangleV(Vector2_(rectPos + V2(0, (1.f - rectT)*rectDim.y)), Vector2_(V2(rectDim.x, rectT*rectDim.y)), fillColor);
				DrawRectangleV(Vector2_(rectPos), Vector2_(V2(rectDim.x, (1.f - rectT)*rectDim.y)), emptyColor);
			}
		}

		// Draw dragging shape
		if (gs->draggingShapeIndex != -1){
			auto slot = &gs->availableSlots[gs->draggingShapeIndex];
			if (isDraggingShapeOnWorld){
				v4 prevColor = slot->shape.color;
				slot->shape.color = (isDraggingShapePosValid ? slot->shape.color : V4_Grey(.5f));
				v2 p = gs->viewPos + Hadamard(V2(draggingShapeTilePos) + V2(slot->shapeDim)/2, gs->tileDim);
				DrawBrickShape(slot, p, 1.f, .6f);
				slot->shape.color = prevColor;
			}else{
				DrawBrickShape(slot, gs->mousePos, 1.f, .6f);
			}
			// Dotted line indicating the end of the grid
			for(s32 x = 0; x < gs->gridDim.x;  x++){
				DrawRectangleV(Vector2_(gs->viewPos + V2(x*gs->tileDim.x + 3.f, gs->gridDim.y*gs->tileDim.y)), Vector2_(V2(gs->tileDim.x - 2*3.f, 4.f)), Color_(V4_Grey(.5f, .2f)));
			}
		}
		
		if (gs->gameEnded){
			f32 h = 220.f;
			DrawRectangleV(Vector2_(V2(0, (gs->winDim.y - h)/2)), Vector2_(V2(gs->winDim.x, h)), Fade(BLACK, .5f));
			
			char *textAttackerWon = "The paddle won!";
			char *textDefenderWon = "The bricks won!";
			char *titleText = (gs->paddleWon ? textAttackerWon : textDefenderWon);
			f32 titleFontSize = 30;
			if (gs->paddleWon){
				char *titleText = "The paddle won!";
				DrawText(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, (gs->winDim.y - h)/2 + 30.f, titleFontSize, WHITE);
			}else{
				char *titleText = "The bricks won!";
				v4 colors[] = { {1.f, .44f, .4f, 1.f},
								{1.f, .7f, .3f, 1.f},
								{1.f, 1.f, .3f, 1.f},
								{.3f, 1.f, .3f, 1.f}, 
								{.35f, .72f, 1.f, 1.f},
								{.9f, .47f, 1.f, 1.f} };
				DrawTextColorful(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, (gs->winDim.y - h)/2 + 30.f, titleFontSize, colors, ArrayCount(colors));
			}
			v2 buttonDim = defaultButtonDim;
			v2 buttonPos = {(gs->winDim.x - buttonDim.x)/2, gs->winDim.y/2 - buttonDim.y/2 + 20.f};
			if (DoButton(301, buttonPos, buttonDim, "Main Menu", defaultButtonColor, 1) || IsKeyPressed(KEY_ENTER)){
				gs->metaState = MetaState_MainMenu;
			}
		}else if (gs->pause){
			f32 h = 220.f;
			DrawRectangleV(Vector2_(V2(0, (gs->winDim.y - h)/2)), Vector2_(V2(gs->winDim.x, h)), Fade(BLACK, .5f));
			
			char *titleText = "Pause";
			f32 titleFontSize = 30;
			DrawText(titleText, (gs->winDim.x - MeasureText(titleText, titleFontSize))/2, (gs->winDim.y - h)/2 + 30.f, titleFontSize, WHITE);
			
			v2 buttonDim = defaultButtonDim;
			v2 buttonPos = {(gs->winDim.x - buttonDim.x)/2, gs->winDim.y/2 - 5.f - buttonDim.y/2};
			if (DoButton(302, buttonPos, buttonDim, "Continue", defaultButtonColor, 1)){
				gs->pause = false;
			}
			buttonPos.y += buttonDim.y + 20.f;
			if (DoButton(303, buttonPos, buttonDim, "Quit", defaultButtonColor, 1)){
				gs->metaState = MetaState_MainMenu;
			}
		}

	}else{
		// Invalid
		ClearBackground(RED);
	}

	if (0){ // Draw delta time.
		char buf[100] = {};
		sprintf(buf, "GetFrameTime(): %.3f\nGetTime() delta: %.3f\ndt: %.3f", getFrameTime, getTimeDelta, dt);
		DrawText(buf, 8 + 1, 8 + 1, 10, Fade(BLACK, .5f));
		DrawText(buf, 8, 8, 10, WHITE);
	}

    EndDrawing();

	FinishFrameForGui();
}