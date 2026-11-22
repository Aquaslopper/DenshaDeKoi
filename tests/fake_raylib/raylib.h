#pragma once

// Minimal Raylib interface for exercising the inspector without a graphics context.
struct Vector2 { float x, y; };
struct Rectangle { float x, y, width, height; };
struct Color { unsigned char r, g, b, a; };
struct Texture2D { unsigned int id; int width, height, mipmaps, format; };
inline constexpr Color WHITE{255, 255, 255, 255};
inline constexpr Color LIGHTGRAY{200, 200, 200, 255};
inline constexpr int TEXTURE_FILTER_POINT = 0;

void InitWindow(int width, int height, const char* title);
bool IsWindowReady();
void CloseWindow();
void SetTargetFPS(int fps);
bool WindowShouldClose();
Vector2 GetMousePosition();
void BeginDrawing();
void EndDrawing();
void ClearBackground(Color color);
bool CheckCollisionPointRec(Vector2 point, Rectangle rect);
void DrawRectangleLinesEx(Rectangle rect, float thickness, Color color);
void DrawText(const char* text, int x, int y, int fontSize, Color color);
const char* TextFormat(const char* format, ...);
const char* GetApplicationDirectory();
Texture2D LoadTexture(const char* fileName);
void UnloadTexture(Texture2D texture);
void SetTextureFilter(Texture2D texture, int filter);
void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest, Vector2 origin,
                    float rotation, Color tint);
