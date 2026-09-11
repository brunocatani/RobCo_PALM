#include "WheelView.h"
#include "PalmTheme.h"
#include <imgui.h>
#include <cstdio>

namespace wheel {
namespace {
ImU32 phosphor(int alpha=255) {return ImGui::ColorConvertFloat4ToU32(palm::theme::green(alpha/255.f));}
void centered(ImDrawList* draw, ImVec2 position, float size, ImU32 tint, const char* text) {
 auto* font=ImGui::GetFont();
 const auto extent=font->CalcTextSizeA(size,10000,0,text);
 draw->AddText(font,size,{position.x-extent.x/2,position.y-extent.y/2},tint,text);
}
Icon categoryIcon(Category category){return static_cast<Icon>(static_cast<unsigned>(Icon::Aid)+static_cast<unsigned>(category));}
void sector(ImDrawList* draw, ImVec2 center, float inner, float outer, float a, float b, ImU32 tint,ImU32 outline=0) {
 const int pieces=std::max(20,static_cast<int>((b-a)*24));
 // Antialias only the perimeter; AA on every tile creates visible radial seams.
 const auto flags=draw->Flags;draw->Flags&=~ImDrawListFlags_AntiAliasedFill;
 for(int k=0;k<pieces;++k) {
  const float start=a+(b-a)*k/pieces, end=a+(b-a)*(k+1)/pieces;
  auto point=[&](float angle,float radius){return ImVec2{center.x+std::cos(angle)*radius,center.y+std::sin(angle)*radius};};
  draw->AddQuadFilled(point(start,inner),point(start,outer),point(end,outer),point(end,inner),tint);
 }
 draw->Flags=flags;
 if(outline) {
  draw->PathArcTo(center,outer,a,b,pieces);draw->PathArcTo(center,inner,b,a,pieces);
  draw->PathStroke(outline,ImDrawFlags_Closed,1.f);
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
 const auto green=phosphor(alpha),muted=phosphor(alpha*3/4);
 const auto dark=ImGui::ColorConvertFloat4ToU32(palm::theme::ink(alpha/255.f));
 const auto surface=ImGui::ColorConvertFloat4ToU32(palm::theme::ink((alpha*3/4)/255.f));
 const auto boundary=phosphor(alpha/3);
 const ImVec2 center=point(512,510);
 const float inner=180*scale, outer=(326+10*view.animation)*scale;
 const float mx=io.MousePos.x-center.x, my=io.MousePos.y-center.y;
 normalizeWheelSelection(model);
 const auto layout=navigationLayout(model);
 const int navigation=hitCenter(mx,my,scale,layout);
 const int hoveredCategory=navigationCategory(navigation);
 if(hoveredCategory>=0 && model.enabled[hoveredCategory]) {
  model.category=static_cast<Category>(hoveredCategory);model.gestures.showing=false;model.activeSection=0;
 }
 if(navigation==static_cast<int>(kLeftGesturesNavigation) || navigation==static_cast<int>(kRightGesturesNavigation)) {
  model.gestures.showing=true;model.gestures.left=navigation==static_cast<int>(kLeftGesturesNavigation);model.activeSection=0;
 }
 if(navigation>=static_cast<int>(kExternalNavigation)) {
  model.activeSection=model.sections.sections[navigation-kExternalNavigation].handle;model.gestures.showing=false;
 }
 const SectionSnapshot* external=nullptr;
 for(unsigned i=0;i<model.sections.count;++i)
  if(model.sections.sections[i].enabled && model.sections.sections[i].handle==model.activeSection)external=&model.sections.sections[i];
 const auto category=static_cast<unsigned>(model.category);
 auto& gestures=model.gestures;const bool showingGestures=!external && gestures.showing;
 const bool showingItems=!external && !showingGestures && model.enabled[category];
 auto& items=model.items[category];
 const int hit=hitSlot(mx,my,inner,outer);
 const std::size_t index=hit>=0 ? hit : items.size();
 const Item* hovered=showingItems && index<items.size()? &items[index]:nullptr;
 const auto* externalHover=external && hit>=0 && static_cast<unsigned>(hit)<external->count?&external->items[hit]:nullptr;
 const unsigned hand=gestures.left?1:0;

 for(std::size_t slot=0;(showingItems || showingGestures || external) && slot<kSlots;++slot) {
  const float mid=-kPi/2+static_cast<float>(slot)*kPi/4;
  const float start=mid-kPi/8+.021f, end=mid+kPi/8-.021f;
  const auto itemIndex=slot;
  if(external) {
   const auto* item=slot<external->count?&external->items[slot]:nullptr;
   const bool available=item && !(item->flags&static_cast<unsigned>(palm::api::ItemFlag::Disabled));
   const bool over=available && hit==static_cast<int>(slot);
   sector(draw,center,inner,outer,start,end,over?green:surface,over?green:item?boundary:phosphor(alpha/8));
   if(!item)continue;
   const ImVec2 p{center.x+std::cos(mid)*258*scale,center.y+std::sin(mid)*258*scale};
   const auto ink=over?dark:available?green:muted;
   drawIcon(draw,{p.x,p.y-39*scale},46*scale,item->icon,ink,icons);
   if(item->flags&static_cast<unsigned>(palm::api::ItemFlag::Equipped))centered(draw,{p.x,p.y+4*scale},16*scale,ink,"EQUIPPED");
   else if(item->flags&static_cast<unsigned>(palm::api::ItemFlag::ShowQuantity)) {
    char quantity[16];std::snprintf(quantity,sizeof(quantity),"%u",item->quantity);
    centered(draw,{p.x,p.y+4*scale},22*scale,ink,quantity);
   }
   const float labelSize=16*scale,labelWidth=140*scale;
   const auto extent=ImGui::GetFont()->CalcTextSizeA(labelSize,labelWidth,labelWidth,item->name);
   draw->PushClipRect({p.x-labelWidth/2,p.y+22*scale},{p.x+labelWidth/2,p.y+62*scale},true);
   draw->AddText(ImGui::GetFont(),labelSize,{p.x-extent.x/2,p.y+23*scale},ink,item->name,nullptr,labelWidth);
   draw->PopClipRect();continue;
  }
  if(showingGestures) {
   const auto& definition=kGestures[slot];const bool over=hit==static_cast<int>(slot);
   const bool selected=gestures.active[hand]==gestureChoice(static_cast<unsigned>(slot),gestures.left);
   sector(draw,center,inner,outer,start,end,over?green:surface,over || selected?green:boundary);
   if(selected)sector(draw,center,outer-4*scale,outer,start,end,green);
   const ImVec2 p{center.x+std::cos(mid)*258*scale,center.y+std::sin(mid)*258*scale};
   drawIcon(draw,{p.x,p.y-21*scale},40*scale,static_cast<Icon>(slot),over?dark:green,icons,!gestures.left);
   centered(draw,{p.x,p.y+27*scale},16*scale,over?dark:green,definition.name);
   if(selected)centered(draw,{p.x,p.y+49*scale},12*scale,over?dark:green,"ACTIVE");
   continue;
  }
  const bool available=itemIndex<items.size();
  const bool active=available && items[itemIndex].count>0 && hit==static_cast<int>(slot);
  sector(draw,center,inner,outer,start,end,active?green:surface,active?green:available?boundary:phosphor(alpha/8));
  const ImVec2 slotCenter{center.x+std::cos(mid)*258*scale,center.y+std::sin(mid)*258*scale};
  if(!available)continue;
  const auto& item=items[itemIndex];
  const auto ink=active?dark:green;
  drawIcon(draw,{slotCenter.x,slotCenter.y-39*scale},46*scale,item.icon==Icon::Automatic?categoryIcon(model.category):item.icon,ink,icons);
  if(!isEquipment(model.category) || item.equipped) {
   char count[32];if(item.equipped && isEquipment(model.category))std::snprintf(count,sizeof(count),"EQUIPPED");
   else std::snprintf(count,sizeof(count),"%u",item.count);
   centered(draw,{slotCenter.x,slotCenter.y+4*scale},isEquipment(model.category)?16*scale:22*scale,ink,count);
  }
  const float labelSize=16*scale, labelWidth=140*scale;
  const auto labelExtent=ImGui::GetFont()->CalcTextSizeA(labelSize,labelWidth,labelWidth,item.name.c_str());
  draw->PushClipRect({slotCenter.x-labelWidth/2,slotCenter.y+22*scale},{slotCenter.x+labelWidth/2,slotCenter.y+62*scale},true);
  draw->AddText(ImGui::GetFont(),labelSize,{slotCenter.x-labelExtent.x/2,slotCenter.y+23*scale},
   ink,item.name.c_str(),nullptr,labelWidth);
  draw->PopClipRect();
 }
 for(unsigned slot=0;slot<layout.count;++slot) {
  const unsigned c=layout.entries[slot];
  const auto* section=c>=kExternalNavigation?&model.sections.sections[c-kExternalNavigation]:nullptr;
  const int itemCategory=navigationCategory(static_cast<int>(c));
  const bool gestureEntry=c==kLeftGesturesNavigation || c==kRightGesturesNavigation;
  const float mid=layout.angle(slot);
  const bool over=navigation==static_cast<int>(c);
  const auto ink=over?dark:green;
  const bool selected=external?section && section->handle==external->handle:showingGestures?c==(gestures.left?kLeftGesturesNavigation:kRightGesturesNavigation):showingItems && itemCategory==static_cast<int>(category);
  sector(draw,center,70*scale,(inner-8*scale),mid-layout.step/2+kNavigationGap,mid+layout.step/2-kNavigationGap,
   over?green:surface,selected || over?green:boundary);
  if(selected)
   sector(draw,center,inner-11*scale,inner-8*scale,mid-layout.step/2+.06f,mid+layout.step/2-.06f,green);
  const ImVec2 label{center.x+std::cos(mid)*119*scale,center.y+std::sin(mid)*119*scale};
  if(section) {
   drawIcon(draw,{label.x,label.y-15*scale},20*scale,section->icon,over || selected?ink:muted,icons);
   const float labelSize=(layout.count>8?12:13)*scale,labelWidth=layout.count>8?66*scale:78*scale;
   const auto extent=ImGui::GetFont()->CalcTextSizeA(labelSize,labelWidth,labelWidth,section->name);
   draw->PushClipRect({label.x-labelWidth/2,label.y+9*scale},{label.x+labelWidth/2,label.y+40*scale},true);
   draw->AddText(ImGui::GetFont(),labelSize,{label.x-extent.x/2,label.y+9*scale},over || selected?ink:muted,section->name,nullptr,labelWidth);
   draw->PopClipRect();
  }else if(gestureEntry) {
   drawIcon(draw,{label.x,label.y-19*scale},19*scale,c==kLeftGesturesNavigation?Icon::LeftHand:Icon::RightHand,over || selected?ink:muted,icons);
   centered(draw,{label.x,label.y+10*scale},15*scale,over || selected?ink:muted,c==kLeftGesturesNavigation?"LEFT":"RIGHT");
   centered(draw,{label.x,label.y+27*scale},11*scale,over || selected?ink:muted,"GESTURES");
  } else {
   drawIcon(draw,{label.x,label.y-12*scale},20*scale,itemCategory>=0?categoryIcon(static_cast<Category>(itemCategory)):Icon::Config,over || selected?ink:muted,icons);
   centered(draw,{label.x,label.y+20*scale},13*scale,over || selected?ink:muted,itemCategory>=0?categoryName(static_cast<Category>(itemCategory)):"CONFIG");
  }
 }
 const bool cancelHovered=navigation==static_cast<int>(kCancelNavigation);
 action.cancelHovered=cancelHovered;
 draw->AddCircleFilled(center,54*scale,cancelHovered?green:surface,48);
 draw->AddCircle(center,54*scale,cancelHovered?green:boundary,48,scale);
 centered(draw,{center.x,center.y-10*scale},16*scale,cancelHovered?dark:green,"P.A.L.M");
 centered(draw,{center.x,center.y+16*scale},(cancelHovered?18:12)*scale,cancelHovered?dark:muted,"CANCEL");
 action.hoveredItem=hovered && hovered->count>0?selectionToken(*hovered):0;
 action.configHovered=navigation==static_cast<int>(kConfigNavigation);
 if(external) {
  centered(draw,point(512,886),23*scale,green,externalHover?externalHover->name:external->name);
  if(externalHover) {
   const bool available=!(externalHover->flags&static_cast<unsigned>(palm::api::ItemFlag::Disabled));
   if(available){action.section=external->handle;action.sectionItem=externalHover->id;}
   centered(draw,point(512,920),16*scale,muted,available?"RELEASE TRIGGER OR GRAB TO SELECT":"UNAVAILABLE");
  }else if(!external->count)centered(draw,point(512,920),16*scale,muted,"No items available from this mod");
 }else if(showingGestures) {
  if(hit>=0) {
   action.hoveredGesture=gestureChoice(static_cast<unsigned>(hit),gestures.left);
   centered(draw,point(512,886),23*scale,green,kGestures[hit].name);
  }
  const bool clearing=action.hoveredGesture && gestures.active[hand]==action.hoveredGesture;
  const bool free=gestures.availability[hand]==GestureAvailability::Free;
  if(hit>=0 || !free)centered(draw,point(512,920),16*scale,free || clearing?muted:green,
   clearing?"RELEASE TRIGGER OR GRAB TO CLEAR GESTURE":!free?gestureAvailabilityText(gestures.availability[hand]):
   gestures.left?"RELEASE TRIGGER OR GRAB TO POSE LEFT HAND":"RELEASE TRIGGER OR GRAB TO POSE RIGHT HAND");
 }else if(hovered) {
  centered(draw,point(512,886),23*scale,green,hovered->name.c_str());
  centered(draw,point(512,920),16*scale,muted,!hovered->count?"NOT CURRENTLY CARRIED":isEquipment(model.category)?(hovered->equipped?"RELEASE TRIGGER OR GRAB TO UNEQUIP":"RELEASE TRIGGER OR GRAB TO EQUIP"):"RELEASE TRIGGER OR GRAB TO TAKE ONE");
 }else if(!showingItems)centered(draw,point(512,886),20*scale,muted,"Enable wheel sections in Config");
 else if(items.empty())centered(draw,point(512,886),20*scale,muted,"Choose your items in Config");
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
 return model;
}
}
