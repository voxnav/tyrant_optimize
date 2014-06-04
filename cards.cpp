#include "cards.h"

#include <boost/tokenizer.hpp>
#include <map>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <list>

#include "tyrant.h"
#include "card.h"

std::string simplify_name(const std::string& card_name)
{
    std::string simple_name;
    for(auto c : card_name)
    {
        if(!strchr(";:, \"'-", c))
        {
            simple_name += ::tolower(c);
        }
    }
    return(simple_name);
}

std::list<std::string> get_abbreviations(const std::string& name)
{
    std::list<std::string> abbr_list;
    boost::tokenizer<boost::char_delimiters_separator<char>> word_token{name, boost::char_delimiters_separator<char>{false, " ", ""}};
    std::string initial;
    auto token_iter = word_token.begin();
    for(; token_iter != word_token.end(); ++token_iter)
    {
        abbr_list.push_back(simplify_name(std::string{token_iter->begin(), token_iter->end()}));
        initial += *token_iter->begin();
    }
    abbr_list.push_back(simplify_name(initial));
    return(abbr_list);
}

//------------------------------------------------------------------------------
Cards::~Cards()
{
    for(Card* c: cards) { delete(c); }
}

const Card* Cards::by_id(unsigned id) const
{
    std::map<unsigned, Card*>::const_iterator cardIter{cards_by_id.find(id)};
    if(cardIter == cards_by_id.end())
    {
        throw std::runtime_error("While trying to find the card with id " + to_string(id) + ": no such key in the cards_by_id map.");
    }
    else
    {
        return(cardIter->second);
    }
}
//------------------------------------------------------------------------------
void Cards::organize()
{
    cards_by_id.clear();
    player_cards.clear();
    player_cards_by_name.clear();
    player_commanders.clear();
    player_assaults.clear();
    player_structures.clear();
    player_actions.clear();
    for(Card* card: cards)
    {
//        std::cout << "C:" << card->m_id << "\n";
        // Remove delimiters from card names
        size_t pos;
        while((pos = card->m_name.find_first_of(";:,")) != std::string::npos)
        {
            card->m_name.erase(pos, 1);
        }
#if defined(TYRANT_UNLEASHED)
        if (card->m_level > 1 && card->m_id == by_id(card->m_base_id)->m_final_id)
        {
            player_cards_by_name[{simplify_name(card->m_name + "-" + to_string(card->m_level)), card->m_hidden}] = card;
        }
        else
        {
            card->m_name += "-" + to_string(card->m_level);
        }
#else
        if(card->m_set == 5002)
        {
            card->m_name += '*';
        }
#endif
        cards_by_id[card->m_id] = card;
        // Card available to players
        if(card->m_set != 0)
        {
            player_cards.push_back(card);
            switch(card->m_type)
            {
                case CardType::commander: {
                    player_commanders.push_back(card);
                    break;
                }
                case CardType::assault: {
                    player_assaults.push_back(card);
                    break;
                }
                case CardType::structure: {
                    player_structures.push_back(card);
                    break;
                }
                case CardType::action: {
                    player_actions.push_back(card);
                    break;
                }
                case CardType::num_cardtypes: {
                    throw card->m_type;
                    break;
                }
            }
            std::string simple_name{simplify_name(card->m_name)};
            auto card_itr = player_cards_by_name.find({simple_name, card->m_hidden});
            if(card_itr == player_cards_by_name.end() || card_itr->second->m_id == card->m_replace)
            {
                player_cards_by_name[{simple_name, card->m_hidden}] = card;
            }
        }
    }
    for(Card* card: cards)
    {
        // generate abbreviations
        if(card->m_set > 0)
        {
            for(auto&& abbr_name : get_abbreviations(card->m_name))
            {
                if(abbr_name.length() > 1 && player_cards_by_name.find({abbr_name, 0}) == player_cards_by_name.end())
                {
                    player_cards_abbr[abbr_name] = card->m_name;
                }
            }
        }

#if not defined(TYRANT_UNLEASHED)
        // update recipes
        if(card->m_set == 5002)
        {
            std::string material_name{simplify_name(card->m_name)};
            material_name.erase(material_name.size() - 1);  // remove suffix "*"
            Card * material_card = player_cards_by_name[{material_name, card->m_hidden}];
            // Promo and Unpurchasable Reward cards only require 1 copy
            unsigned number = material_card->m_set == 5001 || (material_card->m_set == 5000 && material_card->m_reserve) ? 1 : 2;
            // Reward cards still have gold cost
            card->m_recipe_cost = material_card->m_set == 5000 ? (card->m_rarity == 4 ? 100000 : 20000) : 0;
            card->m_recipe_cards[material_card] = number;
            material_card->m_used_for_cards[card] = number;
        }
#endif
    }
}

// class Card
void Card::add_skill(Skill id, unsigned x, Faction y, unsigned c, Skill s, bool all, SkillMod::SkillMod mod)
{
    for(auto it = m_skills[mod].begin(); it != m_skills[mod].end(); ++ it)
    {
        if(it->id == id)
        {
            m_skills[mod].erase(it);
            break;
        }
    }
    m_skills[mod].push_back({id, x, y, c, s, all, mod});
    m_x[mod][id] = std::max(1u, x);
}

