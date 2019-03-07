// based on https://github.com/aras-p/dod-playground
#include "game.h"
#include <vector>
#include <string>
#include <math.h>
#include <assert.h>
#include <ngl/Random.h>
#include <ngl/Vec3.h>
#include <ngl/Util.h>
// -------------------------------------------------------------------------------------------------
// components we use in our "game". these are all just simple structs with some data.
// 2D position: just x,y coordinates
struct PositionComponent
{
    ngl::Vec2 pos;
};


// Sprite: color, sprite and scale for rendering it
struct SpriteComponent
{
    ngl::Vec4 colourScale;
};


// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent
{
    float xMin, xMax, yMin, yMax;
};


// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent
{
    float velx, vely;

    void initialize(float minSpeed, float maxSpeed)
    {
        auto rng=ngl::Random::instance();
        // random angle
        float angle = rng->randomPositiveNumber() * ngl::PI * 2.0f;
        // random movement speed between given min & max
        float speed = minSpeed+rng->randomPositiveNumber(maxSpeed-minSpeed);
        // velocity x & y components
        velx = cosf(angle) * speed;
        vely = sinf(angle) * speed;
    }
};


// -------------------------------------------------------------------------------------------------
// super simple "game entities system", using struct-of-arrays data layout.
// we just have an array for each possible component, and a flags array bit bits indicating
// which components are "present".

// "ID" of a game object is just an index into the scene array.
typedef size_t EntityID;

struct Entities
{
    // not these are bool flags so can't use scoped enums.
    enum
    {
        hasPosition = 1<<0,
        toRender = 1<<1,
        world = 1<<2,
        canMove = 1<<3,
    };

    // arrays of data; the sizes of all of them are the same. EntityID (just an index)
    // is used to access data for any "object/entity". The "object" itself is nothing
    // more than just an index into these arrays.
    
    // data for all components
    std::vector<PositionComponent> m_Positions;
    std::vector<SpriteComponent> m_Sprites;
    std::vector<WorldBoundsComponent> m_WorldBounds;
    std::vector<MoveComponent> m_Moves;
    // bit flags for every component, indicating whether this object "has it"
    std::vector<int> m_Flags;
    
    void reserve(size_t n)
    {
      m_Positions.reserve(n);
      m_Sprites.reserve(n);
      m_WorldBounds.reserve(n);
      m_Moves.reserve(n);
      m_Flags.reserve(n);
    }
    
    EntityID addEntity()
    {
        static EntityID id = 0;
        m_Positions.push_back(PositionComponent());
        m_Sprites.push_back(SpriteComponent());
        m_WorldBounds.push_back(WorldBoundsComponent());
        m_Moves.push_back(MoveComponent());
        m_Flags.push_back(0);
        return id++;
    }
};


// The "scene"
static Entities s_Objects;


// -------------------------------------------------------------------------------------------------
// "systems" that we have; they operate on components of game objects


struct MoveSystem
{
    EntityID boundsID; // ID if object with world bounds
    std::vector<EntityID> entities; // IDs of objects that should be moved

    void addObjectToSystem(EntityID id)
    {
        entities.emplace_back(id);
    }

    void setBounds(EntityID id)
    {
        boundsID = id;
    }
    
    void updateSystem(float deltaTime)
    {
        const WorldBoundsComponent& bounds = s_Objects.m_WorldBounds[boundsID];

        // go through all the objects
        for (size_t io = 0, no = entities.size(); io != no; ++io)
        {
            PositionComponent& pos = s_Objects.m_Positions[io];
            MoveComponent& move = s_Objects.m_Moves[io];
            
            // update position based on movement velocity & delta time
            pos.pos.m_x += move.velx * deltaTime;
            pos.pos.m_y += move.vely * deltaTime;
            
            // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
            if (pos.pos.m_x < bounds.xMin)
            {
                move.velx = -move.velx;
                pos.pos.m_x = bounds.xMin;
            }
            if (pos.pos.m_x > bounds.xMax)
            {
                move.velx = -move.velx;
                pos.pos.m_x = bounds.xMax;
            }
            if (pos.pos.m_y < bounds.yMin)
            {
                move.vely = -move.vely;
                pos.pos.m_y = bounds.yMin;
            }
            if (pos.pos.m_y > bounds.yMax)
            {
                move.vely = -move.vely;
                pos.pos.m_y = bounds.yMax;
            }
        }
    }
};

static MoveSystem s_MoveSystem;



// "Avoidance system" works out interactions between objects that "avoid" and "should be avoided".
// Objects that avoid:
// - when they get closer to things that should be avoided than the given distance, they bounce back,
// - also they take sprite color from the object they just bumped into
struct AvoidanceSystem
{
    // things to be avoided: distances to them, and their IDs
    std::vector<float> avoidDistanceList;
    std::vector<EntityID> avoidList;
    
    // objects that avoid: their IDs
    std::vector<EntityID> objectList;

    void addAvoidThisObjectToSystem(EntityID id, float distance)
    {
        avoidList.emplace_back(id);
        avoidDistanceList.emplace_back(distance * distance);
    }
    
    void addObjectToSystem(EntityID id)
    {
        objectList.emplace_back(id);
    }
    

    
    void resolveCollision(EntityID id, float deltaTime)
    {
        PositionComponent& pos = s_Objects.m_Positions[id];
        MoveComponent& move = s_Objects.m_Moves[id];
        SpriteComponent & sprite = s_Objects.m_Sprites[id];
        // flip velocity
        move.velx = -move.velx;
        move.vely = -move.vely;
        
        // move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
        pos.pos.m_x += move.velx * deltaTime * sprite.colourScale.m_a;
        pos.pos.m_y += move.vely * deltaTime * sprite.colourScale.m_a;
    }
    
