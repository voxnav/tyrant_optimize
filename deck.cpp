#include "deck.h"

#include <boost/range/algorithm_ext/insert.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "card.h"
#include "cards.h"
#include "read.h"

template<class RandomAccessIterator, class UniformRandomNumberGenerator>
void partial_shuffle(RandomAccessIterator first, RandomAccessIterator middle,
                     RandomAccessIterator last,
                     UniformRandomNumberGenerator&& g)
{
    typedef typename std::iterator_traits<RandomAccessIterator>::difference_type diff_t;
    typedef typename std::make_unsigned<diff_t>::type udiff_t;
    typedef typename std::uniform_int_distribution<udiff_t> distr_t;
    typedef typename distr_t::param_type param_t;

    distr_t D;
    diff_t m = middle - first;
    diff_t n = last - first;
    for (diff_t i = 0; i < m; ++i)
    {
        std::swap(first[i], first[D(g, param_t(i, n-1))]);
    }
}

//------------------------------------------------------------------------------
const char* base64_chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789+/";
const char* wmt_b64_magic_chars = "-.~!*";

// Converts cards in `hash' to a deck.
// Stores resulting card IDs in `ids'.
void hash_to_ids_wmt_b64(const char* hash, std::vector<unsigned>& ids)
{
    unsigned int last_id = 0;
    const char* pc = hash;

    while(*pc)
    {
        unsigned id_plus = 0;
        const char* pmagic = strchr(wmt_b64_magic_chars, *pc);
        if(pmagic)
        {
            ++ pc;
            id_plus = 4000 * (pmagic - wmt_b64_magic_chars + 1);
        }
        if(!*pc || !*(pc + 1))
        {
            throw std::runtime_error("Invalid hash length");
        }
        const char* p0 = strchr(base64_chars, *pc);
        const char* p1 = strchr(base64_chars, *(pc + 1));
        if (!p0 || !p1)
        {
            throw std::runtime_error("Invalid hash character");
        }
        pc += 2;
        size_t index0 = p0 - base64_chars;
        size_t index1 = p1 - base64_chars;
        unsigned int id = (index0 << 6) + index1;

        if (id < 4001)
        {
            id += id_plus;
            ids.push_back(id);
            last_id = id;
        }
        else for (unsigned int j = 0; j < id - 4001; ++j)
        {
            ids.push_back(last_id);
        }
    }
}

void encode_id_wmt_b64(std::stringstream &ios, unsigned card_id)
{
    if(card_id > 4000)
    {
        ios << wmt_b64_magic_chars[(card_id - 1) / 4000 - 1];
        card_id = (card_id - 1) % 4000 + 1;
    }
    ios << base64_chars[card_id / 64];
    ios << base64_chars[card_id % 64];
}

void encode_deck_wmt_b64(std::stringstream &ios, const Card* commander, std::vector<const Card*> cards)
{
    if (commander)
    {
        encode_id_wmt_b64(ios, commander->m_id);
    }
    unsigned last_id = 0;
    unsigned num_repeat = 0;
    for(const Card* card: cards)
    {
        auto card_id = card->m_id;
        if(card_id == last_id)
        {
            ++ num_repeat;
        }
        else
        {
            if(num_repeat > 1)
            {
                ios << base64_chars[(num_repeat + 4000) / 64];
                ios << base64_chars[(num_repeat + 4000) % 64];
            }
            last_id = card_id;
            num_repeat = 1;
            encode_id_wmt_b64(ios, card_id);
        }
    }
    if(num_repeat > 1)
    {
        ios << base64_chars[(num_repeat + 4000) / 64];
        ios << base64_chars[(num_repeat + 4000) % 64];
    }
}

void hash_to_ids_ext_b64(const char* hash, std::vector<unsigned>& ids)
{
    const char* pc = hash;
    while (*pc)
    {
        unsigned id = 0;
        unsigned factor = 1;
        const char* p = strchr(base64_chars, *pc);
        if (!p)
        { throw std::runtime_error("Invalid hash character"); }
        size_t d = p - base64_chars;
        while (d < 32)
        {
            id += factor * d;
            factor *= 32;
            ++ pc;
            p = strchr(base64_chars, *pc);
            if (!p)
            { throw std::runtime_error("Invalid hash character"); }
            d = p - base64_chars;
        }
        id += factor * (d - 32);
        ++ pc;
        ids.push_back(id);
    }
}

void encode_id_ext_b64(std::stringstream &ios, unsigned card_id)
{
    while (card_id >= 32)
    {
        ios << base64_chars[card_id % 32];
        card_id /= 32;
    }
    ios << base64_chars[card_id + 32];
}

