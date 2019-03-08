// based on https://github.com/aras-p/dod-playground
#ifndef GAME_H_
#define GAME_H_

#include <cstdlib>
#define kMaxSpriteCount 1100000
#include <ngl/Vec2.h>
#include <ngl/Vec3.h>
#include <ngl/Vec4.h>
struct RenderData
{
    ngl::Vec2 pos;
    ngl::Vec4 colourScale;
} ;

void initialize(size_t _numObjects, size_t _numAvoid);
void teardown();
// returns amount of sprites
size_t update(RenderData* data, float deltaTime);


#endif