    void updateSystem( float deltaTime)
    {
        auto numObjects = objectList.size();
        auto numbAvoidObjects = avoidList.size();
        // go through all the objects
        for (size_t currentObject = 0;  currentObject != numObjects; ++currentObject)
        {
            EntityID go = objectList[currentObject];
            const PositionComponent& myposition = s_Objects.m_Positions[go];

            // check each thing in avoid list
            for (size_t currentAvoid = 0; currentAvoid != numbAvoidObjects; ++currentAvoid)
            {
                float avDistance = avoidDistanceList[currentAvoid];
                EntityID avoid = avoidList[currentAvoid];
                const PositionComponent& avoidposition = s_Objects.m_Positions[avoid];
                
                // is our position closer to "thing to avoid" position than the avoid distance?
                // note code locality here, did have a function for lenghtSquared but
                // inlined and got an imporvement.
                if ((myposition.pos-avoidposition.pos).lengthSquared() < avDistance)
                {
                    resolveCollision(go, deltaTime);
                    
                    // also make our sprite take the color of the thing we just bumped into
                    SpriteComponent& avoidSprite = s_Objects.m_Sprites[avoid];
                    SpriteComponent& mySprite = s_Objects.m_Sprites[go];

                    mySprite.colourScale.m_x=avoidSprite.colourScale.m_x;
                    mySprite.colourScale.m_y=avoidSprite.colourScale.m_y;
                    mySprite.colourScale.m_z=avoidSprite.colourScale.m_z;
                }
            }
        }
    }
};

static AvoidanceSystem s_AvoidanceSystem;


// -------------------------------------------------------------------------------------------------
// "the game"



void initialize(size_t _numObjects, size_t _numAvoid)
{
    s_Objects.reserve(1 + _numObjects + _numAvoid);
    
    // create "world bounds" object
    WorldBoundsComponent bounds;
    {
        EntityID go = s_Objects.addEntity();
        s_Objects.m_WorldBounds[go].xMin = -180.0f;
        s_Objects.m_WorldBounds[go].xMax =  180.0f;
        s_Objects.m_WorldBounds[go].yMin = -150.0f;
        s_Objects.m_WorldBounds[go].yMax =  150.0f;
        bounds = s_Objects.m_WorldBounds[go];
        s_Objects.m_Flags[go] |= Entities::world;
        s_MoveSystem.setBounds(go);
    }
    auto rng=ngl::Random::instance();
    // create regular objects that move
    for (size_t i = 0; i < _numObjects; ++i)
    {
        auto id = s_Objects.addEntity();

        // position it within world bounds
        s_Objects.m_Positions[id].pos.m_x =bounds.xMin+ rng->randomPositiveNumber(  bounds.xMax-bounds.xMin);
        s_Objects.m_Positions[id].pos.m_y =bounds.yMin+ rng->randomPositiveNumber(  bounds.yMax-bounds.yMin);
        s_Objects.m_Flags[id] |= Entities::hasPosition;

        s_Objects.m_Sprites[id].colourScale.set(1.0f,1.0f,1.0f);
        s_Objects.m_Sprites[id].colourScale.m_a = 5.0f;
        s_Objects.m_Flags[id] |= Entities::toRender;

        // make it move
        s_Objects.m_Moves[id].initialize(15.3f, 80.6f);
        s_Objects.m_Flags[id] |= Entities::canMove;
        s_MoveSystem.addObjectToSystem(id);

        // make it avoid the bubble things, by adding to the avoidance system
        s_AvoidanceSystem.addObjectToSystem(id);
    }

    // create objects that should be avoided
    for (size_t i = 0; i < _numAvoid; ++i)
    {
        auto id = s_Objects.addEntity();
        
        // position it in small area near center of world bounds
        s_Objects.m_Positions[id].pos.m_x =rng->randomNumber(  bounds.xMax) * 0.5f;
        s_Objects.m_Positions[id].pos.m_y = rng->randomNumber(  bounds.yMax) * 0.5f;
        s_Objects.m_Flags[id] |= Entities::hasPosition;

        s_Objects.m_Sprites[id].colourScale=rng->getRandomColour3();
        s_Objects.m_Sprites[id].colourScale.clamp(0.25f,1.0f);

        s_Objects.m_Sprites[id].colourScale.m_a = 25.0f;
        s_Objects.m_Flags[id] |= Entities::toRender;
        
        // make it move, slowly
        s_Objects.m_Moves[id].initialize(2.0f, 12.0f);
        s_Objects.m_Flags[id] |= Entities::canMove;
        s_MoveSystem.addObjectToSystem(id);

        // add to avoidance this as "Avoid This" object
        s_AvoidanceSystem.addAvoidThisObjectToSystem(id, (s_Objects.m_Sprites[id].colourScale.m_a/2.0f)-5.0f);
    }
}


void teardown()
{

}


size_t update(RenderData* data,  float deltaTime)
{
    size_t objectCount = 0;
    
    // update object systems
    s_MoveSystem.updateSystem( deltaTime);
    s_AvoidanceSystem.updateSystem( deltaTime);
    // go through all objects
    for (size_t i = 0, n = s_Objects.m_Flags.size(); i != n; ++i)
    {
        // For objects that have a Position & Sprite on them: write out
        // their data into destination buffer that will be rendered later on.
        //
        if ((s_Objects.m_Flags[i] & Entities::hasPosition) && (s_Objects.m_Flags[i] & Entities::toRender))
        {
            RenderData& spr = data[objectCount++];
            const PositionComponent& pos = s_Objects.m_Positions[i];
            spr.pos = pos.pos ;
            const SpriteComponent& sprite = s_Objects.m_Sprites[i];
            spr.colourScale=sprite.colourScale;
        }
    }
    return objectCount;
}

