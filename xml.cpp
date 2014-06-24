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
void load_decks_xml(Decks& decks, const Cards& all_cards)
{
    try
    {
        read_missions(decks, all_cards, "missions.xml");
    }
    catch(const rapidxml::parse_error& e)
    {
        std::cout << "\nException while loading decks from file missions.xml\n";
    }
#if not defined(TYRANT_UNLEASHED)
    try
    {
        read_raids(decks, all_cards, "raids.xml");
    }
    catch(const rapidxml::parse_error& e)
    {
        std::cout << "\nException while loading decks from file raids.xml\n";
    }
    try
    {
        read_quests(decks, all_cards, "quests.xml");
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
void parse_card_node(Cards& all_cards, Card* card, xml_node<>* card_node)
{
    xml_node<>* id_node(card_node->first_node("id"));
    xml_node<>* card_id_node = card_node->first_node("card_id");
    assert(id_node || card_id_node);
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
        else if (card->m_id < 4000)
        { card->m_type = CardType::action; }
        else
        { card->m_type = cost_node ? (attack_node ? CardType::assault : CardType::structure) : (health_node ? CardType::commander : CardType::action); }
    }
    if(hidden_node) { card->m_hidden = atoi(hidden_node->value()); }
    if(replace_node) { card->m_replace = atoi(replace_node->value()); }
    if(attack_node) { card->m_attack = atoi(attack_node->value()); }
    if(health_node) { card->m_health = atoi(health_node->value()); }
    if(cost_node) { card->m_delay = atoi(cost_node->value()); }
    if(unique_node) { card->m_unique = true; }
    if(reserve_node) { card->m_reserve = atoi(reserve_node->value()); }
    if(base_card_node) { card->m_base_id = atoi(base_card_node->value()); }
    if(rarity_node) { card->m_rarity = atoi(rarity_node->value()); }
    if(type_node) { card->m_faction = map_to_faction(atoi(type_node->value())); }
    card->m_set = set;

    if (card_node->first_node("skill"))
    { // inherit no skill if there is skill node
        for (unsigned mod = 0; mod < SkillMod::num_skill_activation_modifiers; ++ mod)
        {
            card->m_skills[mod].clear();
        }
        memset(card->m_skill_value, 0, sizeof card->m_skill_value);
    }
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

        auto x = node_value(skill_node, "x", 0);
        auto y = skill_faction(skill_node);
        auto c = node_value(skill_node, "c", 0);
        auto s = skill_target_skill(skill_node);

        if (played)   { card->add_skill(skill_id, x, y, c, s, all, SkillMod::on_play); }
        if (attacked) { card->add_skill(skill_id, x, y, c, s, all, SkillMod::on_attacked); }
        if (kill)     { card->add_skill(skill_id, x, y, c, s, all, SkillMod::on_kill); }
        if (died)     { card->add_skill(skill_id, x, y, c, s, all, SkillMod::on_death); }
        if (normal)   { card->add_skill(skill_id, x, y, c, s, all); }
    }
    all_cards.cards.push_back(card);
#if defined(TYRANT_UNLEASHED)
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
#endif
}

void read_cards(Cards& all_cards)
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
        parse_card_node(all_cards, card, card_node);
    }
    all_cards.organize();
#if 0
    std::cout << "nb cards: " << nb_cards << "\n";
    for(auto counts: sets_counts)
    {
        std::cout << "set " << counts.first << ": " << counts.second << "\n";
    }
#endif
}
//------------------------------------------------------------------------------
Deck* read_deck(Decks& decks, const Cards& all_cards, xml_node<>* node, const char* effect_node_name, DeckType::DeckType decktype, unsigned id, std::string base_deck_name, bool has_levels=false)
{
    xml_node<>* commander_node(node->first_node("commander"));
    unsigned card_id = atoi(commander_node->value());
    const Card* commander_card{all_cards.by_id(card_id)};
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
#if defined(TYRANT_UNLEASHED)
    if (has_levels)
    {
        for (unsigned level = 1; level <= 9; ++ level)
        {
            std::string deck_name = base_deck_name + "-" + to_string(level);
            decks.decks.push_back(Deck{all_cards, decktype, id, deck_name, effect, level - 1});
            Deck* deck = &decks.decks.back();
            deck->set(commander_card, always_cards, some_cards, reward_cards, mission_req);
            std::string alt_name = decktype_names[decktype] + " #" + to_string(id) + "-" + to_string(level);
            decks.by_name[deck_name] = deck;
            decks.by_name[alt_name] = deck;
        }
    }
#endif
    decks.decks.push_back(Deck{all_cards, decktype, id, base_deck_name, effect});
    Deck* deck = &decks.decks.back();
    deck->set(commander_card, always_cards, some_cards, reward_cards, mission_req);
#if defined(TYRANT_UNLEASHED)
    if (has_levels)
    { // upgrade cards in deck
        deck->commander = deck->commander->m_top_level_card;
        for (auto && card: deck->cards)
        { card = card->m_top_level_card; }
        for (auto && pool: deck->raid_cards)
        {
            for (auto && card: pool.second)
            { card = card->m_top_level_card; }
        }
    }
#endif
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
        read_deck(decks, all_cards, mission_node, "effect", DeckType::mission, id, deck_name, true);
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
void read_quests(Decks& decks, const Cards& all_cards, std::string filename)
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
        read_deck(decks, all_cards, quest_node, "battleground_id", DeckType::quest, id, deck_name);
    }
}

//------------------------------------------------------------------------------
void load_recipes_xml(Cards& all_cards)
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
        Card * card = all_cards.cards_by_id[card_id];
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

//------------------------------------------------------------------------------
Comparator get_comparator(xml_node<>* node, Comparator default_comparator)
{
    xml_attribute<>* compare(node->first_attribute("compare"));
    if(!compare) { return default_comparator; }
    else if(strcmp(compare->value(), "equal") == 0) { return equal; }
    else if(strcmp(compare->value(), "great_equal") == 0) { return great_equal; }
    else if(strcmp(compare->value(), "less_equal") == 0) { return less_equal; }
    else { throw std::runtime_error(std::string("Not implemented: compare=\"") + compare->value() + "\""); }
}

void read_achievement(Decks& decks, const Cards& all_cards, Achievement& achievement, const char* achievement_id_name, std::string filename/* = "achievements.xml"*/)
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
                std::cout << "  Play units: " << all_cards.by_id(atoi(unit_id->value()))->m_name << achievement.req_counter.back().str() << std::endl;
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
