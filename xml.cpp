#include "xml.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <algorithm>
#include "rapidxml.hpp"
#include "card.h"
#include "cards.h"
#include "deck.h"
#include "achievement.h"
#include "tyrant.h"
//---------------------- $20 cards.xml parsing ---------------------------------
// Sets: 1 enclave; 2 nexus; 3 blight; 4 purity; 5 homeworld;
// 6 phobos; 7 phobos aftermath; 8 awakening
// 1000 standard; 5000 rewards; 5001 promotional; 9000 exclusive
// mission only and test cards have no set
using namespace rapidxml;

Faction map_to_faction(unsigned i)
{
#if defined(TYRANT_UNLEASHED)
    return(i == 1 ? imperial :
           i == 2 ? raider :
           i == 3 ? bloodthirsty :
           i == 4 ? xeno :
           i == 5 ? righteous :
           i == 6 ? progenitor :
           allfactions);
#else
    return(i == 1 ? imperial :
           i == 9 ? raider :
           i == 3 ? bloodthirsty :
           i == 4 ? xeno :
           i == 8 ? righteous :
           allfactions);
#endif
}

CardType::CardType map_to_type(unsigned i)
{
    return(i == 1 ? CardType::commander :
           i == 2 ? CardType::assault :
           i == 4 ? CardType::structure :
           i == 8 ? CardType::action :
           CardType::num_cardtypes);
}