void encode_deck_ext_b64(std::stringstream &ios, const Card* commander, std::vector<const Card*> cards)
{
    if (commander)
    {
        encode_id_ext_b64(ios, commander->m_id);
    }
    for (const Card* card: cards)
    {
        encode_id_ext_b64(ios, card->m_id);
    }
}

void hash_to_ids_ddd_b64(const char* hash, std::vector<unsigned>& ids)
{
    const char* pc = hash;
    while(*pc)
    {
        if(!*pc || !*(pc + 1) || !*(pc + 2))
        {
            throw std::runtime_error("Invalid hash length");
        }
        const char* p0 = strchr(base64_chars, *pc);
        const char* p1 = strchr(base64_chars, *(pc + 1));
        const char* p2 = strchr(base64_chars, *(pc + 2));
        if (!p0 || !p1 || !p2)
        {
            throw std::runtime_error("Invalid hash character");
        }
        pc += 3;
        size_t index0 = p0 - base64_chars;
        size_t index1 = p1 - base64_chars;
        size_t index2 = p2 - base64_chars;
        unsigned int id = (index0 << 12) + (index1 << 6) + index2;
        ids.push_back(id);
    }
}

void encode_id_ddd_b64(std::stringstream &ios, unsigned card_id)
{
    ios << base64_chars[card_id / 4096];
    ios << base64_chars[card_id % 4096 / 64];
    ios << base64_chars[card_id % 64];
}

void encode_deck_ddd_b64(std::stringstream &ios, const Card* commander, std::vector<const Card*> cards)
{
    if (commander)
    {
        encode_id_ddd_b64(ios, commander->m_id);
    }
    for (const Card* card: cards)
    {
        encode_id_ddd_b64(ios, card->m_id);
    }
}

DeckDecoder hash_to_ids = hash_to_ids_ext_b64;
DeckEncoder encode_deck = encode_deck_ext_b64;

const std::pair<std::vector<unsigned>, std::map<signed, char>> string_to_ids(const Cards& all_cards, const std::string& deck_string, const std::string & description)
{
    std::vector<unsigned> card_ids;
    std::map<signed, char> card_marks;
    std::vector<std::string> error_list;
    boost::tokenizer<boost::char_delimiters_separator<char>> deck_tokens{deck_string, boost::char_delimiters_separator<char>{false, ":,", ""}};
    auto token_iter = deck_tokens.begin();
    signed p = -1;
    for(; token_iter != deck_tokens.end(); ++token_iter)
    {
        std::string card_spec(*token_iter);
        unsigned card_id{0};
        unsigned card_num{1};
        char num_sign{0};
        char mark{0};
        try
        {
            parse_card_spec(all_cards, card_spec, card_id, card_num, num_sign, mark);
            assert(num_sign == 0);
            for(unsigned i(0); i < card_num; ++i)
            {
                card_ids.push_back(card_id);
                if(mark) { card_marks[p] = mark; }
                ++ p;
            }
        }
        catch(std::exception& e)
        {
            error_list.push_back(e.what());
            continue;
        }
    }
    if (! card_ids.empty())
    {
        if (! error_list.empty())
        {
            std::cerr << "Warning: Ignore some cards while resolving " << description << ": ";
            for (auto error: error_list)
            {
                std::cerr << '[' << error << ']';
            }
            std::cerr << std::endl;
        }
        return {card_ids, card_marks};
    }
    try
    {
        hash_to_ids(deck_string.c_str(), card_ids);
        for (auto & card_id: card_ids)
        {
            try
            {
                all_cards.by_id(card_id);
            }
            catch(std::exception& e)
            {
                throw std::runtime_error(std::string("Deck not found. Error to treat as hash: ") + e.what());
            }
        }
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: Failed to resolve " << description << ": " << e.what() << std::endl;
        throw;
    }
    return {card_ids, card_marks};
}

namespace range = boost::range;

void Deck::set(const std::vector<unsigned>& ids, const std::map<signed, char> &marks)
{
    commander = nullptr;
    strategy = DeckStrategy::random;
    for(auto id: ids)
    {
        const Card* card{all_cards.by_id(id)};
        if(card->m_type == CardType::commander)
        {
            if (commander == nullptr)
            {
                commander = card;
            }
            else
            {
                throw std::runtime_error("While constructing a deck: two commanders detected (" + card->m_name + " and " + commander->m_name + ")");
            }
        }
        else
        {
            cards.emplace_back(card);
        }
    }
    if (commander == nullptr)
    {
        throw std::runtime_error("While constructing a deck: no commander found");
    }
    card_marks = marks;
}

void Deck::set(const std::string& deck_string_)
{
    deck_string = deck_string_;
}

