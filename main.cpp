#include <algorithm>
#include <chrono>
#include <random>

#include "include/LOOMEngine.h"

constexpr auto SCREEN_W = 576;
constexpr auto SCREEN_H = 1024;

constexpr auto PLAYER_X = -144.0f;
constexpr auto PLAYER_SPAWN_Y = 200.0f;
constexpr auto PIPE_SPAWN_INTERVAL = 2.0f;
constexpr auto PIPE_SCROLL_SPEED = 150.0f;
constexpr auto PIPE_DESPAWN_X = -350.0f;
constexpr auto PIPE_SPAWN_X = 350.0f;
constexpr auto PIPE_GAP = 900.0f;
constexpr auto BIRD_JUMP_FORCE = 650.0f;
constexpr auto BIRD_TILT_SENSITIVITY = 0.05f;
constexpr auto BIRD_TILT_SMOOTHING = 10.0f;
constexpr auto BIRD_MAX_UP_TILT = -30.0f;
constexpr auto BIRD_MAX_DOWN_TILT = 90.0f;

constexpr auto START_PROMPT_SPRITE = "./assets/sprites/message.png";
constexpr auto GOVER_PROMPT_SPRITE = "./assets/sprites/gameover.png";
constexpr auto BG_SPRITE = "./assets/sprites/background-night.png";
constexpr auto BASE_SPRITE = "./assets/sprites/base.png";
constexpr auto BIRD_SPRITE = "./assets/sprites/redbird-midflap.png";
constexpr auto PIPE_SPRITE = "./assets/sprites/pipe-red.png";

constexpr auto SCORE_TEXT = "SCORE: ";

constexpr auto MUSIC = "./assets/audio/music.mp3";
constexpr auto DEATH_SFX = "./assets/audio/hit.wav";
constexpr auto FLAP_SFX = "./assets/audio/wing.wav";
constexpr auto POINT_SFX = "./assets/audio/point.wav";

constexpr auto FONT = "./assets/main_font.ttf";

using namespace LOOM;
using namespace LOOM::Math;
using namespace LOOM::Utilities;
using namespace LOOM::Fibers;

// Custom Fibers

struct PlayerTag {};
struct PipeTag {};
struct ScorePoint {};
struct StartPromptTag {};
struct GameOverPromptTag {};

struct Timer
{
    float elapsed = 0.0f;
};

struct Score 
{
    int value = 0;
};

// Helpers

static int RandomInt(int min, int max)
{
    static std::mt19937 gen(std::chrono::steady_clock::now().time_since_epoch().count());
    return std::uniform_int_distribution<int>{min, max}(gen);
}

static void ApplyJump(PhysicsBody& body, Vector force)
{
    body.velocity = force / body.mass;
}

static void ResetGame(Loom& loom)
{
    auto& fabric = loom.GetFabric();

    for (auto& player : fabric.FetchAllTowsOf<PlayerTag>())
        fabric.UnwindTow(player);

    for (auto& pipe : fabric.FetchAllTowsOf<PipeTag>())
        fabric.UnwindTow(pipe);

    if (auto timer = fabric.FirstFiber<Timer>())
        timer->elapsed = 0.0f;

    loom.RemoveShuttle("PipeGeneratorShuttle");

    fabric.SpawnPrefab("GameOverPrompt");
}

// Shuttles Singnatures

static void StartPromptShuttle(Loom&, std::vector<Tow>&, float);
static void GameOverPromptShuttle(Loom&, std::vector<Tow>&, float);
static void PipeGeneratorShuttle(Loom&, float);
static void PipeScrollerShuttle(Loom&, std::vector<Tow>&, float);
static void PlayerControllerShuttle(Loom&, std::vector<Tow>&, float);
static void ScoreDetectionShuttle(Loom&, std::vector<Tow>&, float);
static void LoseConditionShuttle(Loom&, std::vector<Tow>&, float);
static void ScoreTextUpdateShuttle(Loom&, std::vector<Tow>&, float);

// Main