Skill skill_name_to_id(const char* name)
{
    static std::map<std::string, int> skill_map;
    if(skill_map.empty())
    {
        for(unsigned i(0); i < Skill::num_skills; ++i)
        {
            std::string skill_id{skill_names[i]};
            std::transform(skill_id.begin(), skill_id.end(), skill_id.begin(), ::tolower);
            skill_map[skill_id] = i;
        }
    }
    auto x = skill_map.find(name);
    return x == skill_map.end() ? no_skill : (Skill)x->second;
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

unsigned node_value(xml_node<>* skill, const char* attribute)
{
    xml_attribute<>* value_node(skill->first_attribute(attribute));
    return value_node ? atoi(value_node->value()) : 0;
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
void load_decks_xml(Decks& decks, const Cards& cards)
{
    try
    {
        read_missions(decks, cards, "missions.xml");
    }
    catch(const rapidxml::parse_error& e)
    {
        std::cout << "\nException while loading decks from file missions.xml\n";
    }
#if not defined(TYRANT_UNLEASHED)
    try
    {
        read_raids(decks, cards, "raids.xml");
    }
    catch(const rapidxml::parse_error& e)
    {
        std::cout << "\nException while loading decks from file raids.xml\n";
    }
    try
    {
        read_quests(decks, cards, "quests.xml");
    }
    catch(const rapidxml::parse_error& e)
    {
        std::cout << "\nException while loading decks from file quests.xml\n";
    }
#endif
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
void parse_card_node(Cards& cards, Card* card, xml_node<>* card_node)
{
    xml_node<>* id_node(card_node->first_node("id"));
    if(!id_node)
    {
        id_node = card_node->first_node("card_id");
    }
    assert(id_node);
    unsigned id(id_node ? atoi(id_node->value()) : 0);
    xml_node<>* name_node(card_node->first_node("name"));
    xml_node<>* hidden_node(card_node->first_node("hidden"));
    xml_node<>* replace_node(card_node->first_node("replace"));
    xml_node<>* attack_node(card_node->first_node("attack"));
    xml_node<>* health_node(card_node->first_node("health"));
    xml_node<>* cost_node(card_node->first_node("cost"));
    xml_node<>* unique_node(card_node->first_node("unique"));
    xml_node<>* reserve_node(card_node->first_node("reserve"));
    xml_node<>* base_card_node(card_node->first_node("base_card"));
    xml_node<>* rarity_node(card_node->first_node("rarity"));
    xml_node<>* type_node(card_node->first_node("type"));
    xml_node<>* set_node(card_node->first_node("set"));
    int set(set_node ? atoi(set_node->value()) : card->m_set);
    xml_node<>* level_node(card_node->first_node("level"));
#if 0
    if (set > 0)  // not AI only
    {
        nb_cards++;
        sets_counts[set]++;
    }
#endif
    card->m_id = id;
    if(name_node) { card->m_name = name_node->value(); }
    if(level_node) { card->m_level = atoi(level_node->value()); }
    // So far, commanders have attack_node (value == 0)
    if(id < 1000)
    { card->m_type = CardType::assault; }
    else if(id < 2000)
    { card->m_type = CardType::commander; }
    else if(id < 3000)
    { card->m_type = CardType::structure; }
    else if(id < 4000)
    { card->m_type = CardType::action; }
    else
    { card->m_type = CardType::assault; }
    if(hidden_node) { card->m_hidden = atoi(hidden_node->value()); }
    if(replace_node) { card->m_replace = atoi(replace_node->value()); }
    if(attack_node) { card->m_attack = atoi(attack_node->value()); }
    if(health_node) { card->m_health = atoi(health_node->value()); }
    if(cost_node) { card->m_delay = atoi(cost_node->value()); }
    if(unique_node) { card->m_unique = true; }
    if(reserve_node) { card->m_reserve = atoi(reserve_node->value()); }
    if(base_card_node) { card->m_base_id = atoi(base_card_node->value()); }
    else if(card->m_base_id == 0) { card->m_base_id = card->m_id; }
    if(rarity_node) { card->m_rarity = atoi(rarity_node->value()); }
    if(type_node) { card->m_faction = map_to_faction(atoi(type_node->value())); }
    card->m_set = set;
    unsigned skill_pos = 1;
    for(xml_node<>* skill_node = card_node->first_node("skill");
            skill_node;
            skill_node = skill_node->next_sibling("skill"))
    {
        Skill skill_id = skill_name_to_id(skill_node->first_attribute("id")->value());
        if(skill_id == no_skill) { continue; }

        bool all(skill_node->first_attribute("all"));
        bool played(skill_node->first_attribute("played"));
        bool attacked(skill_node->first_attribute("attacked"));
        bool kill(skill_node->first_attribute("kill"));
        bool died(skill_node->first_attribute("died"));
        bool normal(!(played || died || attacked || kill));

        if(normal) { card->m_skill_pos[skill_id] = skill_pos; }

        if(skill_id == antiair)
        { card->m_antiair = node_value(skill_node, "x"); }
        else if(skill_id == armored)
        { card->m_armored = node_value(skill_node, "x"); }
        else if(skill_id == berserk)
        {
            if(attacked) { card->m_berserk_oa = node_value(skill_node, "x"); }
            else {card->m_berserk = node_value(skill_node, "x"); }
        }
        else if(skill_id == blitz)
        { card->m_blitz = true; }
        else if(skill_id == burst)
        { card->m_burst = node_value(skill_node, "x"); }
        else if(skill_id == corrosive)
        { card->m_corrosive = node_value(skill_node, "x"); }
        else if(skill_id == counter)
        { card->m_counter = node_value(skill_node, "x"); }
        else if(skill_id == crush)
        { card->m_crush = node_value(skill_node, "x"); }
        else if(skill_id == disease)
        {
            if(attacked) { card->m_disease_oa = true; }
            else {card->m_disease = true; }
        }
        else if(skill_id == emulate)
        { card->m_emulate = true; }
        else if(skill_id == evade)
#if defined(TYRANT_UNLEASHED)
        { card->m_evade = node_value(skill_node, "x"); }
#else
        { card->m_evade = 1; }
#endif
        else if(skill_id == fear)
        { card->m_fear = true; }
        else if(skill_id == flurry)
        { card->m_flurry = node_value(skill_node, "x"); }
        else if(skill_id == flying)
        { card->m_flying = true; }
        else if(skill_id == fusion)
        { card->m_fusion = true; }
        else if(skill_id == immobilize)
        { card->m_immobilize = true; }
        else if(skill_id == inhibit)
        { card->m_inhibit = node_value(skill_node, "x"); }
        else if(skill_id == intercept)
        { card->m_intercept = true; }
        else if(skill_id == leech)
        { card->m_leech = node_value(skill_node, "x"); }
        else if(skill_id == legion)
        { card->m_legion = node_value(skill_node, "x"); }
        else if(skill_id == payback)
        { card->m_payback = true; }
        else if(skill_id == pierce)
        { card->m_pierce = node_value(skill_node, "x"); }
        else if(skill_id == phase)
        { card->m_phase = true; }
        else if(skill_id == poison)
        {
            if(attacked) { card->m_poison_oa = node_value(skill_node, "x"); }
            else {card->m_poison = node_value(skill_node, "x"); }
        }
        else if(skill_id == refresh)
        { card->m_refresh = true; }
        else if(skill_id == regenerate)
        { card->m_regenerate = node_value(skill_node, "x"); }
        else if(skill_id == siphon)
        { card->m_siphon = node_value(skill_node, "x"); }
        else if(skill_id == stun)
        { card->m_stun = true; }
        else if(skill_id == sunder)
        {
            if(attacked) { card->m_sunder_oa = true; }
            else {card->m_sunder = true; }
        }
        else if(skill_id == swipe)
        { card->m_swipe = true; }
        else if(skill_id == tribute)
        { card->m_tribute = true; }
        else if(skill_id == valor)
        { card->m_valor = node_value(skill_node, "x"); }
        else if(skill_id == wall)
        { card->m_wall = true; }
        else
        {
            if(played) { card->add_skill(skill_id, node_value(skill_node, "x"), skill_faction(skill_node), node_value(skill_node, "c"), skill_target_skill(skill_node), all, SkillMod::on_play); }
            if(attacked) { card->add_skill(skill_id, node_value(skill_node, "x"), skill_faction(skill_node), node_value(skill_node, "c"), skill_target_skill(skill_node), all, SkillMod::on_attacked); }
            if(kill) { card->add_skill(skill_id, node_value(skill_node, "x"), skill_faction(skill_node), node_value(skill_node, "c"), skill_target_skill(skill_node), all, SkillMod::on_kill); }
            if(died) { card->add_skill(skill_id, node_value(skill_node, "x"), skill_faction(skill_node), node_value(skill_node, "c"), skill_target_skill(skill_node), all, SkillMod::on_death); }
            if(normal) { card->add_skill(skill_id, node_value(skill_node, "x"), skill_faction(skill_node), node_value(skill_node, "c"), skill_target_skill(skill_node), all); }
        }
        ++ skill_pos;
    }
    cards.cards.push_back(card);
#if defined(TYRANT_UNLEASHED)
    Card * top_card = card;
    for(xml_node<>* upgrade_node = card_node->first_node("upgrade");
            upgrade_node;
            upgrade_node = upgrade_node->next_sibling("upgrade"))
    {
        Card * pre_upgraded_card = top_card;
        top_card = new Card(*top_card);
        parse_card_node(cards, top_card, upgrade_node);
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
    card->m_final_id = top_card->m_id;
#endif
}

void read_cards(Cards& cards)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file("cards.xml", buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        return;
    }
#if 0
    unsigned nb_cards(0);
    std::map<unsigned, unsigned> sets_counts;
#endif
    for(xml_node<>* card_node = root->first_node("unit");
        card_node;
        card_node = card_node->next_sibling("unit"))
    {
        auto card = new Card();
        parse_card_node(cards, card, card_node);
    }
    cards.organize();
#if 0
    std::cout << "nb cards: " << nb_cards << "\n";
    for(auto counts: sets_counts)
    {
        std::cout << "set " << counts.first << ": " << counts.second << "\n";
    }
#endif
}
//------------------------------------------------------------------------------
Deck* read_deck(Decks& decks, const Cards& cards, xml_node<>* node, DeckType::DeckType decktype, unsigned id, std::string deck_name, unsigned level=1)
{
    xml_node<>* commander_node(node->first_node("commander"));
    unsigned card_id = atoi(commander_node->value());
#if defined(TYRANT_UNLEASHED)
    if(level == 10) { card_id = cards.by_id(cards.by_id(card_id)->m_base_id)->m_final_id; }
#endif
    const Card* commander_card{cards.by_id(card_id)};
    std::vector<const Card*> always_cards;
    std::vector<std::pair<unsigned, std::vector<const Card*>>> some_cards;
    std::vector<const Card*> reward_cards;
    xml_node<>* deck_node(node->first_node("deck"));
    xml_node<>* always_node{deck_node->first_node("always_include")};
    for(xml_node<>* card_node = (always_node ? always_node : deck_node)->first_node("card");
            card_node;
            card_node = card_node->next_sibling("card"))
    {
        card_id = atoi(card_node->value());
#if defined(TYRANT_UNLEASHED)
        if(level == 10) { card_id = cards.by_id(cards.by_id(card_id)->m_base_id)->m_final_id; }
#endif
        always_cards.push_back(cards.by_id(card_id));
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
            cards_from_pool.push_back(cards.by_id(card_id));
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
            reward_cards.push_back(cards.by_id(card_id));
        }
    }
    xml_node<>* mission_req_node(node->first_node(decktype == DeckType::mission ? "req" : "mission_req"));
    unsigned mission_req(mission_req_node ? atoi(mission_req_node->value()) : 0);
#if defined(TYRANT_UNLEASHED)
    if (level < 10) { deck_name += "-" + to_string(level); }
#endif
    decks.decks.push_back(Deck{decktype, id, deck_name});
    Deck* deck = &decks.decks.back();
    deck->set(commander_card, always_cards, some_cards, reward_cards, mission_req);
    decks.by_type_id[{decktype, id}] = deck;
    decks.by_name[deck_name] = deck;
    std::stringstream alt_name;
    alt_name << decktype_names[decktype] << " #" << id;
#if defined(TYRANT_UNLEASHED)
    if (level < 10) { alt_name << "-" << level; }
#endif
    decks.by_name[alt_name.str()] = deck;
    return deck;
}
//------------------------------------------------------------------------------
void read_missions(Decks& decks, const Cards& cards, std::string filename)
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
        Deck* deck;
#if defined(TYRANT_UNLEASHED)
        {
            deck = read_deck(decks, cards, mission_node, DeckType::mission, id, deck_name, 1);
        }
#endif
        deck = read_deck(decks, cards, mission_node, DeckType::mission, id, deck_name, 10);
        xml_node<>* effect_id_node(mission_node->first_node("effect"));
        if(effect_id_node)
        {
            int effect_id(effect_id_node ? atoi(effect_id_node->value()) : 0);
            deck->effect = static_cast<enum Effect>(effect_id);
        }
    }
}
//------------------------------------------------------------------------------
void read_raids(Decks& decks, const Cards& cards, std::string filename)
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
        Deck* deck = read_deck(decks, cards, raid_node, DeckType::raid, id, deck_name);
        xml_node<>* effect_id_node(raid_node->first_node("effect"));
        if(effect_id_node)
        {
            int effect_id(effect_id_node ? atoi(effect_id_node->value()) : 0);
            deck->effect = static_cast<enum Effect>(effect_id);
        }
    }
}
//------------------------------------------------------------------------------
void read_quests(Decks& decks, const Cards& cards, std::string filename)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file(filename.c_str(), buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        return;
    }

    // Seems always_cards is empty for all quests.
    std::vector<const Card*> always_cards;

    for(xml_node<>* quest_node = root->first_node("step");
        quest_node;
        quest_node = quest_node->next_sibling("step"))
    {
        xml_node<>* id_node(quest_node->first_node("id"));
        assert(id_node);
        unsigned id(id_node ? atoi(id_node->value()) : 0);
        std::string deck_name{"Step " + std::string{id_node->value()}};
        Deck* deck = read_deck(decks, cards, quest_node, DeckType::quest, id, deck_name);
        xml_node<>* effect_id_node(quest_node->first_node("battleground_id"));
        int effect_id(effect_id_node ? atoi(effect_id_node->value()) : 0);
        deck->effect = static_cast<enum Effect>(effect_id);
    }
}

