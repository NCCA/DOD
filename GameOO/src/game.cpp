// based on https://github.com/aras-p/dod-playground

#include "game.h"
#include <vector>
#include <string>
#include <math.h>
#include <ngl/Util.h>
#include <ngl/Random.h>
#include <ngl/Vec4.h>
#include <ngl/NGLStream.h>
#include <iostream>

// -------------------------------------------------------------------------------------------------
// super simple "component system"

class GameObject;
class Component;

typedef std::vector<Component*> ComponentVector;
typedef std::vector<GameObject*> GameObjectVector;


// Component base class. Knows about the parent game object, and has some virtual methods.
class Component
{
public:
    Component() : m_GameObject(nullptr) {}
    virtual ~Component() {}
    
    virtual void Start() {}
    virtual void Update(float deltaTime) {}

    const GameObject& GetGameObject() const { return *m_GameObject; }
    GameObject& GetGameObject() { return *m_GameObject; }
    void SetGameObject(GameObject& go) { m_GameObject = &go; }
    bool HasGameObject() const { return m_GameObject != nullptr; }

private:
    GameObject* m_GameObject;
};


// Game object class. Has an array of components.
class GameObject
{
public:
    GameObject(const std::string&& name) : m_Name(name) { }
    ~GameObject()
    {
        // game object owns the components; destroy them when deleting the game object
        for (auto c : m_Components) delete c;
    }

    // get a component of type T, or null if it does not exist on this game object
    template<typename T>
    T* GetComponent()
    {
        for (auto i : m_Components)
        {
            T* c = dynamic_cast<T*>(i);
            if (c != nullptr)
                return c;
        }
        return nullptr;
    }

    // add a new component to this game object
    void AddComponent(Component* c)
    {
        assert(!c->HasGameObject());
        c->SetGameObject(*this);
        m_Components.emplace_back(c);
    }
    
    void Start() { for (auto c : m_Components) c->Start(); }
    void Update(float deltaTime) { for (auto c : m_Components) c->Update(deltaTime); }
    
private:
    std::string m_Name;
    ComponentVector m_Components;
};

// The "scene": array of game objects.
static GameObjectVector s_Objects;


// Finds all components of given type in the whole scene
template<typename T>
static ComponentVector FindAllComponentsOfType()
{
    ComponentVector res;
    for (auto go : s_Objects)
    {
        T* c = go->GetComponent<T>();
        if (c != nullptr)
            res.emplace_back(c);
    }
    return res;
}

// Find one component of given type in the scene (returns first found one)
template<typename T>
static T* FindOfType()
{
    for (auto go : s_Objects)
    {
        T* c = go->GetComponent<T>();
        if (c != nullptr)
            return c;
    }
    return nullptr;
}



// -------------------------------------------------------------------------------------------------
// components we use in our "game"


// 2D position: just x,y coordinates
struct PositionComponent : public Component
{
     ngl::Vec2  pos;
};


// Sprite: color, sprite index (in the sprite atlas), and scale for rendering it
struct SpriteComponent : public Component
{
  ngl::Vec4 colourScale;

};


// World bounds for our "game" logic: x,y minimum & maximum values
struct WorldBoundsComponent : public Component
{
    float xMin, xMax, yMin, yMax;
};


// Move around with constant velocity. When reached world bounds, reflect back from them.
struct MoveComponent : public Component
{
    float velx, vely;
    WorldBoundsComponent* bounds;
    
    MoveComponent(float minSpeed, float maxSpeed)
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

    virtual void Start() override
    {
        bounds = FindOfType<WorldBoundsComponent>();
    }
    
    virtual void Update(float deltaTime) override
    {
        // get Position component on our game object
        PositionComponent* pos = GetGameObject().GetComponent<PositionComponent>();
        
        // update position based on movement velocity & delta time
        pos->pos.m_x += velx * deltaTime;
        pos->pos.m_y += vely * deltaTime;
        // check against world bounds; put back onto bounds and mirror the velocity component to "bounce" back
        if (pos->pos.m_x < bounds->xMin)
        {
            velx = -velx;
            pos->pos.m_x = bounds->xMin;
        }
        if (pos->pos.m_x > bounds->xMax)
        {
            velx = -velx;
            pos->pos.m_x = bounds->xMax;
        }
        if (pos->pos.m_y < bounds->yMin)
        {
            vely = -vely;
            pos->pos.m_y = bounds->yMin;
        }
        if (pos->pos.m_y > bounds->yMax)
        {
            vely = -vely;
            pos->pos.m_y = bounds->yMax;
        }
    }
};


// When present, tells things that have Avoid component to avoid this object
struct AvoidThisComponent : public Component
{
    float distance;
};


// Objects with this component "avoid" objects with AvoidThis component:
// - when they get closer to them than Avoid::distance, they bounce back,
// - also they take sprite color from the object they just bumped into
struct AvoidComponent : public Component
{
    static ComponentVector avoidList;
    
    virtual void Start() override
    {
        // fetch list of objects we'll be avoiding, if we haven't done that yet
        if (avoidList.empty())
            avoidList = FindAllComponentsOfType<AvoidThisComponent>();
    }
    
//    static float DistanceSq(const PositionComponent* a, const PositionComponent* b)
//    {
//        float dx = a->pos.m_x - b->pos.m_x;
//        float dy = a->pos.m_y - b->pos.m_y;
//        return dx * dx + dy * dy;
//    }
    
