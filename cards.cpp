#include "cards.h"

#include <boost/tokenizer.hpp>
#include <map>
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
        if(!strchr(";:,\"' ", c))
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
    for(Card* c: cards) { delete(c); }
}

const Card* Cards::by_id(unsigned id) const
{
    std::map<unsigned, Card*>::const_iterator cardIter{cards_by_id.find(id)};
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
    player_cards_by_name.clear();
    player_commanders.clear();
    player_assaults.clear();
    player_structures.clear();
    player_actions.clear();
    // Round 1: set cards_by_id
    for(Card* card: cards)
    {
        cards_by_id[card->m_id] = card;
    }
    // Round 2: depend on cards_by_id / by_id(); update m_name, [TU] m_top_level_card etc.; set player_cards_by_name; 
    for(Card* card: cards)
    {
        // Remove delimiters from card names
        size_t pos;
        while((pos = card->m_name.find_first_of(";:,")) != std::string::npos)
        {
            card->m_name.erase(pos, 1);
        }
        // set m_top_level_card for non base cards
        card->m_top_level_card = by_id(card->m_base_id)->m_top_level_card;
        // add a suffix of level to the name of cards; register as alias for the full-level cards (the formal name is without suffix)
        if (card == card->m_top_level_card)
        {
            player_cards_by_name[simplify_name(card->m_name + "-" + to_string(card->m_level))] = card;
        }
        else
        {
            card->m_name += "-" + to_string(card->m_level);
        }
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
                case CardType::num_cardtypes: {
                    throw card->m_type;
                    break;
                }
            }
            std::string simple_name{simplify_name(card->m_name)};
            auto card_itr = player_cards_by_name.find(simple_name);
            if (card_itr == player_cards_by_name.end())
            {
                player_cards_by_name[simple_name] = card;
            }
            else
            {
                // TODO check set visible
//                std::cerr << "Duplicated card name [" << card->m_name << "] " << card_itr->second->m_set << ":" << card->m_set << "\n"; // XXX
            }
        }
    }
#if 0 // TODO refactor precedence
    // Round 3: depend on player_cards_by_name; set abbreviations
    for(Card* card: cards)
    {
        // generate abbreviations
        if(card->m_set > 0)
        {
            for(auto&& abbr_name : get_abbreviations(card->m_name))
            {
                if(abbr_name.length() > 1 && player_cards_by_name.find(abbr_name) == player_cards_by_name.end())
                {
                    player_cards_abbr[abbr_name] = card->m_name;
                }
            }
        }
    }
#endif
}

// class Card
void Card::add_skill(Skill id, unsigned x, Faction y, unsigned n, unsigned c, Skill s, bool all)
{
    for(auto it = m_skills.begin(); it != m_skills.end(); ++ it)
    {
        if(it->id == id)
        {
            m_skills.erase(it);
            break;
        }
    }
    m_skills.push_back({id, x, y, n, c, s, all});
    m_skill_value[id] = x ? x : n ? n : 1;
}

