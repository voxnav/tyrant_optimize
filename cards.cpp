#include "cards.h"

#include <boost/tokenizer.hpp>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <list>
#include <iostream>

#include "tyrant.h"
#include "card.h"

std::string simplify_name(const std::string& card_name)
{
    std::string simple_name;
    for(auto c : card_name)
    {
        if(!strchr(";:,\"'! ", c))
        {
            simple_name += ::tolower(c);
        }
    }
    return(simple_name);
}

std::list<std::string> get_abbreviations(const std::string& name)
{
    std::list<std::string> abbr_list;
    boost::tokenizer<boost::char_delimiters_separator<char>> word_token{name, boost::char_delimiters_separator<char>{false, " -", ""}};
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
    for (Card* c: all_cards) { delete(c); }
}

const Card* Cards::by_id(unsigned id) const
{
    const auto cardIter = cards_by_id.find(id);
    if(cardIter == cards_by_id.end())
    {
        throw std::runtime_error("No card with id " + to_string(id));
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
    cards_by_name.clear();
    player_commanders.clear();
    player_assaults.clear();
    player_structures.clear();
    // Round 1: set cards_by_id
    for(Card* card: all_cards)
    {
        cards_by_id[card->m_id] = card;
    }
    // Round 2: depend on cards_by_id / by_id(); update m_name, [TU] m_top_level_card etc.; set cards_by_name; 
    for(Card* card: all_cards)
    {
        // Remove delimiters from card names
        size_t pos;
        while((pos = card->m_name.find_first_of(";:,")) != std::string::npos)
        {
            card->m_name.erase(pos, 1);
        }
        // set m_top_level_card for non base cards
        card->m_top_level_card = by_id(card->m_base_id)->m_top_level_card;
        // Cards available ("visible") to players have priority
        std::string base_name = card->m_name;
        if (card == card->m_top_level_card)
        {
            add_card(card, card->m_name + "-" + to_string(card->m_level));
        }
        else
        {
            card->m_name += "-" + to_string(card->m_level);
        }
        add_card(card, card->m_name);
    }
#if 0 // TODO refactor precedence
    // Round 3: depend on cards_by_name; set abbreviations
    for(Card* card: cards)
    {
        // generate abbreviations
        if(card->m_set > 0)
        {
            for(auto&& abbr_name : get_abbreviations(card->m_name))
            {
                if(abbr_name.length() > 1 && cards_by_name.find(abbr_name) == cards_by_name.end())
                {
                    player_cards_abbr[abbr_name] = card->m_name;
                }
            }
        }
    }
#endif
}

void Cards::add_card(Card * card, const std::string & name)
{
    std::string simple_name{simplify_name(name)};
    auto card_itr = cards_by_name.find(simple_name);
    signed old_visible = card_itr == cards_by_name.end() ? -1 : visible_cardset.count(card_itr->second->m_set);
    signed new_visible = visible_cardset.count(card->m_set);
    if (card_itr != cards_by_name.end())
    {
        if (old_visible == new_visible)
        {
            ambiguous_names.insert(simple_name);
        }
        _DEBUG_MSG(2, "Duplicated card name \"%s\" [%u] set=%u (visible=%u) : [%u] set=%u (visible=%u)\n", name.c_str(), card_itr->second->m_id, card_itr->second->m_set, old_visible, card->m_id, card->m_set, new_visible);
    }
    else if (old_visible < new_visible)
    {
        ambiguous_names.erase(simple_name);
        cards_by_name[simple_name] = card;
        if (new_visible)
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
                case CardType::num_cardtypes: {
                    throw card->m_type;
                    break;
                }
            }
        }
    }
}

// class Card
void Card::add_skill(Skill id, unsigned x, Faction y, unsigned n, unsigned c, Skill s, Skill s2, bool all)
{
    for(auto it = m_skills.begin(); it != m_skills.end(); ++ it)
    {
        if(it->id == id)
        {
            m_skills.erase(it);
            break;
        }
    }
    m_skills.push_back({id, x, y, n, c, s, s2, all});
    m_skill_value[id] = x ? x : n ? n : 1;
}