//------------------------------------------------------------------------------
void load_recipes_xml(Cards& cards)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file("fusion_recipes_cj2.xml", buffer, doc);
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
        Card * card = cards.cards_by_id[card_id];
        for(xml_node<>* resource_node = recipe_node->first_node("resource");
                resource_node;
                resource_node = resource_node->next_sibling("resource"))
        {
            unsigned card_id(node_value(resource_node, "card_id"));
            unsigned number(node_value(resource_node, "number"));
            if (card_id == 0 || number == 0) { continue; }
            Card * material_card = cards.cards_by_id[card_id];
            card->m_recipe_cards[material_card] += number;
            material_card->m_used_for_cards[card] += number;
        }
    }
}

//------------------------------------------------------------------------------
extern unsigned turn_limit;
Comparator get_comparator(xml_node<>* node, Comparator default_comparator)
{
    xml_attribute<>* compare(node->first_attribute("compare"));
    if(!compare) { return default_comparator; }
    else if(strcmp(compare->value(), "equal") == 0) { return equal; }
    else if(strcmp(compare->value(), "great_equal") == 0) { return great_equal; }
    else if(strcmp(compare->value(), "less_equal") == 0) { return less_equal; }
    else { throw std::runtime_error(std::string("Not implemented: compare=\"") + compare->value() + "\""); }
}