int main()
{
    Loom loom;
    loom.Initialize("LOOM Sandbox - Flappy Bird", false, SCREEN_W, SCREEN_H);

    auto& fabric = loom.GetFabric();

    // Tows

    auto background = fabric.CreateTow();
    background.PlyFiber<Transform>().scale *= 2.0f;
    background.PlyFiber<Pattern>(BG_SPRITE, -10);

    auto base = fabric.CreateTow();
    base.PlyFiber<Transform>(Vector(0.0f, -400.0f), 0.0f, Vector::one() * 2.0f);
    base.PlyFiber<Pattern>(BASE_SPRITE);
    base.PlyFiber<BoxCollider>(Vector(672, 224), Vector::zero(), true);

    auto scoreCounter = fabric.CreateTow();
    scoreCounter.PlyFiber<Transform>(Vector(0.0f, 475.0f));
    scoreCounter.PlyFiber<Text>(SCORE_TEXT, FONT, 48, Color::white());
    scoreCounter.PlyFiber<Score>();

    fabric.CreateTow().PlyFiber<Timer>();
    fabric.CreateTow().PlyFiber<AudioPlayer>();

    // Prefabs

    fabric.PreFabricate("StartPrompt")
        .PlyFiber<Transform>(Vector::zero(), 0.0f, Vector::one() * 2.0f)
        .PlyFiber<Pattern>(START_PROMPT_SPRITE, 10)
        .PlyFiber<BoxCollider>(Vector(SCREEN_W, SCREEN_H))
        .PlyFiber<Clickable>()
        .PlyFiber<StartPromptTag>()
        .Register();

    fabric.PreFabricate("GameOverPrompt")
        .PlyFiber<Transform>(Vector::zero(), 0.0f, Vector::one() * 2.0f)
        .PlyFiber<Pattern>(GOVER_PROMPT_SPRITE, 10)
        .PlyFiber<BoxCollider>(Vector(SCREEN_W, SCREEN_H))
        .PlyFiber<Clickable>()
        .PlyFiber<GameOverPromptTag>()
        .Register();

    fabric.PreFabricate("Player")
        .PlyFiber<Transform>(Vector(PLAYER_X, PLAYER_SPAWN_Y), 0.0f, Vector::one() * 2.0f)
        .PlyFiber<Pattern>(BIRD_SPRITE)
        .PlyFiber<BoxCollider>(Vector::one() * 50.0f)
        .PlyFiber<PhysicsBody>()
        .PlyTag<PlayerTag>()
        .Register();

    fabric.PreFabricate("UpperPipe")
        .PlyFiber<Transform>(Vector(PIPE_SPAWN_X, PIPE_GAP * 0.5f), 0.0f, Vector::one() * 2.0f)
        .PlyFiber<Pattern>(PIPE_SPRITE, 0, false, true)
        .PlyFiber<BoxCollider>(Vector(104.0f, 648.0f))
        .PlyTag<PipeTag>()
        .PlyTag<ScorePoint>()
        .Register();

    fabric.PreFabricate("LowerPipe")
        .PlyFiber<Transform>(Vector(PIPE_SPAWN_X, -(PIPE_GAP * 0.5f)), 0.0f, Vector::one() * 2.0f)
        .PlyFiber<Pattern>(PIPE_SPRITE, 0, false, false)
        .PlyFiber<BoxCollider>(Vector(104.0f, 648.0f))
        .PlyTag<PipeTag>()
        .Register();

    // Spawn Prefabs

    fabric.SpawnPrefab("StartPrompt");

    // Shuttles

    loom.AddBoundShuttle<StartPromptTag>(StartPromptShuttle, ShuttleShed::Update, 0);
    loom.AddBoundShuttle<GameOverPromptTag>(GameOverPromptShuttle, ShuttleShed::Update, 0);
    loom.AddBoundShuttle<PipeTag>(PipeScrollerShuttle, ShuttleShed::Update, 2);
    loom.AddBoundShuttle<PlayerTag>(PlayerControllerShuttle, ShuttleShed::Update, 3);
    loom.AddBoundShuttle<ScorePoint>(ScoreDetectionShuttle, ShuttleShed::Update, 4);
    loom.AddBoundShuttle<Score, Text>(ScoreTextUpdateShuttle, ShuttleShed::PostUpdate, 0);
    loom.AddBoundShuttle<PlayerTag>(LoseConditionShuttle, ShuttleShed::PostUpdate, 1);

    // Settings 

    loom.Settings().globalGravityScale *= 2.0f;

    auto& musicPlayer = loom.GetService<MusicPlayer>();
    musicPlayer.musicPath = MUSIC;
    musicPlayer.Play();

    loom.Start();
    return 0;
}

// Shuttle Definitions

static void StartPromptShuttle(Loom& loom, std::vector<Tow>& promptTows, float)
{
    auto& fabric = loom.GetFabric();

    auto prompt = promptTows[0];
    auto& clickable = prompt.FetchFiber<Clickable>();
    if (!clickable.IsClicked())
        return;

    fabric.UnwindTow(prompt);
    fabric.SpawnPrefab("Player");

    loom.AddShuttle(PipeGeneratorShuttle, ShuttleShed::Update, 1, "PipeGeneratorShuttle");
}

