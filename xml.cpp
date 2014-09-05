#include "xml.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include "rapidxml.hpp"
#include "card.h"
#include "cards.h"
#include "deck.h"
#include "tyrant.h"
//---------------------- $20 cards.xml parsing ---------------------------------
// Sets: 1 enclave; 2 nexus; 3 blight; 4 purity; 5 homeworld;
// 6 phobos; 7 phobos aftermath; 8 awakening
// 1000 standard; 5000 rewards; 5001 promotional; 9000 exclusive
// mission only and test cards have no set
using namespace rapidxml;

Faction map_to_faction(unsigned i)
{
    return(i == 1 ? imperial :
           i == 2 ? raider :
           i == 3 ? bloodthirsty :
           i == 4 ? xeno :
           i == 5 ? righteous :
           i == 6 ? progenitor :
           allfactions);
}

CardType::CardType map_to_type(unsigned i)
{
    return(i == 1 ? CardType::commander :
           i == 2 ? CardType::assault :
           i == 4 ? CardType::structure :
           CardType::num_cardtypes);
}

Skill skill_name_to_id(const std::string & name, bool do_warn)
{
    static std::map<std::string, int> skill_map;
    static std::set<std::string> unknown_skills;
    if(skill_map.empty())
    {
        for(unsigned i(0); i < Skill::num_skills; ++i)
        {
            std::string skill_id = boost::to_lower_copy(skill_names[i]);
            skill_map[skill_id] = i;
        }
        skill_map["armored"] = skill_map["armor"]; // Special case for Armor: id and name differ
    }
    auto x = skill_map.find(boost::to_lower_copy(name));
    if (x == skill_map.end())
    {
        if (do_warn and unknown_skills.count(name) == 0)
        { // Warn only once for each unknown skill
            unknown_skills.insert(name);
            std::cerr << "Warning: Ignore unknown skill [" << name << "] in data/cards.xml\n";
        }
        return no_skill;
    }
    else
    {
        return (Skill)x->second;
    }
}

Faction skill_faction(xml_node<>* skill)
{
    unsigned unmapped_faction(0);
    xml_attribute<>* y(skill->first_attribute("y"));
    if(y)
    {
        unmapped_faction = atoi(y->value());
    }
    return(unmapped_faction == 0 ? allfactions : map_to_faction(unmapped_faction));
}

unsigned node_value(xml_node<>* skill, const char* attribute, unsigned default_value = 0)
{
    xml_attribute<>* value_node(skill->first_attribute(attribute));
    return value_node ? atoi(value_node->value()) : default_value;
}

Skill skill_target_skill(xml_node<>* skill)
{
    Skill s(no_skill);
    xml_attribute<>* x(skill->first_attribute("s"));
    if(x)
    {
       s = skill_name_to_id(x->value());
    }
    return(s);
}

//------------------------------------------------------------------------------
void load_decks_xml(Decks& decks, const Cards& all_cards, const char * mission_filename, const char * raid_filename)
{
    try
    {
        read_missions(decks, all_cards, mission_filename);
    }
    catch (const rapidxml::parse_error& e)
    {
        std::cerr << "\nFailed to parse file [" << mission_filename << "]. Skip it.\n";
    }
    try
    {
        read_raids(decks, all_cards, raid_filename);
    }
    catch(const rapidxml::parse_error& e)
    {
        std::cerr << "\nFailed to parse file [" << raid_filename << "]. Skip it.\n";
    }
}

