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
Icon categoryIcon(Category category){return static_cast<Icon>(static_cast<unsigned>(Icon::Aid)+static_cast<unsigned>(category));}
void sector(ImDrawList* draw, ImVec2 center, float inner, float outer, float a, float b, ImU32 tint) {
 constexpr int pieces=20;
 for(int k=0;k<pieces;++k) {
  const float start=a+(b-a)*k/pieces, end=a+(b-a)*(k+1)/pieces;
  auto point=[&](float angle,float radius){return ImVec2{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};};
  draw->AddQuadFilled(point(start,inner),point(start,outer),point(end,outer),point(end,inner),tint);
 }
}
}

Action drawWheel(Model& model, View& view, ImVec2 position, ImVec2 size,const IconTextures& icons) {
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
 const int hoveredCategory=navigationCategory(navigation);
 if(hoveredCategory>=0 && model.enabled[hoveredCategory]) {
  model.category=static_cast<Category>(hoveredCategory);model.gestures.showing=false;
 }
 if(navigation==static_cast<int>(kLeftGesturesNavigation) || navigation==static_cast<int>(kRightGesturesNavigation)) {
  model.gestures.showing=true;model.gestures.left=navigation==static_cast<int>(kLeftGesturesNavigation);
 }
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
   drawIcon(draw,{p.x,p.y-21*scale},40*scale,static_cast<Icon>(slot),over?white:gestureColor,icons,!gestures.left);
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
  drawIcon(draw,{slotCenter.x,slotCenter.y-39*scale},46*scale,item.icon==Icon::Automatic?categoryIcon(model.category):item.icon,ink,icons);
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
  const int itemCategory=navigationCategory(static_cast<int>(c));
  const bool gestureEntry=c==kLeftGesturesNavigation || c==kRightGesturesNavigation;
  if(itemCategory>=0 && !model.enabled[itemCategory])continue;
  const float mid=-kPi/2+c*(2*kPi/kNavigationCount);
  const bool over=navigation==static_cast<int>(c);
  const auto tint=itemCategory>=0?color(static_cast<Category>(itemCategory),alpha):gestureEntry?gestureColor:IM_COL32(104,226,193,alpha);
  const bool selected=showingGestures?c==(gestures.left?kLeftGesturesNavigation:kRightGesturesNavigation):itemCategory==static_cast<int>(category);
  sector(draw,center,70*scale,(inner-8*scale),mid-kPi/kNavigationCount+.035f,mid+kPi/kNavigationCount-.035f,
   over?IM_COL32(42,70,70,alpha):IM_COL32(17,31,35,alpha*9/10));
  if(selected)
   sector(draw,center,inner-12*scale,inner-8*scale,mid-kPi/kNavigationCount+.06f,mid+kPi/kNavigationCount-.06f,tint);
  const ImVec2 label{center.x+std::cos(mid)*119*scale,center.y+std::sin(mid)*119*scale};
  if(gestureEntry) {
   drawIcon(draw,{label.x,label.y-19*scale},19*scale,c==kLeftGesturesNavigation?Icon::LeftHand:Icon::RightHand,over || selected?tint:muted,icons);
   centered(draw,{label.x,label.y+10*scale},15*scale,over || selected?tint:muted,c==kLeftGesturesNavigation?"LEFT":"RIGHT");
   centered(draw,{label.x,label.y+27*scale},11*scale,over || selected?tint:muted,"GESTURES");
  } else {
   drawIcon(draw,{label.x,label.y-12*scale},20*scale,itemCategory>=0?categoryIcon(static_cast<Category>(itemCategory)):Icon::Config,over || selected?tint:muted,icons);
   centered(draw,{label.x,label.y+20*scale},13*scale,over || selected?tint:muted,itemCategory>=0?categoryName(static_cast<Category>(itemCategory)):"CONFIG");
  }
 }
 draw->AddCircleFilled(center,54*scale,navigation==static_cast<int>(kCancelNavigation)?IM_COL32(37,58,61,alpha):IM_COL32(12,24,28,alpha),48);
 centered(draw,center,13*scale,muted,"CANCEL");
 action.hoveredItem=hovered && hovered->count>0?selectionToken(*hovered):0;
 action.configHovered=navigation==static_cast<int>(kConfigNavigation);
 if(showingGestures) {
  if(hit>=0) {
   action.hoveredGesture=gestureChoice(static_cast<unsigned>(hit),gestures.left);
   centered(draw,point(512,886),23*scale,white,kGestures[hit].name);
  } else centered(draw,point(512,886),20*scale,white,gestures.left?"LEFT HAND GESTURES":"RIGHT HAND GESTURES");
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
 const std::array<std::vector<Icon>,kCategoryCount> icons{{
  {Icon::Stimpak,Icon::RadAway,Icon::RadX,Icon::MedX,Icon::Psycho,Icon::Buffout,Icon::Mentats,Icon::Jet,Icon::Antibiotics,Icon::RefreshingBeverage},
  {Icon::NukaCola,Icon::PurifiedWater,Icon::CookedMeat,Icon::PackagedFood,Icon::PackagedFood,Icon::CookedMeat},
  {Icon::FragGrenade,Icon::Molotov,Icon::PlasmaGrenade,Icon::PulseGrenade,Icon::CryoGrenade,Icon::NukaGrenade},
  {Icon::Pistol10mm,Icon::CombatRifle,Icon::LaserRifle,Icon::HuntingRifle},
  {Icon::ChestArmor,Icon::LeftArm,Icon::RightArm,Icon::Clothing}
 }};
 for(unsigned c=0;c<kCategoryCount;++c)for(unsigned i=0;i<model.items[c].size();++i)model.items[c][i].icon=icons[c][i];
 model.status="DESKTOP PREVIEW  /  SAMPLE INVENTORY";
 return model;
}
}