static void GameOverPromptShuttle(Loom& loom, std::vector<Tow>& promptTows, float)
{
    auto& fabric = loom.GetFabric();

    auto& prompt = promptTows[0];
    auto& clickable = prompt.FetchFiber<Clickable>();
    if (!clickable.IsClicked())
        return;

    if (auto score = fabric.FirstFiber<Score>())
        score->value = 0;

    fabric.UnwindTow(prompt);
    fabric.SpawnPrefab("StartPrompt");
}

static void PipeGeneratorShuttle(Loom& loom, float dt)
{
    auto& fabric = loom.GetFabric();

    Timer* timer;
    if (!(timer = fabric.FirstFiber<Timer>()))
        return;

    timer->elapsed += dt;

    while (timer->elapsed >= PIPE_SPAWN_INTERVAL)
    {
        timer->elapsed -= PIPE_SPAWN_INTERVAL;
        auto pipeYOffset = RandomInt(-100, 300);

        Tow upperPipe = fabric.SpawnPrefab("UpperPipe");
        upperPipe.FetchFiber<Transform>().position.y += pipeYOffset;
        Tow lowerPipe = fabric.SpawnPrefab("LowerPipe");
        lowerPipe.FetchFiber<Transform>().position.y += pipeYOffset;
    }
}

static void PipeScrollerShuttle(Loom& loom, std::vector<Tow>& pipes, float dt)
{
    auto& fabric = loom.GetFabric();

    for (auto& pipe : pipes)
    {
        auto& transform = pipe.FetchFiber<Transform>();
        transform.position.x -= PIPE_SCROLL_SPEED * dt;

        if (transform.position.x < PIPE_DESPAWN_X)
            fabric.UnwindTow(pipe);
    }
}

static void PlayerControllerShuttle(Loom& loom, std::vector<Tow>& players, float dt)
{
    auto& fabric = loom.GetFabric();

    auto& inputState = loom.GetService<InputState>();

    for (auto& player : players)
    {
        auto& body = player.FetchFiber<PhysicsBody>();
        auto& transform = player.FetchFiber<Transform>();

        if (inputState.IsMouseButtonPressed(0))
        {
            if (auto audioPlayer = fabric.FirstFiber<AudioPlayer>())
                audioPlayer->Play(FLAP_SFX);

            ApplyJump(body, Vector::up() * BIRD_JUMP_FORCE);
        }

        const float targetAngle = std::clamp(-body.velocity.y * BIRD_TILT_SENSITIVITY, BIRD_MAX_UP_TILT, BIRD_MAX_DOWN_TILT);
        transform.rotation += (targetAngle - transform.rotation) * BIRD_TILT_SMOOTHING * dt;
    }

}

static void ScoreDetectionShuttle(Loom& loom, std::vector<Tow>& scorePoints, float dt) {
    auto& fabric = loom.GetFabric();

    for (auto& scorePoint : scorePoints) {
        auto& transform = scorePoint.FetchFiber<Transform>();
        if (transform.position.x < PLAYER_X) {
            // Update the visible label
            auto score = fabric.FirstFiber<Score>();
            if (score) 
                score->value++;

            if (auto audioPlayer = fabric.FirstFiber<AudioPlayer>())
                audioPlayer->Play(POINT_SFX);

            // Remove the tag so this pipe pair is never counted again
            scorePoint.SeverFiber<ScorePoint>();
        }
    }
}

static void ScoreTextUpdateShuttle(Loom& loom, std::vector<Tow>& scoreTexts, float dt)
{
    for (auto& scoreText : scoreTexts)
        scoreText.FetchFiber<Text>().text = SCORE_TEXT + std::to_string(scoreText.FetchFiber<Score>().value);
}

static void LoseConditionShuttle(Loom& loom, std::vector<Tow>& players, float dt)
{
    auto& fabric = loom.GetFabric();

    auto& collisionEvent = loom.GetService<CollisionEvent>();
    for (auto& collision : collisionEvent.collisionPairs)
    {
        if (collision.a.HoldsFiber<PlayerTag>() || collision.b.HoldsFiber<PlayerTag>())
        {
            if (auto audioPlayer = fabric.FirstFiber<AudioPlayer>())
                audioPlayer->Play(DEATH_SFX);

            ResetGame(loom);
            return;
        }
    }
}