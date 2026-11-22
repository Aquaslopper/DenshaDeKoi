#include "CardPresentation.hpp"
#include "DeckRegister.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>

namespace {
std::atomic<int> allocations{0};
bool track = false;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Near(float a,float b,float tol=0.001f) { return std::abs(a-b) < tol; }
Vector2 Project(const Camera3D& c, Vector3 p) {
    Vector2 result{};
    Check(TableView::ProjectPoint(c,p,1360,768,result), "projection failed"); return result;
}
Vector3 World(float x,float y,float h=TableLayout::CardElevation) {
    auto p=TableLayout::OnPlane(x,y,h); return {p.x,p.y,p.z};
}
float Length(Vector3 a) { return std::sqrt(a.x*a.x+a.y*a.y+a.z*a.z); }
float Dot(Vector3 a,Vector3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vector3 Sub(Vector3 a,Vector3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
TableLayout::Point2 Plan(const Camera3D& c, Vector2 cursor) {
    Vector3 p{}; Check(TableView::IntersectPlane(TableView::ScreenRay(c,cursor,1360,768),TableLayout::CardElevation,p), "plane ray failed");
    return TableLayout::FromPlane({p.x,p.y,p.z});
}
void TestGeometryAndHover() {
    const Camera3D camera = TableView::DefaultOverview();
    for (int index = 0; index < 8; ++index) {
        TableCard card{index,TableLayout::PlayerHand[static_cast<std::size_t>(index)],1,TableLayout::CardElevation,TableCardZone::PLAYER_HAND};
        TableCardList cards; cards.push_back(card);
        CardPresentation a,b;
        const auto flat = a.Pose(card,camera,{});
        Check(Near(flat.center.y,TableLayout::CardElevation) && Near(flat.normal.y,1), "unhovered card not flat");
        const Vector2 cursor = Project(camera,CardPresentation::PointOnCard(flat,0.5f,0.5f));
        a.Update(cards,camera,cursor,0.15f);
        Check(a.Lift(card.id)>0.97f, "hover transition is too slow");
        for (int i=0;i<30;++i) a.Update(cards,camera,cursor,1.0f/30);
        for (int i=0;i<144;++i) b.Update(cards,camera,cursor,1.0f/144);
        Check(a.InspectedCard()==card.id && a.Lift(card.id)>0.999f, "stationary hover did not remain lifted");
        Check(Near(a.Lift(card.id),b.Lift(card.id),0.00001f), "hover easing depends on frame rate");
        const auto lifted = a.Pose(card,camera,cursor);
        Check(Near(lifted.center.y-flat.center.y,0.35f,0.001f), "wrong lift");
        for (float u : {0.f, 1.f}) for (float v : {0.f, 1.f}) {
            const auto edge = Project(camera,CardPresentation::PointOnCard(lifted,u,v));
            Check(edge.x>=0 && edge.x<=1360 && edge.y>=0 && edge.y<=768, "lifted hand card is clipped");
        }
        const auto toCamera = Sub(camera.position,lifted.center);
        Check(Dot(toCamera,lifted.normal)/Length(toCamera)>0.9999f, "lifted card not facing camera");
        const auto faceCursor = Project(camera,CardPresentation::PointOnCard(lifted,0.3f,0.4f));
        const auto hit = a.Pick(cards,camera,faceCursor);
        Check(hit && hit->card.id==card.id && Near(hit->u,0.3f) && Near(hit->v,0.4f), "tilted card pick incorrect");
        a.Update(cards,camera,{-100,-100},0.15f);
        Check(a.Lift(card.id)<0.03f && a.InspectedCard()<0, "card did not settle after hover");
    }
    Vector3 ignored{};
    Check(!TableView::IntersectPlane({{0,1,0},{1,0,0}},0,ignored),"parallel ray accepted");
    Check(!TableView::IntersectPlane({{0,1,0},{0,1,0}},0,ignored),"backwards ray accepted");
    Check(!TableView::IntersectPlane({{0,1,0},{0,std::numeric_limits<float>::quiet_NaN(),0}},0,ignored),"NaN accepted");
}
void TestAllVisibleZones() {
    const auto camera=TableView::DefaultOverview();
    for (auto zone : {TableCardZone::PLAYER_HAND,TableCardZone::OTHER}) for (int id=0;id<48;++id) {
        TableCard card{id,TableLayout::Field[0],1,TableLayout::CardElevation,zone};
        TableCardList cards; cards.push_back(card);
        CardPresentation p;
        const auto cursor=Project(camera,CardPresentation::PointOnCard(p.Pose(card,camera,{}),0.5f,0.5f));
        p.Update(cards,camera,cursor,0.15f);
        Check(p.InspectedCard()==id,"visible card not inspectable");
        Check(zone==TableCardZone::PLAYER_HAND ? p.Lift(id)>0.97f : p.Lift(id)==0,"wrong card zone lifted");
    }
    TableCardList hidden; hidden.push_back({-1,TableLayout::Field[0]});
    CardPresentation p; auto c=p.Pose(hidden[0],camera,{});
    p.Update(hidden,camera,Project(camera,c.center),0.15f);
    Check(p.InspectedCard()==-1,"hidden card identity leaked");
}
void TestDragAndMatch() {
    const auto camera=TableView::DefaultOverview();
    int checked = 0;
    for (unsigned seed=1;seed<1000 && checked<40;++seed) {
        TableGame game(seed);
        if (game.Rules().GetPhase()!=Phase::AWAIT_HAND_PLAY || game.Rules().CurrentPlayer()!=Player::PLAYER) continue;
        const auto& hand=game.Rules().Hand(Player::PLAYER);
        const auto& field=game.Rules().Field();
        std::size_t index=seed%hand.size();
        auto target=std::find_if(field.begin(),field.end(),[&](int f){return f/4==hand[index]/4;});
        if (target==field.end()) continue;
        const int id=hand[index];
        const auto r=TableLayout::PlayerHand[index];
        CardPresentation p;
        auto cards=game.CardVisuals();
        const float ux = 0.2f + 0.3f*static_cast<float>(seed%3);
        const float vy = 0.2f + 0.3f*static_cast<float>((seed/3)%3);
        Vector2 mouse=Project(camera,World(r.x+r.width*ux,r.y+r.height*vy));
        for (int i=0;i<25;++i) p.Update(cards,camera,mouse,1.f/60);
        auto picked=p.Pick(cards,camera,mouse); Check(picked && picked->card.id==id,"cannot pick lifted hand");
        const float u=picked->u,v=picked->v;
        game.PointerDown({r.x+r.width*u,r.y+r.height*v}); Check(game.Dragging(),"drag not started"); p.BeginDrag(*picked);
        const auto dest=game.FieldRect(*target);
        const auto start=mouse;
        const auto end=Project(camera,World(dest.x+dest.width*.5f,dest.y+dest.height*.5f));
        for(int i=0;i<=60;++i) {
            const float t=i/60.0f; mouse={start.x+(end.x-start.x)*t,start.y+(end.y-start.y)*t};
            cards=game.CardVisuals(); p.Update(cards,camera,mouse,1.f/60);
            game.PointerMove(Plan(camera,mouse)); cards=game.CardVisuals();
            const auto& dragged=cards.back(); Check(dragged.zone==TableCardZone::DRAGGED,"dragged visual missing");
            const auto pose=p.Pose(dragged,camera,mouse);
            const auto screen=Project(camera,CardPresentation::PointOnCard(pose,u,v));
            Check(Near(screen.x,mouse.x,0.01f)&&Near(screen.y,mouse.y,0.01f),"dragged point slipped under cursor");
            const auto ray=TableView::ScreenRay(camera,mouse,1360,768);
            Check(-Dot(ray.direction,pose.normal)/Length(ray.direction)>0.999f,"dragged card not facing cursor ray");
        }
        auto visual=p.DragVisualRect(game.CardVisuals(),camera,mouse);
        Check(visual.has_value(),"missing visual drop position");
        game.PointerUp(Plan(camera,mouse),visual); p.EndDrag();
        Check(game.Busy() && !game.Dragging(),"matched drop did not begin");
        const auto moving=game.CardVisuals().back();
        Check(Near(moving.rect.x,visual->x,0.01f)&&Near(moving.rect.y,visual->y,0.01f),"release animation jumped away");
        ++checked;
    }
    Check(checked==40, "insufficient player drag fixtures");
}
void TestAllocationAndViewport() {
    const auto camera=TableView::DefaultOverview(); CardPresentation p;
    TableCardList cards; cards.push_back({0,TableLayout::PlayerHand[0],1,TableLayout::CardElevation,TableCardZone::PLAYER_HAND});
    const auto cursor=Project(camera,World(93,638));
    allocations=0; track=true;
    for(int i=0;i<500;++i) {p.Update(cards,camera,cursor,1.f/60); (void)p.Pick(cards,camera,cursor); (void)p.Pose(cards[0],camera,cursor);}
    track=false; Check(allocations==0,"presentation allocated per frame");
    auto vp=TableView::CanvasViewport(800,1000); Vector2 pnt{};
    Check(TableView::WindowToCanvas({vp.x+vp.width,vp.y+vp.height},vp,pnt)&&Near(pnt.x,1360,0.01f)&&Near(pnt.y,768,0.01f),"viewport mapping failed");
}
}
void* operator new(std::size_t n) {if(track)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {if(track)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept {std::free(p);} void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);} void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
int main(){try{TestGeometryAndHover();TestAllVisibleZones();TestDragAndMatch();TestAllocationAndViewport();std::cout<<"Card presentation passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
