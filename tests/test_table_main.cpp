#include "TableGame.hpp"
#include "TableView.hpp"
#include "CardInspection.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

int TableMainForTest();
namespace {
struct Vertex { Vector3 p; float u,v; };
struct DrawnCard { std::array<Vertex,4> v; unsigned texture; };
std::vector<DrawnCard> drawn;
std::vector<std::string> texts;
std::array<Vertex,4> vertices{};
int vertexCount=0, frame=0, mode=0, ticks=0, targetMonth=-1;
float texU=0,texV=0; unsigned texture=0; Camera3D camera{};
Vector2 mouse{-1,-1}, start{}, target{};
bool press=false, release=false, succeeded=false, sawLift=false, sawNativePreview=false;
int loads=0;
void Check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int Id(const DrawnCard& c) {return static_cast<int>(std::floor(c.v[0].u*12+.001f))*4+static_cast<int>(std::floor(c.v[0].v*4+.001f));}
Vector3 Center(const DrawnCard& c) {
    Vector3 result{}; for(auto v:c.v){result.x+=v.p.x/4;result.y+=v.p.y/4;result.z+=v.p.z/4;} return result;
}
Vector2 Screen(Vector3 p){Vector2 v{};Check(TableView::ProjectPoint(camera,p,1360,768,v),"failed to project input fixture");return v;}
bool HasText(const char* s){for(const auto& t:texts)if(t==s)return true;return false;}
void NextFrame() {
    press=release=false;
    if (mode==0 && HasText("Your turn")) {
        for(const auto& hand:drawn){
            if(hand.texture!=2 || TableLayout::FromPlane({Center(hand).x,0,Center(hand).z}).y<560)continue;
            for(const auto& field:drawn){
                if(field.texture!=2)continue;
                auto center=Center(field);auto plan=TableLayout::FromPlane({center.x,0,center.z});
                if(plan.y<160 || plan.y>550 || Id(hand)/4!=Id(field)/4)continue;
                start=Screen(Center(hand)); target=Screen(center); targetMonth=Id(hand)/4;
                mouse=start;mode=1;ticks=0;break;
            }
            if(mode==1)break;
        }
    } else if(mode==1) {
        ++ticks;
        for(const auto& card:drawn) if(card.texture==2 && Id(card)/4==targetMonth && Center(card).y>0.30f) sawLift=true;
        if(ticks>=20){Check(sawLift,"real loop did not lift hovered card");press=true;mode=2;ticks=0;}
    } else if(mode==2) {
        ++ticks; const float t=std::min(1.0f,ticks/30.0f);
        mouse={start.x+(target.x-start.x)*t,start.y+(target.y-start.y)*t};
        if(ticks>=30){release=true;mode=3;ticks=0;}
    } else if(mode==3) {
        ++ticks;
        if(HasText("CAPTURES  YOU / CPU")) {
            mouse={CardInspection::GridX+6,CardInspection::GridY+10}; mode=4; ticks=0;
        }
        if(ticks>240)throw std::runtime_error("real loop did not capture the matched card");
    } else if(mode==4) {
        Check(HasText("TRAIN TELEMETRY") && sawNativePreview, "captured-card browser did not show native inspection");
        succeeded=true;
    }
    drawn.clear();texts.clear();sawNativePreview=false;++frame;
}
}
void SetConfigFlags(unsigned int){} void InitWindow(int,int,const char*){} bool IsWindowReady(){return true;}
void CloseWindow(){} void SetWindowMinSize(int,int){} void SetTargetFPS(int){} bool WindowShouldClose(){return succeeded||frame>1500;}
float GetFrameTime(){return 1.f/60;} Vector2 GetMousePosition(){return mouse;}
bool IsMouseButtonPressed(int b){return b==MOUSE_BUTTON_LEFT && press;}
bool IsMouseButtonReleased(int b){return b==MOUSE_BUTTON_LEFT && release;}
const char* GetApplicationDirectory(){return ".";} int GetScreenWidth(){return 1360;} int GetScreenHeight(){return 768;}
Texture2D LoadTexture(const char*){return ++loads==1?Texture2D{1,1360,768,1,0}:Texture2D{2,2148,1056,1,0};}
void UnloadTexture(Texture2D){} void SetTextureFilter(Texture2D,int){}
RenderTexture2D LoadRenderTexture(int w,int h){return {3,{4,w,h,1,0},{}};} void UnloadRenderTexture(RenderTexture2D){}
void BeginTextureMode(RenderTexture2D){} void EndTextureMode(){}
void BeginMode3D(Camera3D c){
    const auto expected = TableView::DefaultOverview();
    Check(c.position.x==expected.position.x && c.position.y==expected.position.y && c.position.z==expected.position.z &&
          c.target.x==expected.target.x && c.target.y==expected.target.y && c.target.z==expected.target.z &&
          c.up.x==expected.up.x && c.up.y==expected.up.y && c.up.z==expected.up.z &&
          c.fovy==expected.fovy && c.projection==expected.projection, "world camera changed during the match");
    camera=c;
}
void EndMode3D(){}
void ClearBackground(Color){} void BeginDrawing(){} void EndDrawing(){NextFrame();}
void DrawRectangle(int,int,int,int,Color){} void DrawText(const char* text,int,int,int,Color){texts.emplace_back(text);}
void DrawTexturePro(Texture2D t,Rectangle src,Rectangle dst,Vector2,float,Color){
    if(t.id==2 && std::abs(dst.width-179)<.01f && std::abs(dst.height-264)<.01f &&
       static_cast<int>(src.x/179)==targetMonth) sawNativePreview=true;
}
unsigned int rlGetTextureIdDefault(){return 99;} void rlSetTexture(unsigned int id){texture=id;} void rlBegin(int){vertexCount=0;}
void rlEnd(){if(vertexCount==4)drawn.push_back({vertices,texture});} void rlColor4ub(unsigned char,unsigned char,unsigned char,unsigned char){}
void rlTexCoord2f(float u,float v){texU=u;texV=v;} void rlVertex3f(float x,float y,float z){if(vertexCount<4)vertices[vertexCount++]={{x,y,z},texU,texV};}
void rlVertex2f(float,float){throw std::runtime_error("unexpected backdrop warp");}
int main(){try{Check(TableMainForTest()==0,"game loop failed");Check(succeeded,"script could not find a matching playable hand");std::cout<<"Scripted main loop hover, drag, and capture passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
