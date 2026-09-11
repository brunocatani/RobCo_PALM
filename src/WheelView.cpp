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
 case Category::Weapons:return IM_COL32(122,191,255,alpha);
 case Category::Armor:return IM_COL32(192,165,255,alpha);
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
 } else if(category==Category::Weapons) {
  draw->AddRectFilled({p.x-r,p.y-r*.5f},{p.x+r,p.y+r*.05f},tint,r*.1f);
  draw->AddQuadFilled({p.x-r*.55f,p.y},{p.x,p.y},{p.x-r*.2f,p.y+r},{p.x-r*.75f,p.y+r},tint);
 } else if(category==Category::Armor) {
  draw->AddQuadFilled({p.x-r*.7f,p.y-r},{p.x+r*.7f,p.y-r},{p.x+r*.55f,p.y+r*.4f},{p.x-r*.55f,p.y+r*.4f},tint);
  draw->AddTriangleFilled({p.x-r*.55f,p.y+r*.4f},{p.x+r*.55f,p.y+r*.4f},{p.x,p.y+r},tint);
  draw->AddLine({p.x,p.y-r*.75f},{p.x,p.y+r*.35f},IM_COL32(18,27,38,220),r*.15f);
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
void gestureIcon(ImDrawList* draw,ImVec2 p,float r,const GestureDefinition& gesture,ImU32 tint,bool left) {
 const float mirror=left?-1.f:1.f;
 draw->AddRectFilled({p.x-r*.55f,p.y},{p.x+r*.55f,p.y+r*.65f},tint,r*.18f);
 for(unsigned finger=1;finger<5;++finger) {
  const float x=p.x+mirror*(-.45f+(finger-1)*.3f)*r;
  const float length=gesture.flex[finger]>.5f?1.f:.2f;
  draw->AddLine({x,p.y+r*.15f},{x,p.y-r*length},tint,r*.22f);
 }
 draw->AddLine({p.x-mirror*r*.45f,p.y+r*.35f},
  {p.x-mirror*r*(gesture.flex[0]>.5f?1.15f:.6f),p.y-r*(gesture.flex[0]>.5f?.35f:-.05f)},tint,r*.25f);
}
}