void Deck::resolve()
{
    if (commander != nullptr)
    {
        return;
    }
    auto && id_marks = string_to_ids(all_cards, deck_string, short_description());
    set(id_marks.first, id_marks.second);
    deck_string.clear();
}

void Deck::shrink(const unsigned deck_len)
{
    if (cards.size() > deck_len)
    {
        cards.resize(deck_len);
    }
}

void Deck::set_vip_cards(const std::string& deck_string)
{
    auto && id_marks = string_to_ids(all_cards, deck_string, "vip");
    for (const auto & cid : id_marks.first)
    {
        vip_cards.insert(cid);
    }
}

void Deck::set_given_hand(const std::string& deck_string)
{
    auto && id_marks = string_to_ids(all_cards, deck_string, "hand");
    given_hand = id_marks.first;
}

void Deck::set_forts(const std::string& deck_string)
{
    auto && id_marks = string_to_ids(all_cards, deck_string, "fort_cards");
    fort_cards.clear();
    for (auto id: id_marks.first)
    {
       fort_cards.push_back(all_cards.by_id(id));
    }
}

std::string Deck::hash() const
{
    std::stringstream ios;
    if (strategy == DeckStrategy::random)
    {
        auto sorted_cards = cards;
        std::sort(sorted_cards.begin(), sorted_cards.end(), [](const Card* a, const Card* b) { return a->m_id < b->m_id; });
        encode_deck(ios, commander, sorted_cards);
    }
    else
    {
        encode_deck(ios, commander, cards);
    }
    return ios.str();
}

std::string Deck::short_description() const
{
    std::stringstream ios;
    ios << decktype_names[decktype];
    if(id > 0) { ios << " #" << id; }
    if(!name.empty()) { ios << " \"" << name << "\""; }
    if(deck_string.empty())
    {
        if(raid_cards.empty()) { ios << ": " << hash(); }
    }
    else
    {
        ios << ": " << deck_string;
    }
    return ios.str();
}

std::string Deck::medium_description() const
{
    std::stringstream ios;
    ios << short_description() << std::endl;
    if (commander)
    {
        ios << commander->m_name;
    }
    else
    {
        ios << "No commander";
    }
    for(const Card * card: cards)
    {
        ios << ", " << card->m_name;
    }
    unsigned num_pool_cards = 0;
    for(auto& pool: raid_cards)
    {
        num_pool_cards += pool.first;
    }
    if(num_pool_cards > 0)
    {
        ios << ", and " << num_pool_cards << " cards from pool";
    }
    if (upgrade_points > 0)
    {
        ios << " +" << upgrade_points << "/" << upgrade_opportunities;
    }
    return ios.str();
}

extern std::string card_description(const Cards& all_cards, const Card* c);

std::string Deck::long_description() const
{
    std::stringstream ios;
    ios << medium_description() << "\n";
    if(effect != Effect::none)
    {
        ios << "Effect: " << effect_names[effect] << "\n";
    }
    if (commander)
    {
        show_upgrades(ios, commander, "");
    }
    else
    {
        ios << "No commander\n";
    }
    for(const Card* card: cards)
    {
        show_upgrades(ios, card, "  ");
    }
    for(auto& pool: raid_cards)
    {
        ios << pool.first << " of:\n";
        for(auto& card: pool.second)
        {
            show_upgrades(ios, card, "  ");
        }
    }
    for (const Card * fort: fort_cards)
    {
        ios << card_description(all_cards, fort) << "\n";
    }
    if (debug_print >= 2 && !reward_cards.empty())
    {
        ios << "Reward Cards: ";
        for (const auto & card : reward_cards)
        {
            ios << card->m_name << ", ";
        }
        ios << "\n";
    }
    return ios.str();
}

void Deck::show_upgrades(std::stringstream &ios, const Card* card, const char * leading_chars) const
{
    ios << leading_chars << card_description(all_cards, card) << "\n";
    if (upgrade_points == 0 || card == card->m_top_level_card)
    {
        return;
    }
    if (debug_print < 2 && decktype != DeckType::raid)
    {
        ios << leading_chars << "-> " << card_description(all_cards, card->m_top_level_card) << "\n";
        return;
    }
    // nCm * p^m / q^(n-m)
    double p = 1.0 * upgrade_points / upgrade_opportunities;
    double q = 1.0 - p;
    unsigned n = card->m_top_level_card->m_level - card->m_level;
    unsigned m = 0;
    double prob = 100.0 * pow(q, n);
    ios << leading_chars << std::fixed << std::setprecision(2) << std::setw(5) << prob << "% no up\n";
    while (card != card->m_top_level_card)
    {
        card = card->upgraded();
        ++m;
        prob = prob * (n + 1 - m) / m * p / q;
        ios << leading_chars << std::setw(5) << prob << "% -> " << card_description(all_cards, card) << "\n";
    }
}

