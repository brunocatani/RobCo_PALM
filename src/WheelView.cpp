#include "WheelView.h"
#include <imgui.h>
#include <cstdio>

namespace wheel {
namespace {
ImU32 color(Category category, int alpha=255) {
 switch(category) {
 case Category::Aid:return IM_COL32(104,226,193,alpha);
 case Category::Food:return IM_COL32(244,197,114,alpha);
 case Category::Grenades:return IM_COL32(249,137,111,alpha);
 }
 return 0;
}
void centered(ImDrawList* draw, ImVec2 position, float size, ImU32 tint, const char* text) {
 auto* font=ImGui::GetFont();
 const auto extent=font->CalcTextSizeA(size,10000,0,text);
 draw->AddText(font,size,{position.x-extent.x/2,position.y-extent.y/2},tint,text);
}
void icon(ImDrawList* draw, ImVec2 p, float r, Category category, ImU32 tint) {
 if(category==Category::Aid) {
  draw->AddRectFilled({p.x-r*.26f,p.y-r},{p.x+r*.26f,p.y+r},tint,r*.09f);
  draw->AddRectFilled({p.x-r,p.y-r*.26f},{p.x+r,p.y+r*.26f},tint,r*.09f);
 } else if(category==Category::Food) {
  draw->AddEllipseFilled({p.x,p.y-r*.15f},{r*.65f,r},tint,-.55f,24);
  draw->AddLine({p.x-r*.65f,p.y+r},{p.x+r*.5f,p.y-r*.9f},IM_COL32(19,33,33,255),r*.15f);
 } else {
  draw->AddRectFilled({p.x-r*.7f,p.y-r*.55f},{p.x+r*.7f,p.y+r},tint,r*.45f);
  draw->AddRectFilled({p.x-r*.28f,p.y-r*.95f},{p.x+r*.28f,p.y-r*.5f},tint,r*.1f);
  draw->AddLine({p.x,p.y-r*.95f},{p.x+r*.9f,p.y-r*.85f},tint,r*.18f);
  draw->AddLine({p.x+r*.9f,p.y-r*.85f},{p.x+r*.9f,p.y+r*.45f},tint,r*.14f);
  for(int i=0;i<2;++i) draw->AddLine({p.x-r*.55f,p.y+r*(.02f+.4f*i)},{p.x+r*.55f,p.y+r*(.02f+.4f*i)},IM_COL32(19,33,33,190),r*.09f);
 }
}
void sector(ImDrawList* draw, ImVec2 center, float inner, float outer, float a, float b, ImU32 tint) {
 constexpr int pieces=20;
 for(int k=0;k<pieces;++k) {
  const float start=a+(b-a)*k/pieces, end=a+(b-a)*(k+1)/pieces;
  auto point=[&](float angle,float radius){return ImVec2{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};};
  draw->AddQuadFilled(point(start,inner),point(start,outer),point(end,outer),point(end,inner),tint);
 }
}
}

Action drawWheel(Model& model, View& view) {
 auto& io=ImGui::GetIO(); Action action;
 const float scale=std::min(io.DisplaySize.x,io.DisplaySize.y)/1024.f;
 const ImVec2 origin{(io.DisplaySize.x-1024*scale)/2,(io.DisplaySize.y-1024*scale)/2};
 auto point=[&](float x,float y){return ImVec2{origin.x+x*scale,origin.y+y*scale};};
 ImGui::SetNextWindowPos({0,0}); ImGui::SetNextWindowSize(io.DisplaySize);
 ImGui::Begin("Wheel",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
  ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
 auto* draw=ImGui::GetWindowDrawList();
 view.animation=std::min(1.f,view.animation+io.DeltaTime*7);
 const int alpha=static_cast<int>(255*view.animation);
 const auto accent=color(model.category,alpha);
 const auto white=IM_COL32(233,242,239,alpha);
 const auto muted=IM_COL32(146,166,164,alpha);
 const ImVec2 center=point(512,510);
 const float inner=180*scale, outer=(326+10*view.animation)*scale;
 const float mx=io.MousePos.x-center.x, my=io.MousePos.y-center.y;
 const auto category=static_cast<unsigned>(model.category);
 auto& items=model.items[category];
 model.page=std::min(model.page,pageCount(items.size())-1);
 const int hit=hitSlot(mx,my,inner,outer);
 const std::size_t index=hit>=0 ? model.page*kSlots+hit : items.size();
 const Item* hovered=index<items.size()? &items[index]:nullptr;

 draw->AddCircleFilled(center,outer+12*scale,IM_COL32(10,19,24,alpha*3/4),128);
 draw->AddCircle(center,outer+13*scale,IM_COL32(126,164,165,alpha/4),128,1.2f*scale);
 for(std::size_t slot=0;slot<kSlots;++slot) {
  const float mid=-kPi/2+static_cast<float>(slot)*kPi/4;
  const float start=mid-kPi/8+.021f, end=mid+kPi/8-.021f;
  const auto itemIndex=model.page*kSlots+slot;
  const bool available=itemIndex<items.size();
  const bool active=available && hit==static_cast<int>(slot);
  sector(draw,center,inner,outer,start,end,active?color(model.category,alpha*3/4):IM_COL32(25,40,45,available?alpha*9/10:alpha/3));
  if(active) sector(draw,center,outer-5*scale,outer,start,end,accent);
  const ImVec2 slotCenter{center.x+std::cos(mid)*258*scale,center.y+std::sin(mid)*258*scale};
  if(!available) { centered(draw,slotCenter,20*scale,IM_COL32(91,115,116,alpha/2),"—"); continue; }
  const auto& item=items[itemIndex];
  const auto ink=active?IM_COL32(12,28,29,alpha):accent;
  icon(draw,{slotCenter.x,slotCenter.y-30*scale},18*scale,model.category,ink);
  char count[32]; std::snprintf(count,sizeof(count),"%u",item.count);
  centered(draw,{slotCenter.x,slotCenter.y+4*scale},22*scale,active?IM_COL32(10,27,28,alpha):white,count);
  const float labelSize=16*scale, labelWidth=140*scale;
  const auto labelExtent=ImGui::GetFont()->CalcTextSizeA(labelSize,labelWidth,labelWidth,item.name.c_str());
  draw->PushClipRect({slotCenter.x-labelWidth/2,slotCenter.y+22*scale},{slotCenter.x+labelWidth/2,slotCenter.y+62*scale},true);
  draw->AddText(ImGui::GetFont(),labelSize,{slotCenter.x-labelExtent.x/2,slotCenter.y+23*scale},
   active?IM_COL32(10,27,28,alpha):white,item.name.c_str(),nullptr,labelWidth);
  draw->PopClipRect();
  if(item.equipped) draw->AddCircleFilled({slotCenter.x+33*scale,slotCenter.y-30*scale},4*scale,white);
 }
 draw->AddCircleFilled(center,inner-7*scale,IM_COL32(13,25,29,alpha*9/10),96);
 draw->AddCircle(center,inner-7*scale,IM_COL32(140,174,172,alpha/5),96,scale);
 icon(draw,point(512,437),22*scale,model.category,accent);
 centered(draw,point(512,482),18*scale,muted,hovered?(model.category==Category::Grenades?"EQUIP THROWABLE":"USE ONE ITEM"):"QUICK ACCESS");
 const char* title=hovered?hovered->name.c_str():items.empty()?"Nothing carried":categoryName(model.category);
 const float titleSize=hovered?29*scale:37*scale;
 const float wrapWidth=280*scale;
 const auto textSize=ImGui::GetFont()->CalcTextSizeA(titleSize,wrapWidth,wrapWidth,title);
 draw->PushClipRect(point(360,501),point(664,595),true);
 draw->AddText(ImGui::GetFont(),titleSize,{center.x-textSize.x/2,origin.y+507*scale},white,title,nullptr,wrapWidth);
 draw->PopClipRect();
 centered(draw,point(512,611),17*scale,muted,hovered?(hovered->equipped?"CURRENTLY EQUIPPED":"CLICK TO SELECT"):"CLICK CENTER TO CLOSE");

 centered(draw,point(512,75),20*scale,muted,"R O C K   /   F I E L D   K I T");
 for(unsigned cat=0;cat<3;++cat) {
  const float x=282.f+cat*230;
  const ImVec2 a=point(x-100,113), b=point(x+100,162);
  const bool over=io.MousePos.x>=a.x && io.MousePos.x<=b.x && io.MousePos.y>=a.y && io.MousePos.y<=b.y;
  const auto tint=color(static_cast<Category>(cat),alpha);
  draw->AddRectFilled(a,b,cat==category?IM_COL32(30,52,55,alpha):IM_COL32(13,25,29,alpha*3/4),20*scale);
  if(cat==category || over) draw->AddRect(a,b,tint,20*scale,0,scale);
  centered(draw,point(x,137),22*scale,cat==category?tint:muted,categoryName(static_cast<Category>(cat)));
  if(over && ImGui::IsMouseClicked(0)) {model.category=static_cast<Category>(cat);model.page=0;view.click={};}
 }
 char pagination[64]; std::snprintf(pagination,sizeof(pagination),"%zu ITEMS   /   %zu OF %zu",items.size(),model.page+1,pageCount(items.size()));
 centered(draw,point(512,900),19*scale,muted,pagination);
 for(int direction:{-1,1}) {
  const auto pos=point(512+direction*205,900);
  const bool over=std::hypot(io.MousePos.x-pos.x,io.MousePos.y-pos.y)<24*scale;
  draw->AddCircleFilled(pos,24*scale,over?IM_COL32(48,71,72,alpha):IM_COL32(22,37,42,alpha),32);
  centered(draw,pos,23*scale,white,direction<0?"‹":"›");
  if(over && ImGui::IsMouseClicked(0)) {
   if(direction<0 && model.page>0)--model.page;
   if(direction>0 && model.page+1<pageCount(items.size()))++model.page;
   view.click={};
  }
 }
 if(std::hypot(mx,my)<=outer && io.MouseWheel!=0) {
  view.scroll+=io.MouseWheel;
  if(std::fabs(view.scroll)>=.3f) {
   if(view.scroll>0 && model.page>0)--model.page;
   if(view.scroll<0 && model.page+1<pageCount(items.size()))++model.page;
   view.scroll=0; view.click={};
  }
 }
 centered(draw,point(512,960),14*scale,muted,model.status.c_str());
 action.useItem=view.click.update(ImGui::IsMouseDown(0),ImGui::IsMouseClicked(0),ImGui::IsMouseReleased(0),hovered?hovered->id:0);
 action.close=std::hypot(mx,my)<inner-7*scale && ImGui::IsMouseClicked(0);
 ImGui::End();
 return action;
}

Model demoInventory() {
 Model model; std::uint32_t id=1;
 for(const char* name:{"Stimpak","RadAway","Rad-X","Med-X","Psycho","Buffout","Mentats","Jet","Antibiotics","Refreshing Beverage"})
  model.items[0].push_back({id++,name,8+id*2,false});
 for(const char* name:{"Nuka-Cola","Purified Water","Grilled Radstag","Dandy Boy Apples","Fancy Lads Snack Cakes","Iguana on a Stick"})
  model.items[1].push_back({id++,name,3+id,false});
 for(const char* name:{"Fragmentation Grenade","Molotov Cocktail","Plasma Grenade","Pulse Grenade","Cryogenic Grenade","Nuka Grenade"})
  model.items[2].push_back({id++,name,2+id%7,id==18});
 model.status="DESKTOP PREVIEW  /  SAMPLE INVENTORY";
 return model;
}
}