//------------------------------------------------------------------------------
void parse_file(const char* filename, std::vector<char>& buffer, xml_document<>& doc)
{
    std::ifstream cards_stream(filename, std::ios::binary);
    if(!cards_stream.good())
    {
        std::cout << "Warning: The file '" << filename << "' does not exist. Proceeding without reading from this file.\n";
        buffer.resize(1);
        buffer[0] = 0;
        doc.parse<0>(&buffer[0]);
        return;
    }
    // Get the size of the file
    cards_stream.seekg(0,std::ios::end);
    std::streampos length = cards_stream.tellg();
    cards_stream.seekg(0,std::ios::beg);
    buffer.resize(length + std::streampos(1));
    cards_stream.read(&buffer[0],length);
    // zero-terminate
    buffer[length] = '\0';
    try
    {
        doc.parse<0>(&buffer[0]);
    }
    catch(rapidxml::parse_error& e)
    {
        std::cerr << "Parse error exception.\n";
        std::cout << e.what();
        throw(e);
    }
}
//------------------------------------------------------------------------------
void parse_card_node(Cards& all_cards, Card* card, xml_node<>* card_node)
{
    xml_node<>* id_node(card_node->first_node("id"));
    xml_node<>* card_id_node = card_node->first_node("card_id");
    assert(id_node || card_id_node);
    xml_node<>* name_node(card_node->first_node("name"));
    xml_node<>* attack_node(card_node->first_node("attack"));
    xml_node<>* health_node(card_node->first_node("health"));
    xml_node<>* cost_node(card_node->first_node("cost"));
    xml_node<>* rarity_node(card_node->first_node("rarity"));
    xml_node<>* type_node(card_node->first_node("type"));
    xml_node<>* set_node(card_node->first_node("set"));
    int set(set_node ? atoi(set_node->value()) : card->m_set);
    xml_node<>* level_node(card_node->first_node("level"));
    if (id_node) { card->m_base_id = card->m_id = atoi(id_node->value()); }
    else if (card_id_node) { card->m_id = atoi(card_id_node->value()); }
    if (name_node) { card->m_name = name_node->value(); }
    if (level_node) { card->m_level = atoi(level_node->value()); }
    if (id_node)
    {
        if (card->m_id < 1000)
        { card->m_type = CardType::assault; }
        else if (card->m_id < 2000)
        { card->m_type = CardType::commander; }
        else if (card->m_id < 3000)
        { card->m_type = CardType::structure; }
#if 0
        else if (card->m_id < 4000)
        { card->m_type = CardType::action; }
#endif
        else if (card->m_id < 8000)
        { card->m_type = CardType::assault; }
        else if (card->m_id < 10000)
        { card->m_type = CardType::structure; }
        else
        { card->m_type = cost_node ? (attack_node ? CardType::assault : CardType::structure) : CardType::commander; }
#if 0
        { card->m_type = cost_node ? (attack_node ? CardType::assault : CardType::structure) : (health_node ? CardType::commander : CardType::action); }
#endif
    }
    if(attack_node) { card->m_attack = atoi(attack_node->value()); }
    if(health_node) { card->m_health = atoi(health_node->value()); }
    if(cost_node) { card->m_delay = atoi(cost_node->value()); }
    if(rarity_node) { card->m_rarity = atoi(rarity_node->value()); }
    if(type_node) { card->m_faction = map_to_faction(atoi(type_node->value())); }
    card->m_set = set;

    if (card_node->first_node("skill"))
    { // inherit no skill if there is skill node
        card->m_skills.clear();
        memset(card->m_skill_value, 0, sizeof card->m_skill_value);
    }
    for(xml_node<>* skill_node = card_node->first_node("skill");
            skill_node;
            skill_node = skill_node->next_sibling("skill"))
    {
        Skill skill_id = skill_name_to_id(skill_node->first_attribute("id")->value());
        if(skill_id == no_skill) { continue; }
        auto x = node_value(skill_node, "x", 0);
        auto y = skill_faction(skill_node);
        auto n = node_value(skill_node, "n", 0);
        auto c = node_value(skill_node, "c", 0);
        auto s = skill_target_skill(skill_node);
        bool all(skill_node->first_attribute("all"));
        card->add_skill(skill_id, x, y, n, c, s, all);
    }
    all_cards.cards.push_back(card);
    Card * top_card = card;
    for(xml_node<>* upgrade_node = card_node->first_node("upgrade");
            upgrade_node;
            upgrade_node = upgrade_node->next_sibling("upgrade"))
    {
        Card * pre_upgraded_card = top_card;
        top_card = new Card(*top_card);
        parse_card_node(all_cards, top_card, upgrade_node);
        if (top_card->m_type == CardType::commander)
        {
            // Commanders cost twice and cannot be salvaged.
            top_card->m_recipe_cost = 2 * upgrade_cost[pre_upgraded_card->m_level];
        }
        else
        {
            // Salvaging income counts?
            top_card->m_recipe_cost = upgrade_cost[pre_upgraded_card->m_level]; // + salvaging_income[top_card->m_rarity][pre_upgraded_card->m_level] - salvaging_income[top_card->m_rarity][top_card->m_level];
        }
        top_card->m_recipe_cards.clear();
        top_card->m_recipe_cards[pre_upgraded_card] = 1;
        pre_upgraded_card->m_used_for_cards[top_card] = 1;
    }
    card->m_top_level_card = top_card;
}

