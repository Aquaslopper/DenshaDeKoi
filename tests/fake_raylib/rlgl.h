#pragma once
inline constexpr int RL_QUADS = 7;
unsigned int rlGetTextureIdDefault();
void rlSetTexture(unsigned int id);
void rlBegin(int mode);
void rlEnd();
void rlColor4ub(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
void rlTexCoord2f(float x, float y);
void rlVertex3f(float x, float y, float z);
