#pragma once
#include <cstdint>

namespace Blam::Tags::Objects
{
	enum MedalType : uint8_t
	{
		Exterminations,
		Perfections,
		DoubleKills,
		TripleKills,
		OverKills,
		Killtaculars,
		Killtrocities,
		Killimanjaros,
		Killtastrophes,
		Killpocalypses,
		Killionaires,
		KillingSprees,
		KillingFrenzies,
		RunningRiots,
		Rampages,
		Untouchables,
		Invincibles,
		SniperSprees,
		Sharpshooters,
		ShotgunSprees,
		OpenSeasons,
		SplatterSprees,
		VehicularManslaughters,
		SwordSprees,
		SliceNDices,
		JuggernautSprees,
		Unstoppables,
		InfectionSprees,
		MMMBrains,
		ZombieKillingSprees,
		HellsJanitors,
		Unknown1,
		HailToTheKings,
		Bulltrue,
		Splatters,
		HiJacks,
		Skyjacks,
		KillsFromTheGrave,
		Killjoys,
		LaserKills,
		Sticks,
		Headshots,
		Assasinations,
		Beatdowns,
		Incinerations,
		Wheelmans,
		BombPlanteds,
		KilledBombCarriers,
		KilledVIPs,
		KilledJuggernauts,
		OddballKills,
		FlagScores,
		KilledFlagCarriers,
		MedalCount
	};

	const std::string MedalTypeNames[MedalType::MedalCount] =
	{
		"extermination",
		"perfection",
		"double_kill",
		"triple_kill",
		"overkill",
		"killtacular",
		"killtrocity",
		"killimanjaro",
		"killtastrophe",
		"killpocalypse",
		"killionaire",
		"killing_spree",
		"killing_frenzy",
		"running_riot",
		"rampage",
		"untouchable",
		"invincible",
		"sniper_spree",
		"sharpshooter",
		"shotgun_spree",
		"open_season",
		"splatter_spree",
		"vehicular_manslaughter",
		"sword_spree",
		"slice_n_dice",
		"juggernaut_spree",
		"unstoppable",
		"infection_spree",
		"mmm_brains",
		"zombie_killing_spree",
		"hells_janitor",
		"Unknown1",
		"hail_to_the_king",
		"bulltrue",
		"splatter",
		"hijacker",
		"skyjacker",
		"from_the_grave",
		"killjoy",
		"laser_kill",
		"grenade_stick",
		"sniper_kill",
		"assassin",
		"beat_down",
		"incineration",
		"wheelman",
		"bomb_planted",
		"bomb_carrier_kill",
		"vip_kill",
		"juggernaut_kill",
		"oddball_kill",
		"flag_captured",
		"flag_carrier_kill",
	};
}