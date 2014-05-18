#include "tyrant.h"

#include <string>

const std::string faction_names[Faction::num_factions] =
{ "", "imperial", "raider", "bloodthirsty", "xeno", "righteous", "progenitor" };

std::string skill_names[Skill::num_skills] =
{
    // Placeholder for new gained skill from battleground effect:
    "<Error>",
    // Attack:
    "0",
    // Activation (Including Destroyed):
    "Augment", "Backfire", "Chaos", "Cleanse", "Enfeeble",
    "Freeze", "Heal", "Infuse", "Jam",
    "Mimic", "Protect", "Rally", "Recharge", "Repair", "Rush", "Shock",
    "Siege", "Split", "Strike", "Summon", "Supply",
    "trigger_regen",
    "Weaken",
    // Combat-Modifier:
    "AntiAir", "Burst", "Fear", "Flurry", "Pierce", "Swipe", "Valor",
    // Damage-Dependant:
    "Berserk", "Crush", "Disease", "Immobilize", "Inhibit", "Leech", "Phase", "Poison", "Siphon", "Sunder",
    // Defensive:
    "Armored", "Corrosive", "Counter", "Emulate", "Evade", "Flying", "Intercept", "Payback", "Refresh", "Regenerate", "Stun", "Tribute", "Wall",
    // Triggered:
    "Blitz", "Legion",
    // Tyrant Unleashed:
    "Enhance",
    // Static (Ignored):
    "Fusion",
    /* "Blizzard", "Mist", */
};

std::set<Skill> helpful_skills{
    augment, cleanse, enhance, heal, protect, rally, repair, rush, supply,
};

std::set<Skill> defensive_skills{
    armored, counter, emulate, evade, flying, intercept, payback, refresh, regenerate, stun, tribute, wall,
};

std::string skill_activation_modifier_names[SkillMod::num_skill_activation_modifiers] = {"", " on Play", " on Attacked", " on Kill", " on Death", };

std::string cardtype_names[CardType::num_cardtypes]{"Commander", "Assault", "Structure", "Action", };

#if defined(TYRANT_UNLEASHED)
std::string rarity_names[6]{"", "common", "rare", "epic", "legendary", "vindicator", };
#else
std::string rarity_names[5]{"", "common", "uncommon", "rare", "legendary", };
#endif

// begin for TYRANT_UNLEASHED
unsigned upgrade_cost[]{0, 5, 15, 30, 75, 150};
unsigned salvaging_income[][7]{{}, {0, 1, 2, 5}, {0, 5, 10, 15, 20}, {0, 20, 25, 30, 40, 50, 65}, {0, 40, 45, 60, 75, 100, 125}, {0, 80, 85, 100, 125, 175, 250}};
// end

std::string decktype_names[DeckType::num_decktypes]{"Deck", "Mission", "Raid", "Quest", "Custom Deck", };

std::string effect_names[Effect::num_effects] = {
    "None",
    "Time Surge",
    "Copycat",
    "Quicksilver",
    "Decay",
    "High Skies",
    "Impenetrable",
    "Invigorate",
    "Clone Project",
    "Friendly Fire",
    "Genesis",
    "Artillery Strike",
    "Photon Shield",
    "Decrepit",
    "Forcefield",
    "Chilling Touch",
    "Clone Experiment",
    "Toxic",
    "Haunt",
    "United Front",
    "Harsh Conditions",
};

std::string achievement_misc_req_names[AchievementMiscReq::num_achievement_misc_reqs] = {
    "Kill units with skill: flying",
    "Skill activated: (any)",
    "Turns",
    "Damage",
    "Total damage to the enemy Commander"
};
