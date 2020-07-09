#include <Ecs.hh>

#include <functional>
#include <iostream>

using std::cout;
using std::endl;

// Event
struct IncomingMissile {
    IncomingMissile(int x, int y) : x(x), y(y) {}
    int x, y;
};

// Event
struct Explosion {
    Explosion(int x, int y) : x(x), y(y) {}
    int x, y;
};

// Component
struct Character {
    Character(int x, int y, string name) : x(x), y(y), name(name) {}
    int x, y;
    string name;
};

// Callback for when Explosion events occur
class ExplosionHandler {
public:
    void operator()(ecs::Entity entity, const Explosion &explosion) {
        explosionsSeen += 1;

        if (entity.Has<Character>()) {
            Character character = entity.Get<Character>();

            if (character.x == explosion.x && character.y == explosion.y) {
                entity.Destroy();
            }
        }
    }
    int explosionsSeen = 0;
};

int main(int argc, char **argv)
{
    ecs::EntityManager em;

    // announce character deaths when they occur
    typedef ecs::EntityDestruction Destruction;
    em.Subscribe<Destruction>([](ecs::Entity e, const Destruction &d) {
        if (e.Has<Character>()) {
            cout << e.Get<Character>().name << " has died" << endl;
        }
    });

    ecs::Entity player = em.NewEntity();
    player.Set<Character>(1, 1, "John Cena");

    // player will be smart and moves out of the way of missiles
    auto intelligence = [](ecs::Entity e, const IncomingMissile &missile) {
        Character character = e.Get<Character>();
        if (character.x == missile.x && character.y == missile.y) {
            // we better move...
            cout << character.name << " has moved out of the way!" << endl;
            character.x += 10;
            e.Set<Character>(character);
        }
    };

    ecs::Subscription sub = player.Subscribe<IncomingMissile>(intelligence);

    // We can also use functors (be sure to pass by std::ref to maintain state)
    ExplosionHandler explosionHandler;
    em.Subscribe<Explosion>(std::ref(explosionHandler));

    Character playerChar = player.Get<Character>();
    cout << "Firing a missile at " << playerChar.name << endl;
    int missileX = playerChar.x;
    int missileY = playerChar.y;
    player.Emit(IncomingMissile(missileX, missileY));
    player.Emit(Explosion(missileX, missileY));

    cout << playerChar.name << " stops paying attention (Uh oh)" << endl;
    sub.Unsubscribe();

    playerChar = player.Get<Character>();
    cout << "Firing a missile at " << playerChar.name << endl;
    missileX = playerChar.x;
    missileY = playerChar.y;
    player.Emit(IncomingMissile(missileX, missileY));
    player.Emit(Explosion(missileX, missileY));

    cout << "The explosion handler saw " << explosionHandler.explosionsSeen
         << " explosions" << endl;

    return 0;
}
