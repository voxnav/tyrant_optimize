#ifndef TYRANT_H_INCLUDED
#define TYRANT_H_INCLUDED

#include <string>
#include <tuple>

enum Faction
{
    allfactions,
    bloodthirsty,
    imperial,
    raider,
    righteous,
    xeno,
    num_factions
};
extern const std::string faction_names[num_factions];

enum ActiveSkill
{augment, augment_all, chaos, chaos_all, cleanse, cleanse_all, enfeeble, enfeeble_all,
 freeze, freeze_all, heal, heal_all, infuse, jam, jam_all,
 mimic, protect, protect_all, rally, rally_all, rush, shock,
 siege, siege_all, strike, strike_all, summon, supply,
 trigger_regen, // not actually a skill; handles regeneration after strike/siege
 weaken, weaken_all, num_skills};
extern std::string skill_names[num_skills];

namespace CardType {
enum CardType {
    action,
    assault,
    commander,
    structure,
    num_cardtypes
};
}

extern std::string cardtype_names[CardType::num_cardtypes];

enum gamemode_t
{
    fight,
    surge,
    tournament
};

struct true_ {};

struct false_ {};

template<unsigned>
struct skillTriggersRegen { typedef false_ T; };

template<>
struct skillTriggersRegen<strike> { typedef true_ T; };

template<>
struct skillTriggersRegen<strike_all> { typedef true_ T; };

template<>
struct skillTriggersRegen<siege> { typedef true_ T; };

template<>
struct skillTriggersRegen<siege_all> { typedef true_ T; };

enum SkillSourceType
{
    source_hostile,
    source_allied,
    source_global_hostile,
    source_global_allied,
    source_chaos
};

typedef std::tuple<ActiveSkill, unsigned, Faction> SkillSpec;

#endif
