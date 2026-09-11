#pragma once
#include "ItemIconCatalog.h"

namespace wheel {
// Keyword checks must include the inventory stack's instance data: an OMOD
// can turn a laser/plasma pistol into a rifle without changing its base FormID.
template<class HasKeyword>
Icon weaponIcon(Icon known,std::uint8_t shape,HasKeyword has) {
 struct Specific {std::uint32_t keyword;Icon icon;};
 constexpr Specific specialized[]{
  {0x0022575B,Icon::MissileLauncher},{0x0022575C,Icon::FatMan},
  {0x0022575D,Icon::Minigun},{0x0022575E,Icon::GatlingLaser},
  {0x0022575F,Icon::Cryolator},{0x00225760,Icon::Flamer},
  {0x00225762,Icon::GammaGun},{0x00225763,Icon::JunkJet},
  {0x00225764,Icon::RailwayRifle},{0x00225766,Icon::Broadsider},
  {0x00226452,Icon::LaserMusket},{0x00226456,Icon::GaussRifle},
  {0x00225765,Icon::Syringer},{0x00225761,Icon::FlareGun},
  {0x0016968B,Icon::AlienBlaster},{0x00225767,Icon::Ripper},{0x00225768,Icon::Shishkebab}
 };
 for(const auto& entry:specialized)if(has(entry.keyword))return entry.icon;
 const bool pistol=has(0x0004A0A0);
 if(has(0x00092A85))return pistol?Icon::PlasmaPistol:Icon::PlasmaRifle;
 if(has(0x00092A84))return pistol?Icon::LaserPistol:known==Icon::InstituteRifle?known:Icon::LaserRifle;
 if(known!=Icon::Automatic)return known;
 if(has(0x0004A0A3))return Icon::Minigun;
 if(has(0x00226454))return Icon::CombatShotgun;
 if(has(0x00226455))return Icon::AssaultRifle;
 if(pistol)return Icon::Pistol10mm;
 if(has(0x001E325D))return Icon::HuntingRifle;
 if(has(0x0004A0A1))return Icon::CombatRifle;
 // WEAPON_TYPE values from the current CommonLibF4VR source.
 switch(shape) {
 case 0:return Icon::Fist;
 case 1:case 5:return Icon::OfficerSword;
 case 2:return Icon::CombatKnife;
 case 3:case 6:return Icon::GrognakAxe;
 case 4:return Icon::BaseballBat;
 case 9:return Icon::CombatRifle;
 case 10:return Icon::FragGrenade;
 case 11:return Icon::Mine;
 default:return Icon::Weapons;
 }
}
inline constexpr Icon armorIcon(std::uint32_t slots,bool powerArmor) {
 // BOD2 editor slots, indexed from slot 30. Cosmetic selection only.
 constexpr std::uint32_t head=(1u<<0)|(1u<<1)|(1u<<2)|(1u<<16)|(1u<<17)|(1u<<18)|(1u<<19)|(1u<<20)|(1u<<22);
 constexpr std::uint32_t torso=(1u<<3)|(1u<<6)|(1u<<11);
 constexpr std::uint32_t leftArm=(1u<<4)|(1u<<7)|(1u<<12),rightArm=(1u<<5)|(1u<<8)|(1u<<13);
 constexpr std::uint32_t leftLeg=(1u<<9)|(1u<<14),rightLeg=(1u<<10)|(1u<<15);
 if(powerArmor && (slots&torso))return Icon::PowerArmor;
 if((slots&torso) && (slots&(leftArm|rightArm|leftLeg|rightLeg)))return Icon::Clothing;
 if(slots&head)return Icon::Helmet;
 if(slots&torso)return Icon::ChestArmor;
 if(slots&leftArm)return Icon::LeftArm;
 if(slots&rightArm)return Icon::RightArm;
 if(slots&leftLeg)return Icon::LeftLeg;
 if(slots&rightLeg)return Icon::RightLeg;
 return Icon::Clothing;
}
}