Deck* Deck::clone() const
{
    return(new Deck(*this));
}

const Card* Deck::next()
{
    if(shuffled_cards.empty())
    {
        return(nullptr);
    }
    else if(strategy == DeckStrategy::random || strategy == DeckStrategy::exact_ordered)
    {
        const Card* card = shuffled_cards.front();
        shuffled_cards.pop_front();
        return(card);
    }
    else if(strategy == DeckStrategy::ordered)
    {
        auto cardIter = std::min_element(shuffled_cards.begin(), shuffled_cards.begin() + std::min<unsigned>(3u, shuffled_cards.size()), [this](const Card* card1, const Card* card2) -> bool
                                         {
                                             auto card1_order = order.find(card1->m_id);
                                             if(!card1_order->second.empty())
                                             {
                                                 auto card2_order = order.find(card2->m_id);
                                                 if(!card2_order->second.empty())
                                                 {
                                                     return(*card1_order->second.begin() < *card2_order->second.begin());
                                                 }
                                                 else
                                                 {
                                                     return(true);
                                                 }
                                             }
                                             else
                                             {
                                                 return(false);
                                             }
                                         });
        auto card = *cardIter;
        shuffled_cards.erase(cardIter);
        auto card_order = order.find(card->m_id);
        if(!card_order->second.empty())
        {
            card_order->second.erase(card_order->second.begin());
        }
        return(card);
    }
    throw std::runtime_error("Unknown strategy for deck.");
}

const Card* Deck::upgrade_card(const Card* card, std::mt19937& re, unsigned &remaining_upgrade_points, unsigned &remaining_upgrade_opportunities)
{
    unsigned oppos = card->m_top_level_card->m_level - card->m_level;
    if (remaining_upgrade_points > 0)
    {
        for (; oppos > 0; -- oppos)
        {
            std::mt19937::result_type rnd = re();
            if (rnd % remaining_upgrade_opportunities < remaining_upgrade_points)
            {
                card = card->upgraded();
                -- remaining_upgrade_points;
            }
            -- remaining_upgrade_opportunities;
        }
    }
    return card;
}

void Deck::shuffle(std::mt19937& re)
{
    shuffled_commander = commander;
    shuffled_cards.clear();
    boost::insert(shuffled_cards, shuffled_cards.end(), cards);
    if(!raid_cards.empty())
    {
        if(strategy != DeckStrategy::random)
        {
            throw std::runtime_error("Support only random strategy for raid/quest deck.");
        }
        for(auto& card_pool: raid_cards)
        {
            assert(card_pool.first <= card_pool.second.size());
            partial_shuffle(card_pool.second.begin(), card_pool.second.begin() + card_pool.first, card_pool.second.end(), re);
            shuffled_cards.insert(shuffled_cards.end(), card_pool.second.begin(), card_pool.second.begin() + card_pool.first);
        }
    }
    if (upgrade_points > 0)
    {
        unsigned remaining_upgrade_points = upgrade_points;
        unsigned remaining_upgrade_opportunities = upgrade_opportunities;
        shuffled_commander = upgrade_card(commander, re, remaining_upgrade_points, remaining_upgrade_opportunities);
        for (auto && card: shuffled_cards)
        {
            card = upgrade_card(card, re, remaining_upgrade_points, remaining_upgrade_opportunities);
        }
    }
    if(strategy == DeckStrategy::ordered)
    {
        unsigned i = 0;
        order.clear();
        for(auto card: cards)
        {
            order[card->m_id].push_back(i);
            ++i;
        }
    }
    if(strategy != DeckStrategy::exact_ordered)
    {
        auto shufflable_iter = shuffled_cards.begin();
        for(auto hand_card_id: given_hand)
        {
            auto it = std::find_if(shufflable_iter, shuffled_cards.end(), [hand_card_id](const Card* card) -> bool { return card->m_id == hand_card_id; });
            if(it != shuffled_cards.end())
            {
                std::swap(*shufflable_iter, *it);
                ++ shufflable_iter;
            }
        }
        std::shuffle(shufflable_iter, shuffled_cards.end(), re);
#if 0
        if(!given_hand.empty())
        {
            for(auto card: cards) std::cout << ", " << card->m_name;
            std::cout << std::endl;
            std::cout << strategy;
            for(auto card: shuffled_cards) std::cout << ", " << card->m_name;
            std::cout << std::endl;
        }
#endif
    }
}

void Deck::place_at_bottom(const Card* card)
{
    shuffled_cards.push_back(card);
}

