#ifndef CARD_H_INCLUDED
#define CARD_H_INCLUDED

#include <string>
#include <vector>
#include <cstring>
#include "tyrant.h"

class Card
{
public:
    unsigned m_attack;
    unsigned m_base_id;  // The id of the original card if a card is unique and alt/upgraded. The own id of the card otherwise.
    unsigned m_delay;
    bool m_disease_oa;
    Faction m_faction;
    unsigned m_health;
    unsigned m_hidden;
    unsigned m_id;
    unsigned m_level;
    std::string m_name;
    unsigned m_phase;
    unsigned m_rarity;
    unsigned m_replace;
    unsigned m_reserve;
    unsigned m_set;
    bool m_unique;
    std::vector<SkillSpec> m_skills[SkillMod::num_skill_activation_modifiers];
    unsigned m_skill_value[SkillMod::num_skill_activation_modifiers][num_skills];
    CardType::CardType m_type;
    const Card* m_top_level_card; // [TU] corresponding full-level card
    unsigned m_recipe_cost;
    std::map<const Card*, unsigned> m_recipe_cards;
    std::map<const Card*, unsigned> m_used_for_cards;

public:
    Card() :
        m_attack(0),
        m_base_id(0),
        m_delay(0),
        m_disease_oa(false),
        m_faction(imperial),
        m_health(0),
        m_hidden(0),
        m_id(0),
        m_level(1),
        m_name(""),
        m_phase(0),
        m_rarity(1),
        m_replace(0),
        m_set(0),
        m_unique(false),
        m_skills(),
        m_type(CardType::assault),
        m_top_level_card(this),
        m_recipe_cost(0),
        m_recipe_cards(),
        m_used_for_cards()
    {
        std::memset(m_skill_value, 0, sizeof m_skill_value);
    }

    void add_skill(Skill id, unsigned x, Faction y, unsigned c, Skill s, bool all, SkillMod::SkillMod mod=SkillMod::on_activate);
    const Card* upgraded() const { return this == m_top_level_card ? this : m_used_for_cards.begin()->first; }
};

#endif