Action drawWheel(Model& model, View& view, ImVec2 position, ImVec2 size) {
 auto& io=ImGui::GetIO(); Action action;
 if(size.x<=0 || size.y<=0)size=io.DisplaySize;
 const float scale=std::min(size.x,size.y)/1024.f;
 const ImVec2 origin{position.x+(size.x-1024*scale)/2,position.y+(size.y-1024*scale)/2};
 auto point=[&](float x,float y){return ImVec2{origin.x+x*scale,origin.y+y*scale};};
 ImGui::SetNextWindowPos(position); ImGui::SetNextWindowSize(size);
 ImGui::Begin("Wheel",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoBackground|
  ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollbar);
 auto* draw=ImGui::GetWindowDrawList();
 view.animation=std::min(1.f,view.animation+io.DeltaTime*7);
 const int alpha=static_cast<int>(255*view.animation);
 const auto white=IM_COL32(233,242,239,alpha);
 const auto muted=IM_COL32(146,166,164,alpha);
 const ImVec2 center=point(512,510);
 const float inner=180*scale, outer=(326+10*view.animation)*scale;
 const float mx=io.MousePos.x-center.x, my=io.MousePos.y-center.y;
 const int navigation=hitCenter(mx,my,scale);
 if(navigation>=0 && navigation<static_cast<int>(kCategoryCount) && model.enabled[navigation]) {
  model.category=static_cast<Category>(navigation);model.gestures.showing=false;
 }
 if(navigation==static_cast<int>(kGesturesNavigation))model.gestures.showing=true;
 if(!model.enabled[static_cast<unsigned>(model.category)])
  for(unsigned c=0;c<kCategoryCount;++c)if(model.enabled[c]){model.category=static_cast<Category>(c);break;}
 const auto category=static_cast<unsigned>(model.category);
 auto& gestures=model.gestures;const bool showingGestures=gestures.showing;
 const auto gestureColor=IM_COL32(247,181,218,alpha);
 const auto accent=showingGestures?gestureColor:color(model.category,alpha);
 auto& items=model.items[category];
 const int hit=hitSlot(mx,my,inner,outer);
 const std::size_t index=hit>=0 ? hit : items.size();
 const Item* hovered=!showingGestures && index<items.size()? &items[index]:nullptr;
 if(showingGestures) {
  centered(draw,point(512,72),26*scale,gestureColor,"GESTURES");
  for(unsigned hand=0;hand<2;++hand) {
   const bool left=hand==1;const float x=left?400.f:624.f;
   const auto a=point(x-101,99),b=point(x+101,147);
   if(io.MousePos.x>=a.x && io.MousePos.x<=b.x && io.MousePos.y>=a.y && io.MousePos.y<=b.y)gestures.left=left;
   const bool selected=gestures.left==left;
   draw->AddRectFilled(a,b,selected?IM_COL32(110,58,88,alpha):IM_COL32(25,40,45,alpha),8*scale);
   centered(draw,point(x,123),18*scale,selected?white:muted,left?"LEFT HAND":"RIGHT HAND");
  }
 }
 const unsigned hand=gestures.left?1:0;

 draw->AddCircleFilled(center,outer+12*scale,IM_COL32(10,19,24,alpha*3/4),128);
 draw->AddCircle(center,outer+13*scale,IM_COL32(126,164,165,alpha/4),128,1.2f*scale);
 for(std::size_t slot=0;slot<kSlots;++slot) {
  const float mid=-kPi/2+static_cast<float>(slot)*kPi/4;
  const float start=mid-kPi/8+.021f, end=mid+kPi/8-.021f;
  const auto itemIndex=slot;
  if(showingGestures) {
   const auto& definition=kGestures[slot];const bool over=hit==static_cast<int>(slot);
   const bool selected=gestures.active[hand]==gestureChoice(static_cast<unsigned>(slot),gestures.left);
   sector(draw,center,inner,outer,start,end,over?IM_COL32(110,58,88,alpha):IM_COL32(35,31,44,alpha*9/10));
   if(over || selected)sector(draw,center,outer-5*scale,outer,start,end,gestureColor);
   const ImVec2 p{center.x+std::cos(mid)*258*scale,center.y+std::sin(mid)*258*scale};
   gestureIcon(draw,{p.x,p.y-21*scale},22*scale,definition,over?white:gestureColor,gestures.left);
   centered(draw,{p.x,p.y+27*scale},16*scale,white,definition.name);
   if(selected)centered(draw,{p.x,p.y+49*scale},12*scale,gestureColor,"ACTIVE");
   continue;
  }
  const bool available=itemIndex<items.size();
  const bool active=available && items[itemIndex].count>0 && hit==static_cast<int>(slot);
  sector(draw,center,inner,outer,start,end,active?color(model.category,alpha*3/4):IM_COL32(25,40,45,available?alpha*9/10:alpha/3));
  if(active) sector(draw,center,outer-5*scale,outer,start,end,accent);
  const ImVec2 slotCenter{center.x+std::cos(mid)*258*scale,center.y+std::sin(mid)*258*scale};
  if(!available) { centered(draw,slotCenter,20*scale,IM_COL32(91,115,116,alpha/2),"—"); continue; }
  const auto& item=items[itemIndex];
  const auto ink=active?IM_COL32(12,28,29,alpha):accent;
  icon(draw,{slotCenter.x,slotCenter.y-30*scale},18*scale,model.category,ink);
  char count[32]; if(isEquipment(model.category))std::snprintf(count,sizeof(count),"%s",item.equipped?"EQUIPPED":"EQUIP");else std::snprintf(count,sizeof(count),"%u",item.count);
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
 for(unsigned c=0;c<kNavigationCount;++c) {
  if(c<kCategoryCount && !model.enabled[c])continue;
  const float mid=-kPi/2+c*(2*kPi/kNavigationCount);
  const bool over=navigation==static_cast<int>(c);
  const auto tint=c<kCategoryCount?color(static_cast<Category>(c),alpha):c==kGesturesNavigation?gestureColor:IM_COL32(104,226,193,alpha);
  const bool selected=showingGestures?c==kGesturesNavigation:c==category && model.enabled[c];
  sector(draw,center,70*scale,(inner-8*scale),mid-kPi/kNavigationCount+.035f,mid+kPi/kNavigationCount-.035f,
   over?IM_COL32(42,70,70,alpha):IM_COL32(17,31,35,alpha*9/10));
  if(selected)
   sector(draw,center,inner-12*scale,inner-8*scale,mid-kPi/kNavigationCount+.06f,mid+kPi/kNavigationCount-.06f,tint);
  centered(draw,{center.x+std::cos(mid)*119*scale,center.y+std::sin(mid)*119*scale},
   16*scale,over || selected?tint:muted,c<kCategoryCount?categoryName(static_cast<Category>(c)):c==kGesturesNavigation?"GESTURES":"CONFIG");
 }
 draw->AddCircleFilled(center,54*scale,navigation==static_cast<int>(kCancelNavigation)?IM_COL32(37,58,61,alpha):IM_COL32(12,24,28,alpha),48);
 centered(draw,center,13*scale,muted,"CANCEL");
 action.hoveredItem=hovered && hovered->count>0?selectionToken(*hovered):0;
 action.configHovered=navigation==static_cast<int>(kConfigNavigation);
 if(showingGestures) {
  if(hit>=0) {
   action.hoveredGesture=gestureChoice(static_cast<unsigned>(hit),gestures.left);
   centered(draw,point(512,886),23*scale,white,kGestures[hit].name);
  } else centered(draw,point(512,886),20*scale,white,"Hover a hand above, then choose a gesture");
  const bool clearing=action.hoveredGesture && gestures.active[hand]==action.hoveredGesture;
  const bool free=gestures.availability[hand]==GestureAvailability::Free;
  centered(draw,point(512,920),16*scale,free || clearing?muted:IM_COL32(255,170,110,alpha),
   clearing?"RELEASE B TO CLEAR GESTURE":!free?gestureAvailabilityText(gestures.availability[hand]):
   gestures.left?"RELEASE B TO POSE LEFT HAND":"RELEASE B TO POSE RIGHT HAND");
 }else if(hovered) {
  centered(draw,point(512,886),23*scale,white,hovered->name.c_str());
  centered(draw,point(512,920),16*scale,muted,!hovered->count?"NOT CURRENTLY CARRIED":isEquipment(model.category)?(hovered->equipped?"RELEASE B TO UNEQUIP":"RELEASE B TO EQUIP"):"RELEASE B TO TAKE ONE");
 }else if(items.empty())centered(draw,point(512,886),20*scale,muted,"Choose your items in Config");
 centered(draw,point(512,960),14*scale,muted,model.status.c_str());
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
 for(const char* name:{"10mm Pistol","Suppressed Combat Rifle","Laser Rifle","Scoped Hunting Rifle"})
  model.items[3].push_back({id++,name,1,id==24});
 for(const char* name:{"Leather Chest Piece","Combat Armor Left Arm","Combat Armor Right Arm","Vault Suit"})
  model.items[4].push_back({id++,name,1,id==28});
 for(auto& category:model.items)for(auto& item:category)item.key="preview|"+std::to_string(item.id);
 model.status="DESKTOP PREVIEW  /  SAMPLE INVENTORY";
 return model;
}
}
