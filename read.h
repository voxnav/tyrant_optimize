#ifndef READ_H_INCLUDED
#define READ_H_INCLUDED

#include <map>
#include <string>

#include "deck.h"

class Cards;
class Decks;
class Deck;

DeckList parse_deck_list(std::string list_string, Decks& decks);
void parse_card_spec(const Cards& cards, const std::string& card_spec, unsigned& card_id, unsigned& card_num, char& num_sign, char& mark);
const std::pair<std::vector<unsigned>, std::map<signed, char>> string_to_ids(const Cards& all_cards, const std::string& deck_string, const std::string & description);
unsigned load_custom_decks(Decks& decks, Cards& cards, const std::string & filename);
void read_owned_cards(Cards& cards, std::map<unsigned, unsigned>& owned_cards, const std::string & filename);
unsigned read_card_abbrs(Cards& cards, const std::string& filename);
unsigned read_bge_aliases(std::unordered_map<std::string, std::string> & bge_aliases, const std::string & filename);

#endif
