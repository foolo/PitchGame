#ifndef MYGAME1_GAME_H
#define MYGAME1_GAME_H

#include <vector>
#include <cstdint>
#include "Model.h"

class Game {
public:
    Game();

    void update(float dt);

    Vector2 getPlayerPos() const { return playerPos_; }
    Vector2 getCameraPos() const { return cameraPos_; }
    float getPlayerRadius() const { return playerRadius_; }

    const std::vector<Vector2>& getStaticObjects() const { return staticObjects_; }
    const std::vector<Vector2>& getCloudObjects() const { return cloudObjects_; }
    const std::vector<Vector2>& getTreeObjects() const { return treeObjects_; }

private:
    void initWorld();

    Vector2 playerPos_;
    Vector2 cameraPos_;
    float playerRadius_;

    std::vector<Vector2> staticObjects_;
    std::vector<Vector2> cloudObjects_;
    std::vector<Vector2> treeObjects_;
};

#endif //MYGAME1_GAME_H
