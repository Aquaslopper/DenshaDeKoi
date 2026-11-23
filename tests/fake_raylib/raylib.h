#pragma once

// Minimal Raylib interface for exercising the inspector without a graphics context.
struct Vector2 { float x, y; };
struct Rectangle { float x, y, width, height; };
struct Color { unsigned char r, g, b, a; };
struct Texture2D { unsigned int id; int width, height, mipmaps, format; };
struct Image { void* data; int width, height, mipmaps, format; };
struct Font { int baseSize, glyphCount, glyphPadding; Texture2D texture; void* recs; void* glyphs; };
inline constexpr int PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 = 7;
Texture2D LoadTextureFromImage(Image image);
bool FileExists(const char* fileName);
Font LoadFontEx(const char* fileName, int fontSize, int* codepoints, int codepointCount);
Font GetFontDefault();
void UnloadFont(Font font);
void DrawTextEx(Font font, const char* text, Vector2 position, float fontSize, float spacing, Color tint);
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

// Additional interfaces used by the perspective table preview tests.
struct Vector3 { float x, y, z; };
struct Camera3D { Vector3 position, target, up; float fovy; int projection; };
struct RenderTexture2D { unsigned int id; Texture2D texture, depth; };
inline constexpr Color BLACK{0, 0, 0, 255};
inline constexpr int TEXTURE_FILTER_BILINEAR = 1;
inline constexpr int CAMERA_PERSPECTIVE = 0;
inline constexpr unsigned int FLAG_WINDOW_RESIZABLE = 4;
void SetConfigFlags(unsigned int flags);
void SetWindowMinSize(int width, int height);
int GetScreenWidth();
int GetScreenHeight();
RenderTexture2D LoadRenderTexture(int width, int height);
void UnloadRenderTexture(RenderTexture2D target);
void BeginTextureMode(RenderTexture2D target);
void EndTextureMode();
void BeginMode3D(Camera3D camera);
void EndMode3D();

void DrawRectangle(int x, int y, int width, int height, Color color);