    void ResolveCollision(float deltaTime)
    {
        MoveComponent* move = GetGameObject().GetComponent<MoveComponent>();
        // flip velocity
        move->velx = -move->velx;
        move->vely = -move->vely;
        
        // move us out of collision, by moving just a tiny bit more than we'd normally move during a frame
        PositionComponent* pos = GetGameObject().GetComponent<PositionComponent>();
        pos->pos.m_x += move->velx * deltaTime * 1.1f;
        pos->pos.m_y += move->vely * deltaTime * 1.1f;
    }

    virtual void Update(float time) override
    {
        PositionComponent* myposition = GetGameObject().GetComponent<PositionComponent>();
        // check each thing in avoid list
        for (auto avc : avoidList)
        {
            AvoidThisComponent* av = (AvoidThisComponent*)avc;

            PositionComponent* avoidposition = av->GetGameObject().GetComponent<PositionComponent>();
            // is our position closer to "thing to avoid" position than the avoid distance?
            if ((myposition->pos-avoidposition->pos).lengthSquared() < av->distance * av->distance)
            {
                ResolveCollision(time);

                // also make our sprite take the color of the thing we just bumped into
                SpriteComponent* avoidSprite = av->GetGameObject().GetComponent<SpriteComponent>();
                SpriteComponent* mySprite = GetGameObject().GetComponent<SpriteComponent>();
                mySprite->colourScale.m_x = avoidSprite->colourScale.m_x;
                mySprite->colourScale.m_y = avoidSprite->colourScale.m_y;
                mySprite->colourScale.m_z = avoidSprite->colourScale.m_z;
            }
        }
    }
};

ComponentVector AvoidComponent::avoidList;


// -------------------------------------------------------------------------------------------------
// "the game"


void initialize(size_t _numObjects, size_t _numAvoid)
{
    // create "world bounds" object
    {
        GameObject* go = new GameObject("bounds");
        WorldBoundsComponent* bounds = new WorldBoundsComponent();
        bounds->xMin = -180.0f;
        bounds->xMax =  180.0f;
        bounds->yMin = -150.0f;
        bounds->yMax =  150.0f;
        go->AddComponent(bounds);
        s_Objects.emplace_back(go);
    }
    WorldBoundsComponent* bounds = FindOfType<WorldBoundsComponent>();
    auto rng=ngl::Random::instance();

    // create regular objects that move
    for (size_t i = 0; i < _numObjects; ++i)
    {
        GameObject* go = new GameObject("object");

        // position it within world bounds
        PositionComponent* pos = new PositionComponent();
        pos->pos.m_x = bounds->xMin+ rng->randomPositiveNumber(  bounds->xMax-bounds->xMin);
        pos->pos.m_y = bounds->yMin+ rng->randomPositiveNumber(  bounds->yMax-bounds->yMin);
        go->AddComponent(pos);

        // setup a sprite for it (random sprite index from first 5), and initial white color
        SpriteComponent* sprite = new SpriteComponent();
        sprite->colourScale.set(1.0f,1.0f,1.0f,5.0f);
        go->AddComponent(sprite);

        // make it move
        MoveComponent* move = new MoveComponent(15.3f, 80.6f);
        go->AddComponent(move);

        // make it avoid the bubble things
        AvoidComponent* avoid = new AvoidComponent();
        go->AddComponent(avoid);

        s_Objects.emplace_back(go);
    }

    // create objects that should be avoided
    for (auto i = 0; i < _numAvoid; ++i)
    {
        GameObject* go = new GameObject("toavoid");
        
        // position it in small area near center of world bounds
        PositionComponent* pos = new PositionComponent();
        pos->pos.m_x = rng->randomNumber(  bounds->xMax) * 0.5f;
        pos->pos.m_y = rng->randomNumber(  bounds->yMax) * 0.5f;
        go->AddComponent(pos);

        // setup a sprite for it (6th one), and a random color
        SpriteComponent* sprite = new SpriteComponent();
        sprite->colourScale = rng->getRandomColour3();
        sprite->colourScale.clamp(0.25f,1.0f);
        sprite->colourScale.m_w = 25.0f;
        go->AddComponent(sprite);
        
        // make it move, slowly
        MoveComponent* move = new MoveComponent(2.0f, 12.0f);
        go->AddComponent(move);
        
        // setup an "avoid this" component
        AvoidThisComponent* avoid = new AvoidThisComponent();
        avoid->distance = (sprite->colourScale.m_a/2.0f)-5.0f;
        go->AddComponent(avoid);
        
        s_Objects.emplace_back(go);
    }

    // call Start on all objects/components once they are all created
    for (auto go : s_Objects)
    {
        go->Start();
    }
}

void teardown()
{
    // just delete all objects/components
    for (auto go : s_Objects)
        delete go;
    s_Objects.clear();
}


size_t update(RenderData* data, float time)
{
    int objectCount = 0;
    // go through all objects
    for (auto go : s_Objects)
    {
        // Update all their components
        go->Update(time);

        // For objects that have a Position & Sprite on them: write out
        // their data into destination buffer that will be rendered later on.
        //
        // Using a smaller global scale "zooms out" the rendering, so to speak.
        PositionComponent* pos = go->GetComponent<PositionComponent>();
        SpriteComponent* sprite = go->GetComponent<SpriteComponent>();
        if (pos != nullptr && sprite != nullptr)
        {
            RenderData& spr = data[objectCount++];
            //spr.pos.m_x=pos->pos.m_x;
            //spr.pos.m_y=pos->pos.m_y;
            spr.pos=pos->pos;
            spr.colourScale=sprite->colourScale;

            //spr.colourScale.m_z=globalScale;
            //spr.

        }
    }
    return objectCount;
}

