#pragma once
#include <cstdint>

namespace wheel {
// Row-major cells in the authored 4x4 atlases. These are presentation IDs;
// they never enter a save record or affect inventory/equipment behavior.
enum class Icon : std::uint16_t {
 ThumbsUp, MiddleFinger, RockAndRoll, Peace, Pointing, Fist, OpenHand, Shaka,
 Aid, Food, Grenades, Weapons, Armor, Config, LeftHand, RightHand,
 Pistol10mm, Revolver, PipePistol, Deliverer, CombatRifle, HuntingRifle, AssaultRifle, CombatShotgun,
 DoubleBarrel, SubmachineGun, LaserPistol, LaserRifle, PlasmaPistol, PlasmaRifle, InstituteRifle, GaussRifle,
 Minigun, GatlingLaser, MissileLauncher, FatMan, Flamer, Cryolator, GammaGun, LaserMusket,
 RailwayRifle, Broadsider, JunkJet, CombatKnife, BaseballBat, SuperSledge, PowerFist, DeathclawGauntlet,
 Stimpak, RadAway, RadX, MedX, Psycho, Jet, Buffout, Mentats,
 Addictol, Antibiotics, BloodPack, StealthBoy, Serum, Herbal, RefreshingBeverage, Chems,
 NukaCola, NukaCherry, NukaQuantum, PurifiedWater, DirtyWater, Beer, Spirits, Wine,
 RawMeat, CookedMeat, Vegetables, Mutfruit, PackagedFood, CannedFood, SweetRoll, Soup,
 FragGrenade, Molotov, PlasmaGrenade, PulseGrenade, CryoGrenade, NukaGrenade, Mine, BottlecapMine,
 Helmet, PowerArmor, Clothing, ChestArmor, LeftArm, RightArm, LeftLeg, RightLeg,
 GrognakAxe, OfficerSword, Machete, Switchblade, Ripper, Shishkebab, PipeWrench, LeadPipe,
 Baton, Knuckles, BoxingGlove, Board, AlienBlaster, FlareGun, Syringer, PipeRevolver,
 Count, Automatic=Count
};
inline constexpr unsigned kIconSheetCount=7;
static_assert(static_cast<unsigned>(Icon::Count)==kIconSheetCount*16);
}
