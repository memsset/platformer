#include <Engine.h>


Engine* Engine::sInstance = nullptr;


bool Engine::Initialize(std::string title, const unsigned int& width, const unsigned int& height)
{
    if (SDL_Init(SDL_INIT_EVERYTHING) < 0)
    {
        std::cout << "[SDL2]: SDL_Init() failed   : " << SDL_GetError() << '\n';
        return false;
    }
    if (!(IMG_Init(IMG_INIT_PNG)))
    {
        std::cout << "[SDL_image]: IMG_Init() failed  :" << IMG_GetError() << '\n';
        return false;
    }
    if (TTF_Init() == -1)
    {
        std::cout << "[SDL_TTF]: TTF_Init() failed   : " << TTF_GetError() << std::endl;
        return false;
    }
    if (Mix_Init(MIX_INIT_OGG) && Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1)
    {
        std::cout << "[SDL_mixer]: Mix_Init() failed   : " << Mix_GetError() << std::endl;
        return false;
    }

    Window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, SDL_WINDOW_SHOWN);
    if (!Window)
    {
        std::cout << "[SDL_CreateWindow]: Could not create window.   : " << SDL_GetError() << std::endl;
        return false;
    }

    Renderer = SDL_CreateRenderer(Window, -1, SDL_RENDERER_ACCELERATED || SDL_RENDERER_PRESENTVSYNC);
    if (!Renderer)
    {
        std::cout << "[SDL2]: Could not create renderer.   : " << SDL_GetError() << std::endl;
        return false;
    }

    EngineTimer.Start();

    Width = width;
    Height = height;
    
    CurrentTick = SDL_GetPerformanceCounter();
    LastTick = 0;

    PhysicsDebugger.SetFlags(b2Draw::e_shapeBit);
    PhysicsSystem.GetWorld()->SetDebugDraw(&PhysicsDebugger);

    //Used for collision callbacks
    collisionListener = CollisionListener();
    PhysicsSystem.World->SetContactListener(&collisionListener);

    RegisterSolid(b2Vec2(0, -4), b2Vec2(14.2f, 0.5f));
    RegisterActor(b2Vec2(1.2f, 4), b2Vec2(0.5f, 0.5f), 0, 12, 1, 0.1f);

    Player = RegisterPlayer(b2Vec2(0, 0), b2Vec2(0.45f, 0.65f), "../../res/img/Solid_red.png");
    std::cout << Player << '\n';
    

    return true;
}

Engine* Engine::Get()
{
    return sInstance = (sInstance != nullptr) ? sInstance : new Engine();
}

SDLTimer* Engine::GetTimer()
{
    return &EngineTimer;
}

SDL_Renderer* Engine::GetRenderer()
{
    return Renderer;
}

EventHandler* Engine::GetEventHandler()
{
    return &KeyboardInput;
}

std::size_t Engine::CreateEntity()
{
    static std::size_t Entities;
    ++Entities;

    MaxEntities = Entities;
    return Entities;
}

std::size_t Engine::GetMaxEntity()
{
    return MaxEntities;
}

SDL_Surface* Engine::GetWindowSurface()
{
    return SDL_GetWindowSurface(Window);
}

Registry* Engine::GetRegistry()
{
    return &EngineRegistry;
}

void Engine::Update()
{
    LastTick = CurrentTick;
    CurrentTick = SDL_GetPerformanceCounter();
    
    float Dt = (float)((CurrentTick - LastTick) * 10 / (float)SDL_GetPerformanceFrequency());
    
    FPS = FrameCount / (EngineTimer.GetTicks() / 1000.0f);
    
    PlayerSystem.Update(Player, EngineRegistry, 0, PhysicsSystem.GetWorld(), PhysicsDebugger);
    PhysicsSystem.Update(Dt, EngineRegistry);
    GraphicsSystem.Update(EngineRegistry);


    FrameCount++;
}

std::size_t Engine::RegisterSolid(const b2Vec2& position, const b2Vec2& dimensions)
{
    //Box2D Initialization
    std::size_t Solid = CreateEntity();
    EngineRegistry.regUser[Solid].ECS_ID = Solid;

    b2BodyDef defSolid;
    defSolid.type = b2_staticBody;
    defSolid.position.Set(position.x, position.y);

    b2Body* bodySolid = PhysicsSystem.World->CreateBody(&defSolid);

    b2PolygonShape shapeSolid;
    shapeSolid.SetAsBox(dimensions.x / 2, dimensions.y / 2);

    b2FixtureDef fixtSolid;
    fixtSolid.shape = &shapeSolid;
    fixtSolid.density = 0;
    fixtSolid.userData.pointer = reinterpret_cast<uintptr_t>(&EngineRegistry.regUser[Solid]);

    b2Fixture* SolidFixture = bodySolid->CreateFixture(&fixtSolid);
        
    //Body enters the Registry
    EngineRegistry.regPhysics[Solid].body = bodySolid;
    
    return Solid;
}

