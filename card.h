#ifndef CARD_H_INCLUDED
#define CARD_H_INCLUDED

#include <string>
#include <vector>
#include <cstring>
#include "tyrant.h"

class Card
{
public:
    unsigned m_antiair;
    unsigned m_armored;
    unsigned m_attack;
    unsigned m_base_id;  // The id of the original card if a card is unique and alt/upgraded. The own id of the card otherwise.
    unsigned m_berserk;
    unsigned m_berserk_oa;
    bool m_blitz;
    unsigned m_burst;
    unsigned m_corrosive;
    unsigned m_counter;
    unsigned m_crush;
    unsigned m_delay;
    bool m_disease;
    bool m_disease_oa;
    bool m_emulate;
    unsigned m_evade;
    Faction m_faction;
    bool m_fear;
    unsigned m_final_id; // The id of fully upgraded card
    unsigned m_flurry;
    bool m_flying;
    bool m_fusion;
    unsigned m_health;
    unsigned m_hidden;
    unsigned m_id;
    bool m_immobilize;
    unsigned m_inhibit;
    bool m_intercept;
    unsigned m_leech;
    unsigned m_legion;
    unsigned m_level;
    std::string m_name;
    bool m_payback;
    unsigned m_pierce;
    unsigned m_phase;
    unsigned m_poison;
    unsigned m_poison_oa;
    unsigned m_rarity;
    bool m_refresh;
    unsigned m_regenerate;
    unsigned m_replace;
    unsigned m_reserve;
    unsigned m_set;
    unsigned m_siphon;
    bool m_split;
    bool m_stun;
    bool m_sunder;
    bool m_sunder_oa;
    bool m_swipe;
    bool m_tribute;
    bool m_unique;
    unsigned m_valor;
    bool m_wall;
    std::vector<SkillSpec> m_skills[SkillMod::num_skill_activation_modifiers];
    CardType::CardType m_type;
    unsigned m_recipe_cost;
    std::map<const Card*, unsigned> m_recipe_cards;
    std::map<const Card*, unsigned> m_used_for_cards;
    unsigned m_skill_pos[num_skills];

public:
    Card() :
        m_antiair(0),
        m_armored(0),
        m_attack(0),
        m_base_id(0),
        m_berserk(0),
        m_berserk_oa(0),
        m_blitz(false),
        m_burst(0),
        m_corrosive(0),
        m_counter(0),
        m_crush(0),
        m_delay(0),
        m_disease(false),
        m_disease_oa(false),
        m_emulate(false),
        m_evade(0),
        m_faction(imperial),
        m_fear(false),
        m_final_id(0),
        m_flurry(0),
        m_flying(false),
        m_fusion(false),
        m_health(0),
        m_hidden(0),
        m_id(0),
        m_immobilize(false),
        m_inhibit(0),
        m_intercept(false),
        m_leech(0),
        m_legion(0),
        m_level(1),
        m_name(""),
        m_payback(false),
        m_pierce(0),
        m_phase(0),
        m_poison(0),
        m_poison_oa(0),
        m_rarity(1),
        m_refresh(false),
        m_regenerate(0),
        m_replace(0),
        m_set(0),
        m_siphon(0),
        m_split(false),
        m_stun(false),
        m_sunder(false),
        m_sunder_oa(false),
        m_swipe(false),
        m_tribute(false),
        m_unique(false),
        m_valor(0),
        m_wall(false),
        m_skills(),
        m_type(CardType::assault),
        m_recipe_cost(0),
        m_recipe_cards(),
        m_used_for_cards()
    {
        std::memset(m_skill_pos, 0, sizeof m_skill_pos);
    }

    void add_skill(Skill id, unsigned x, Faction y, unsigned c, Skill s, bool all, SkillMod::SkillMod mod=SkillMod::on_activate);
};

#endif
