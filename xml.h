#ifndef XML_H_INCLUDED
#define XML_H_INCLUDED

#include <string>
#include "tyrant.h"

class Cards;
class Decks;
class Achievement;

Skill skill_name_to_id(const std::string & name);
void load_cards_xml(Cards & all_cards, const char * filename);
void load_decks_xml(Decks& decks, const Cards& all_cards, const char * mission_filename);
void load_recipes_xml(Cards& all_cards, const char * filename);
void read_missions(Decks& decks, const Cards& all_cards, std::string filename);
void read_raids(Decks& decks, const Cards& all_cards, std::string filename);

#endif