void load_cards_xml(Cards & all_cards, const char * filename)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file(filename, buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        return;
    }
    for(xml_node<>* card_node = root->first_node("unit");
        card_node;
        card_node = card_node->next_sibling("unit"))
    {
        auto card = new Card();
        parse_card_node(all_cards, card, card_node);
    }
    all_cards.organize();
}
//------------------------------------------------------------------------------
Deck* read_deck(Decks& decks, const Cards& all_cards, xml_node<>* node, const char* effect_node_name, DeckType::DeckType decktype, unsigned id, std::string base_deck_name)
{
    xml_node<>* commander_node(node->first_node("commander"));
    unsigned card_id = atoi(commander_node->value());
    const Card* commander_card{all_cards.by_id(card_id)};
    std::vector<const Card*> always_cards;
    std::vector<std::pair<unsigned, std::vector<const Card*>>> some_cards;
    std::vector<const Card*> reward_cards;
    xml_node<>* deck_node(node->first_node("deck"));
    xml_node<>* levels_node(node->first_node("levels"));
    unsigned max_level = levels_node ? atoi(levels_node->value()) : 10;
    xml_node<>* always_node{deck_node->first_node("always_include")};
    for(xml_node<>* card_node = (always_node ? always_node : deck_node)->first_node("card");
            card_node;
            card_node = card_node->next_sibling("card"))
    {
        card_id = atoi(card_node->value());
        always_cards.push_back(all_cards.by_id(card_id));
    }
    for(xml_node<>* pool_node = deck_node->first_node("card_pool");
            pool_node;
            pool_node = pool_node->next_sibling("card_pool"))
    {
        unsigned num_cards_from_pool(atoi(pool_node->first_attribute("amount")->value()));
        std::vector<const Card*> cards_from_pool;

        for(xml_node<>* card_node = pool_node->first_node("card");
                card_node;
                card_node = card_node->next_sibling("card"))
        {
            unsigned card_id(atoi(card_node->value()));
            cards_from_pool.push_back(all_cards.by_id(card_id));
        }
        some_cards.push_back(std::make_pair(num_cards_from_pool, cards_from_pool));
    }
    xml_node<>* rewards_node(node->first_node("rewards"));
    if(decktype == DeckType::mission && rewards_node)
    {
        for(xml_node<>* card_node = rewards_node->first_node("card");
                card_node;
                card_node = card_node->next_sibling("card"))
        {
            unsigned card_id(atoi(card_node->value()));
            reward_cards.push_back(all_cards.by_id(card_id));
        }
    }
    xml_node<>* mission_req_node(node->first_node(decktype == DeckType::mission ? "req" : "mission_req"));
    unsigned mission_req(mission_req_node ? atoi(mission_req_node->value()) : 0);
    xml_node<>* effect_id_node(node->first_node(effect_node_name));
    Effect effect = effect_id_node ? static_cast<enum Effect>(atoi(effect_id_node->value())) : Effect::none;

    for (unsigned level = 1; level < max_level; ++ level)
    {
        std::string deck_name = base_deck_name + "-" + to_string(level);
        decks.decks.push_back(Deck{all_cards, decktype, id, deck_name, effect, level - 1, max_level - 1});
        Deck* deck = &decks.decks.back();
        deck->set(commander_card, always_cards, some_cards, reward_cards, mission_req);
        std::string alt_name = decktype_names[decktype] + " #" + to_string(id) + "-" + to_string(level);
        decks.by_name[deck_name] = deck;
        decks.by_name[alt_name] = deck;
    }

    decks.decks.push_back(Deck{all_cards, decktype, id, base_deck_name, effect});
    Deck* deck = &decks.decks.back();
    deck->set(commander_card, always_cards, some_cards, reward_cards, mission_req);

    // upgrade cards for full-level missions/raids
    deck->commander = deck->commander->m_top_level_card;
    for (auto && card: deck->cards)
    { card = card->m_top_level_card; }
    for (auto && pool: deck->raid_cards)
    {
        for (auto && card: pool.second)
        { card = card->m_top_level_card; }
    }

    std::string alt_name = decktype_names[decktype] + " #" + to_string(id);
    decks.by_name[base_deck_name] = deck;
    decks.by_name[alt_name] = deck;
    decks.by_type_id[{decktype, id}] = deck;
    return deck;
}
//------------------------------------------------------------------------------
void read_missions(Decks& decks, const Cards& all_cards, std::string filename)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file(filename.c_str(), buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        return;
    }

    for(xml_node<>* mission_node = root->first_node("mission");
        mission_node;
        mission_node = mission_node->next_sibling("mission"))
    {
        std::vector<unsigned> card_ids;
        xml_node<>* id_node(mission_node->first_node("id"));
        assert(id_node);
        unsigned id(id_node ? atoi(id_node->value()) : 0);
        xml_node<>* name_node(mission_node->first_node("name"));
        std::string deck_name{name_node->value()};
        try
        {
            read_deck(decks, all_cards, mission_node, "effect", DeckType::mission, id, deck_name);
        }
        catch (const std::runtime_error& e)
        {
            std::cerr << "Warning: Failed to parse mission [" << deck_name << "] in file " << filename << ": [" << e.what() << "]. Skip the mission.\n";
            continue;
        }
    }
}
//------------------------------------------------------------------------------
void read_raids(Decks& decks, const Cards& all_cards, std::string filename)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file(filename.c_str(), buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        return;
    }

    for(xml_node<>* raid_node = root->first_node("raid");
        raid_node;
        raid_node = raid_node->next_sibling("raid"))
    {
        xml_node<>* id_node(raid_node->first_node("id"));
        assert(id_node);
        unsigned id(id_node ? atoi(id_node->value()) : 0);
        xml_node<>* name_node(raid_node->first_node("name"));
        std::string deck_name{name_node->value()};
        read_deck(decks, all_cards, raid_node, "effect", DeckType::raid, id, deck_name);
    }
}

//------------------------------------------------------------------------------
void load_recipes_xml(Cards& all_cards, const char * filename)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file(filename, buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        return;
    }

    for(xml_node<>* recipe_node = root->first_node("fusion_recipe");
        recipe_node;
        recipe_node = recipe_node->next_sibling("fusion_recipe"))
    {
        xml_node<>* card_id_node(recipe_node->first_node("card_id"));
        if (!card_id_node) { continue; }
        unsigned card_id(atoi(card_id_node->value()));
        Card * card = all_cards.cards_by_id[card_id];
        if (!card) {
            std::cerr << "Could not find card by id " << card_id << std::endl;
            continue;
        }

        for(xml_node<>* resource_node = recipe_node->first_node("resource");
                resource_node;
                resource_node = resource_node->next_sibling("resource"))
        {
            unsigned card_id(node_value(resource_node, "card_id"));
            unsigned number(node_value(resource_node, "number"));
            if (card_id == 0 || number == 0) { continue; }
            Card * material_card = all_cards.cards_by_id[card_id];
            card->m_recipe_cards[material_card] += number;
            material_card->m_used_for_cards[card] += number;
        }
    }
}

