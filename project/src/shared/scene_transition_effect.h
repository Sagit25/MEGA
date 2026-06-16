#ifndef SCENE_TRANSITION_EFFECT_H
#define SCENE_TRANSITION_EFFECT_H

namespace scene_fade {

void init();
bool active();
void start(int sceneIdx, float now);
int update(float now);
float alpha(float now);
void draw(float alpha);

} // namespace scene_fade

#endif