void read_achievement(Decks& decks, const Cards& cards, Achievement& achievement, const char* achievement_id_name, std::string filename/* = "achievements.xml"*/)
{
    std::vector<char> buffer;
    xml_document<> doc;
    parse_file(filename.c_str(), buffer, doc);
    xml_node<>* root = doc.first_node();

    if(!root)
    {
        throw std::runtime_error("Failed to parse " + filename);
    }

    for(xml_node<>* achievement_node = root->first_node("achievement");
        achievement_node;
        achievement_node = achievement_node->next_sibling("achievement"))
    {
        xml_node<>* id_node(achievement_node->first_node("id"));
        xml_node<>* name_node(achievement_node->first_node("name"));
        if(!id_node || !name_node || (strcmp(id_node->value(), achievement_id_name) != 0 && strcmp(name_node->value(), achievement_id_name) != 0)) { continue; }
        achievement.id = atoi(id_node->value());
        achievement.name = name_node->value();
        std::cout << "Achievement " << id_node->value() << " " << name_node->value() << ": " << achievement_node->first_node("desc")->value() << std::endl;
        xml_node<>* type_node(achievement_node->first_node("type"));
        xml_attribute<>* mission_id(type_node ? type_node->first_attribute("mission_id") : NULL);
        if(!type_node || !mission_id)
        {
            throw std::runtime_error("Must be 'mission' type.");
        }
        assert(strcmp(type_node->first_attribute("winner")->value(), "1") == 0);
        if(strcmp(mission_id->value(), "*") != 0)
        {
            achievement.mission_condition.init(atoi(mission_id->value()), get_comparator(type_node, equal));
            std::cout << "  Mission" << achievement.mission_condition.str() << " (" << decks.by_type_id[{DeckType::mission, atoi(mission_id->value())}]->name << ") and win" << std::endl;
        }
        for (xml_node<>* req_node = achievement_node->first_node("req");
            req_node;
            req_node = req_node->next_sibling("req"))
        {
            Comparator comparator = get_comparator(req_node, great_equal);
            xml_attribute<>* skill_id_node(req_node->first_attribute("skill_id"));
            xml_attribute<>* unit_id(req_node->first_attribute("unit_id"));
            xml_attribute<>* unit_type(req_node->first_attribute("unit_type"));
            xml_attribute<>* unit_race(req_node->first_attribute("unit_race"));
            xml_attribute<>* unit_rarity(req_node->first_attribute("unit_rarity"));
            xml_attribute<>* num_turns(req_node->first_attribute("num_turns"));
            xml_attribute<>* num_used(req_node->first_attribute("num_used"));
            xml_attribute<>* num_played(req_node->first_attribute("num_played"));
            xml_attribute<>* num_killed(req_node->first_attribute("num_killed"));
            xml_attribute<>* num_killed_with(req_node->first_attribute("num_killed_with"));
            xml_attribute<>* damage(req_node->first_attribute("damage"));
            xml_attribute<>* com_total(req_node->first_attribute("com_total"));
            xml_attribute<>* only(req_node->first_attribute("only"));
            if(skill_id_node && num_used)
            {
                auto skill_id = skill_name_to_id(skill_id_node->value());
                if(skill_id == no_skill)
                {
                    throw std::runtime_error(std::string("Unknown skill ") + skill_id_node->value());
                }
                achievement.skill_used[skill_id] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_used->value()), comparator);
                std::cout << "  Use skills: " << skill_id_node->value() << achievement.req_counter.back().str() << std::endl;
            }
            else if(unit_id && num_played)
            {
                achievement.unit_played[atoi(unit_id->value())] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_played->value()), comparator);
                std::cout << "  Play units: " << cards.by_id(atoi(unit_id->value()))->m_name << achievement.req_counter.back().str() << std::endl;
            }
            else if(unit_type && num_played)
            {
                auto i = map_to_type(atoi(unit_type->value()));
                if(i == CardType::num_cardtypes)
                {
                    throw std::runtime_error(std::string("Unknown unit_type ") + unit_type->value());
                }
                achievement.unit_type_played[i] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_played->value()), comparator);
                std::cout << "  Play units of type: " << cardtype_names[i] << achievement.req_counter.back().str() << std::endl;
            }
            else if(unit_race && num_played)
            {
                auto i = map_to_faction(atoi(unit_race->value()));
                if(i == Faction::allfactions)
                {
                    throw std::runtime_error(std::string("Unknown unit_race ") + unit_race->value());
                }
                achievement.unit_faction_played[i] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_played->value()), comparator);
                std::cout << "  Play units of race (faction): " << faction_names[i] << achievement.req_counter.back().str() << std::endl;
            }
            else if(unit_rarity && num_played)
            {
                achievement.unit_rarity_played[atoi(unit_rarity->value())] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_played->value()), comparator);
                std::cout << "  Play units of rarity: " << rarity_names[atoi(unit_rarity->value())] << achievement.req_counter.back().str() << std::endl;
            }
            else if(unit_type && num_killed)
            {
                auto i = map_to_type(atoi(unit_type->value()));
                if(i == CardType::num_cardtypes)
                {
                    throw std::runtime_error(std::string("Unknown unit_type ") + unit_type->value());
                }
                achievement.unit_type_killed[i] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_killed->value()), comparator);
                std::cout << "  Kill units of type: " << cardtype_names[i] << achievement.req_counter.back().str() << std::endl;
            }
            else if(num_killed_with && skill_id_node && strcmp(skill_id_node->value(), "flying") == 0)
            {
                achievement.misc_req[AchievementMiscReq::unit_with_flying_killed] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_killed_with->value()), comparator);
                std::cout << "  " << achievement_misc_req_names[AchievementMiscReq::unit_with_flying_killed] << achievement.req_counter.back().str() << std::endl;
            }
            else if(only && skill_id_node && strcmp(skill_id_node->value(), "0") == 0)
            {
                achievement.misc_req[AchievementMiscReq::skill_activated] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(0, equal);
                std::cout << "  " << achievement_misc_req_names[AchievementMiscReq::skill_activated] << achievement.req_counter.back().str() << std::endl;
            }
            else if(num_turns)
            {
                achievement.misc_req[AchievementMiscReq::turns] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(num_turns->value()), comparator);
                std::cout << "  " << achievement_misc_req_names[AchievementMiscReq::turns] << achievement.req_counter.back().str() << std::endl;
            }
            else if(damage)
            {
                achievement.misc_req[AchievementMiscReq::damage] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(damage->value()), comparator);
                std::cout << "  " << achievement_misc_req_names[AchievementMiscReq::damage] << achievement.req_counter.back().str() << std::endl;
            }
            else if(com_total)
            {
                achievement.misc_req[AchievementMiscReq::com_total] = achievement.req_counter.size();
                achievement.req_counter.emplace_back(atoi(com_total->value()), comparator);
                std::cout << "  " << achievement_misc_req_names[AchievementMiscReq::com_total] << achievement.req_counter.back().str() << std::endl;
            }
            else
            {
                throw std::runtime_error("Not implemented.");
            }
        }
        return;
    }
    throw std::runtime_error("No such achievement.");
}