std::size_t Engine::RegisterActor(const b2Vec2& position, const b2Vec2& dimensions, const bool& kinematic, 
const float& angle, const float& density, const float& frictionCoeff)
{
    std::size_t Actor = CreateEntity();
    EngineRegistry.regUser[Actor].ECS_ID = Actor;

    
    b2BodyDef defActor;
    if (kinematic)
        defActor.type = b2_kinematicBody;
    else   
        defActor.type = b2_dynamicBody; 
    defActor.allowSleep = false;
    defActor.angle = angle;
    defActor.position.Set(position.x, position.y);

    b2PolygonShape shapeActor;
    shapeActor.SetAsBox(dimensions.x / 2, dimensions.y / 2);

    b2Body* bodyActor = PhysicsSystem.World->CreateBody(&defActor);

    b2FixtureDef fixtActor;
    fixtActor.shape = &shapeActor;
    fixtActor.userData.pointer = reinterpret_cast<uintptr_t>(&EngineRegistry.regUser[Actor]);
    
    fixtActor.density = density;
    fixtActor.friction = frictionCoeff;
    
    b2Fixture* FixtureActor = bodyActor->CreateFixture(&fixtActor);

    EngineRegistry.regPhysics[Actor].body = bodyActor;
    EngineRegistry.regActor[Actor].PreviousPosition = b2Vec2(position.x, position.y);
    
    return Actor;
}

std::size_t Engine::RegisterPlayer(const b2Vec2& position, const b2Vec2& dimensions, const std::string& texturePath)
{
    std::size_t Player = CreateEntity();
    EngineRegistry.regUser[Player].ECS_ID = Player;
    EngineRegistry.regUser[Player].GroundCheck = true;

    b2BodyDef defPlayer;
    defPlayer.type = b2_dynamicBody;
    defPlayer.fixedRotation = true;
    defPlayer.allowSleep = false;
    defPlayer.position.Set(position.x, position.y);

    b2Body* bodyPlayer = PhysicsSystem.World->CreateBody(&defPlayer);

    b2PolygonShape shapePlayer;
    shapePlayer.SetAsBox(dimensions.x / 2, dimensions.y / 2);

    b2FixtureDef fixtPlayer;
    fixtPlayer.shape = &shapePlayer;
    fixtPlayer.density = 0.9f;
    fixtPlayer.friction = 0.3f;
    fixtPlayer.restitution = 0.1f;
    fixtPlayer.userData.pointer = reinterpret_cast<uintptr_t>(&EngineRegistry.regUser[Player]);

    b2Fixture* FixturePlayer = bodyPlayer->CreateFixture(&fixtPlayer);

    b2FixtureDef fixtGroundCheck;
    shapePlayer.SetAsBox(((dimensions.x / 2) - 0.02f), 0.2f, b2Vec2(0, -((dimensions.y / 2))), 0);
    fixtGroundCheck.shape = &shapePlayer;
    fixtGroundCheck.isSensor = true;
    fixtGroundCheck.userData.pointer = reinterpret_cast<uintptr_t>(&EngineRegistry.regUser[Player]);

    FixturePlayer = bodyPlayer->CreateFixture(&fixtGroundCheck);

    std::size_t TextureID = GraphicsSystem.LoadFromFile(GraphicsSystem.GenTextureID(), texturePath.c_str());

    EngineRegistry.regGraphics[Player].TextureID = TextureID;
    EngineRegistry.regPhysics[Player].body = bodyPlayer;
    EngineRegistry.regActor[Player].PreviousPosition = b2Vec2(position.x, position.y);
    EngineRegistry.regPlayer[Player].MoveState = PlayerMoveX::STOP;
    EngineRegistry.regPlayer[Player].DoubleJump = 1;

    return Player;
}

void Engine::PollEvents()
{
    KeyboardInput.Poll();
    InputSystem.Listen(EngineRegistry, Player);
}

void Engine::Render()
{   
    SDL_SetRenderDrawColor(Renderer, 0, 0, 0, 0xFF);
    SDL_RenderClear(Renderer);

    GraphicsSystem.Render(EngineRegistry);

    PhysicsDebugger.DrawGridline(40);
    PhysicsDebugger.DrawCartesianAxis();
    PhysicsSystem.World->DebugDraw();
    SDL_RenderPresent(Renderer);
}


b2Vec2 Engine::SDLBox2D(const b2Vec2& vec2)
{
   return b2Vec2(vec2.x / 80 - 8, vec2.y / 80 - 4.5f);
}

b2Vec2 Engine::Box2DSDL(const b2Vec2& vec2)
{	
   return b2Vec2((vec2.x + 8) * 80, (vec2.y + 4.5f) * 80);
}

float Engine::Box2DSDLf(const float& f)
{	
   return f * 80;
}

float Engine::SDLBox2Df(const float& f)
{	
   return f / 80;
}

void Engine::Quit()
{
    Running = 0;
}

bool Engine::IsRunning()
{
    return Running;
}