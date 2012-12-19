// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//------------------------------------------------------------------------------
//#define NDEBUG
#define BOOST_THREAD_USE_LIB
#include <cassert>
#include <cstring>
#include <ctime>
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <tuple>
#include <boost/range/join.hpp>
#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/math/distributions/binomial.hpp>
#include "card.h"
#include "cards.h"
#include "deck.h"
#include "read.h"
#include "sim.h"
#include "tyrant.h"
#include "xml.h"
//#include "timer.hpp"

using namespace std::placeholders;
//------------------------------------------------------------------------------
void print_deck(DeckIface& deck)
{
    std::cout << "Deck:" << std::endl;
    if(deck.commander)
    {
        std::cout << deck.commander->m_name << "\n";
    }
    else
    {
        std::cout << "No commander\n";
    }
    for(const Card* card: deck.cards)
    {
        std::cout << "  " << card->m_name << "\n" << std::flush;
    }
}
//------------------------------------------------------------------------------
DeckIface* find_deck(const Decks& decks, std::string name)
{
    auto it1 = decks.mission_decks_by_name.find(name);
    if(it1 != decks.mission_decks_by_name.end())
    {
        return(it1->second);
    }
    auto it2 = decks.raid_decks_by_name.find(name);
    if(it2 != decks.raid_decks_by_name.end())
    {
        return(it2->second);
    }
    auto it3 = decks.custom_decks.find(name);
    if(it3 != decks.custom_decks.end())
    {
        return(it3->second);
    }
    return(nullptr);
}
//---------------------- $80 deck optimization ---------------------------------
//------------------------------------------------------------------------------
// Owned cards
//------------------------------------------------------------------------------
std::map<unsigned, unsigned> owned_cards;
bool use_owned_cards{false};

// No raid rewards from 500 and 1k honor for ancient raids
// No very hard to get rewards (level >= 150, faction >= 13)
// No WB
// No UB
bool cheap_1(const Card* card)
{
    if(card->m_set == 2 || card->m_set == 3 || card->m_set == 4 || card->m_set == 6 || card->m_set == 5001 || card->m_set == 9000) { return(false); }
    // Ancient raids rewards
    // pantheon
    if(card->m_id == 567 || card->m_id == 566) { return(false); }
    // sentinel
    if(card->m_id == 572 || card->m_id == 564) { return(false); }
    // lithid
    if(card->m_id == 570 || card->m_id == 571) { return(false); }
    // hydra
    if(card->m_id == 565 || card->m_id == 568) { return(false); }
    // Arachnous
    if(card->m_id == 432) { return(false); }
    // Shrouded defiler
    if(card->m_id == 434) { return(false); }
    // Emergency fire
    if(card->m_id == 3021) { return(false); }
    // Turbo commando
    if(card->m_id == 428) { return(false); }
    // Solar powerhouse
    if(card->m_id == 530) { return(false); }
    // Utopia Beacon
    if(card->m_id == 469) { return(false); }
    return(true);
}

// Top commanders
std::set<unsigned> top_commanders{
    // common commanders:
    1105, // Opak
        1121, // Daizon
        // uncommon commanders:
        1031, // Dalia
        1017, // Duncan
        1120, // Emanuel
        1102, // Korvald
        // rare commanders:
        1021, // Atlas
        1153, // Daedalus
        1045, // Dracorex
        1099, // Empress
        1116, // Gaia
        1182, // Gialdrea
        1050, // IC
        1184, // Kleave
        1004, // LoT
        1123, // LtW
        1171, // Nexor
        1104, // Stavros
        1152, // Svetlana
        1141, // Tabitha
        1172, // Teiffa
        // halcyon + terminus:
        1203, // Halcyon the Corrupt shitty artwork
        1204, // Halcyon the Corrupt LE
        1200, // Corra
        1049, // Lord Halcyon
        1198, // Virulentus
        1199, // Lord Silus
        };
//------------------------------------------------------------------------------
bool suitable_non_commander(DeckIface& deck, unsigned slot, const Card* card)
{
    assert(card->m_type != CardType::commander);
    if(use_owned_cards)
    {
        if(owned_cards.find(card->m_id) == owned_cards.end()) { return(false); }
        else
        {
            unsigned num_in_deck{0};
            for(unsigned i(0); i < deck.cards.size(); ++i)
            {
                if(i != slot && deck.cards[i]->m_id == card->m_id)
                {
                    ++num_in_deck;
                }
            }
            if(owned_cards.find(card->m_id)->second <= num_in_deck) { return(false); }
        }
    }
    if(card->m_rarity == 4) // legendary - 1 per deck
    {
        for(unsigned i(0); i < deck.cards.size(); ++i)
        {
            if(i != slot && deck.cards[i]->m_rarity == 4)
            {
                return(false);
            }
        }
    }
    if(card->m_unique) // unique - 1 card with same id per deck
    {
        for(unsigned i(0); i < deck.cards.size(); ++i)
        {
            if(i != slot && deck.cards[i]->m_id == card->m_id)
            {
                return(false);
            }
        }
    }
    return(true);
}

bool keep_commander{false};
bool suitable_commander(const Card* card)
{
    assert(card->m_type == CardType::commander);
    if(keep_commander) { return(false); }
    if(use_owned_cards)
    {
        auto owned_iter = owned_cards.find(card->m_id);
        if(owned_iter == owned_cards.end()) { return(false); }
        else
        {
            if(owned_iter->second <= 0) { return(false); }
        }
    }
    if(top_commanders.find(card->m_id) == top_commanders.end()) { return(false); }
    return(true);
}
//------------------------------------------------------------------------------
double compute_efficiency(const std::pair<std::vector<unsigned> , unsigned>& results)
{
    if(results.second == 0) { return(0.); }
    double sum{0.};
    for(unsigned index(0); index < results.first.size(); ++index)
    {
        sum += (double)results.second / results.first[index];
    }
    return(results.first.size() / sum);
}
//------------------------------------------------------------------------------
bool use_efficiency{false};
double compute_score(const std::pair<std::vector<unsigned> , unsigned>& results, std::vector<double>& factors)
{
    double score{0.};
    if(use_efficiency)
    {
        score = compute_efficiency(results);
    }
    else
    {
        if(results.second == 0) { score = 0.; }
        double sum{0.};
        for(unsigned index(0); index < results.first.size(); ++index)
        {
            sum += results.first[index] * factors[index];
        }
        score = sum / std::accumulate(factors.begin(), factors.end(), 0.) / (double)results.second;
    }
    return(score);
}
//------------------------------------------------------------------------------
volatile unsigned thread_num_iterations{0}; // written by threads
std::vector<unsigned> thread_score; // written by threads
volatile unsigned thread_total{0}; // written by threads
volatile double thread_prev_score{0.0};
volatile bool thread_compare{false};
volatile bool thread_compare_stop{false}; // written by threads
volatile bool destroy_threads;
//------------------------------------------------------------------------------
// Per thread data.
// seed should be unique for each thread.
// d1 and d2 are intended to point to read-only process-wide data.
struct SimulationData
{
    std::mt19937 re;
    const Cards& cards;
    const Decks& decks;
    std::shared_ptr<DeckIface> att_deck;
    Hand att_hand;
    std::vector<std::shared_ptr<DeckIface> > def_decks;
    std::vector<Hand*> def_hands;
    std::vector<double> factors;
    gamemode_t gamemode;

    SimulationData(unsigned seed, const Cards& cards_, const Decks& decks_, unsigned num_def_decks_, std::vector<double> factors_, gamemode_t gamemode_) :
        re(seed),
        cards(cards_),
        decks(decks_),
        att_deck(),
        att_hand(nullptr),
        def_decks(num_def_decks_),
        factors(factors_),
        gamemode(gamemode_)
    {
        for(auto def_deck: def_decks)
        {
            def_hands.emplace_back(new Hand(nullptr));
        }
    }

    ~SimulationData()
    {
        for(auto hand: def_hands) { delete(hand); }
    }

    void set_decks(const DeckIface* const att_deck_, std::vector<DeckIface*> const & def_decks_)
    {
        att_deck.reset(att_deck_->clone());
        att_hand.deck = att_deck.get();
        for(unsigned i(0); i < def_decks_.size(); ++i)
        {
            def_decks[i].reset(def_decks_[i]->clone());
            def_hands[i]->deck = def_decks[i].get();
        }
    }

    inline std::vector<unsigned> evaluate()
    {
        std::vector<unsigned> res;
        for(Hand* def_hand: def_hands)
        {
            att_hand.reset(re);
            def_hand->reset(re);
            Field fd(re, cards, att_hand, *def_hand, gamemode);
            unsigned result(play(&fd));
            res.emplace_back(result);
        }
        return(res);
    }
};
//------------------------------------------------------------------------------
class Process;
void thread_evaluate(boost::barrier& main_barrier,
                     boost::mutex& shared_mutex,
                     SimulationData& sim,
                     const Process& p);
//------------------------------------------------------------------------------
class Process
{
public:
    unsigned num_threads;
    std::vector<boost::thread*> threads;
    std::vector<SimulationData*> threads_data;
    boost::barrier main_barrier;
    boost::mutex shared_mutex;
    const Cards& cards;
    const Decks& decks;
    DeckIface* att_deck;
    const std::vector<DeckIface*> def_decks;
    std::vector<double> factors;
    gamemode_t gamemode;

    Process(unsigned _num_threads, const Cards& cards_, const Decks& decks_, DeckIface* att_deck_, std::vector<DeckIface*> _def_decks, std::vector<double> _factors, gamemode_t _gamemode) :
        num_threads(_num_threads),
        cards(cards_),
        decks(decks_),
        att_deck(att_deck_),
        def_decks(_def_decks),
        factors(_factors),
        main_barrier(num_threads+1),
        gamemode(_gamemode)
    {
        destroy_threads = false;
        unsigned seed(time(0));
        for(unsigned i(0); i < num_threads; ++i)
        {
            threads_data.push_back(new SimulationData(seed + i, cards, decks, def_decks.size(), factors, gamemode));
            threads.push_back(new boost::thread(thread_evaluate, std::ref(main_barrier), std::ref(shared_mutex), std::ref(*threads_data.back()), std::ref(*this)));
        }
    }

    ~Process()
    {
        destroy_threads = true;
        main_barrier.wait();
        for(auto thread: threads) { thread->join(); }
        for(auto data: threads_data) { delete(data); }
    }

    std::pair<std::vector<unsigned> , unsigned> evaluate(unsigned num_iterations)
    {
        thread_num_iterations = num_iterations;
        thread_score = std::vector<unsigned>(def_decks.size(), 0u);
        thread_total = 0;
        thread_compare = false;
        // unlock all the threads
        main_barrier.wait();
        // wait for the threads
        main_barrier.wait();
        return(std::make_pair(thread_score, thread_total));
    }

    std::pair<std::vector<unsigned> , unsigned> compare(unsigned num_iterations, double prev_score)
    {
        thread_num_iterations = num_iterations;
        thread_score = std::vector<unsigned>(def_decks.size(), 0u);
        thread_total = 0;
        thread_prev_score = prev_score;
        thread_compare = true;
        thread_compare_stop = false;
        // unlock all the threads
        main_barrier.wait();
        // wait for the threads
        main_barrier.wait();
        return(std::make_pair(thread_score, thread_total));
    }
};
//------------------------------------------------------------------------------
void thread_evaluate(boost::barrier& main_barrier,
                     boost::mutex& shared_mutex,
                     SimulationData& sim,
                     const Process& p)
{
    while(true)
    {
        main_barrier.wait();
        sim.set_decks(p.att_deck, p.def_decks);
        if(destroy_threads) { return; }
        while(true)
        {
            shared_mutex.lock(); //<<<<
            if(thread_num_iterations == 0 || (thread_compare && thread_compare_stop)) //!
            {
                shared_mutex.unlock(); //>>>>
                main_barrier.wait();
                break;
            }
            else
            {
                --thread_num_iterations; //!
                shared_mutex.unlock(); //>>>>
                std::vector<unsigned> result{sim.evaluate()};
                shared_mutex.lock(); //<<<<
                std::vector<unsigned> thread_score_local(thread_score.size(), 0); //!
                for(unsigned index(0); index < result.size(); ++index)
                {
                    thread_score[index] += result[index] == 0 ? 1 : 0; //!
                    thread_score_local[index] = thread_score[index]; // !
                }
                ++thread_total; //!
                unsigned thread_total_local{thread_total}; //!
                shared_mutex.unlock(); //>>>>
                if(thread_compare && thread_total_local >= 1 && thread_total_local % 100 == 0)
                {
                    unsigned score_accum = 0;
                    // Multiple defense decks case: scaling by factors and approximation of a "discrete" number of events.
                    if(result.size() > 1)
                    {
                        double score_accum_d = 0.0;
                        for(unsigned i = 0; i < thread_score_local.size(); ++i)
                        {
                            score_accum_d += thread_score_local[i] * sim.factors[i];
                        }
                        score_accum_d /= std::accumulate(sim.factors.begin(), sim.factors.end(), .0d);
                        score_accum = score_accum_d;
                    }
                    else
                    {
                        score_accum = thread_score_local[0];
                    }
                    if(boost::math::binomial_distribution<>::find_upper_bound_on_p(thread_total_local, score_accum, 0.01) < thread_prev_score)
                    {
                        shared_mutex.lock(); //<<<<
                        //std::cout << thread_total_local << "\n";
                        thread_compare_stop = true; //!
                        shared_mutex.unlock(); //>>>>
                    }
                }
            }
        }
    }
}
//------------------------------------------------------------------------------
void print_score_info(const std::pair<std::vector<unsigned> , unsigned>& results, std::vector<double>& factors)
{
    std::cout << "win%: " << compute_score(results, factors) * 100.0 << " (";
    for(auto val: results.first)
    {
        std::cout << val << " ";
    }
    std::cout << "out of " << results.second << ")\n" << std::flush;
}
//------------------------------------------------------------------------------
void hill_climbing(unsigned num_iterations, DeckIface* d1, Process& proc)
{
    auto results = proc.evaluate(num_iterations);
    print_score_info(results, proc.factors);
    double current_score = compute_score(results, proc.factors);
    double best_score = current_score;
    // Non-commander cards
    auto non_commander_cards = boost::join(boost::join(proc.cards.player_assaults, proc.cards.player_structures), proc.cards.player_actions);
    const Card* best_commander = d1->commander;
    std::vector<const Card*> best_cards = d1->cards;
    bool deck_has_been_improved = true;
    bool eval_commander = true;
    while(deck_has_been_improved && best_score < 1.0)
    {
        deck_has_been_improved = false;
        for(unsigned slot_i(0); slot_i < d1->cards.size(); ++slot_i)
        {
            if(eval_commander && !keep_commander)
            {
                for(const Card* commander_candidate: proc.cards.player_commanders)
                {
                    // Various checks to check if the card is accepted
                    assert(commander_candidate->m_type == CardType::commander);
                    if(commander_candidate == best_commander) { continue; }
                    if(!suitable_commander(commander_candidate)) { continue; }
                    // Place it in the deck
                    d1->commander = commander_candidate;
                    // Evaluate new deck
                    auto compare_results = proc.compare(num_iterations, best_score);
                    current_score = compute_score(compare_results, proc.factors);
                    // Is it better ?
                    if(current_score > best_score)
                    {
                        // Then update best score/commander, print stuff
                        best_score = current_score;
                        best_commander = commander_candidate;
                        deck_has_been_improved = true;
                        std::cout << "Deck improved: commander -> " << commander_candidate->m_name << ": ";
                        print_score_info(compare_results, proc.factors);
                    }
                }
                // Now that all commanders are evaluated, take the best one
                d1->commander = best_commander;
                eval_commander = false;
            }
            for(const Card* card_candidate: non_commander_cards)
            {
                // Various checks to check if the card is accepted
                assert(card_candidate->m_type != CardType::commander);
                if(card_candidate == best_cards[slot_i]) { continue; }
                if(!suitable_non_commander(*d1, slot_i, card_candidate)) { continue; }
                // Place it in the deck
                d1->cards[slot_i] = card_candidate;
                // Evaluate new deck
                auto compare_results = proc.compare(num_iterations, best_score);
                current_score = compute_score(compare_results, proc.factors);
                // Is it better ?
                if(current_score > best_score)
                {
                    // Then update best score/slot, print stuff
                    best_score = current_score;
                    best_cards[slot_i] = card_candidate;
                    eval_commander = true;
                    deck_has_been_improved = true;
                    std::cout << "Deck improved: slot " << slot_i << " -> " << card_candidate->m_name << ": ";
                    print_score_info(compare_results, proc.factors);
                }
            }
            // Now that all cards are evaluated, take the best one
            d1->cards[slot_i] = best_cards[slot_i];
        }
    }
    std::cout << "Best deck: " << best_score * 100.0 << "%\n";
    std::cout << best_commander->m_name;
    for(const Card* card: best_cards)
    {
        std::cout << ", " << card->m_name;
    }
    std::cout << "\n";
}
//------------------------------------------------------------------------------
void hill_climbing_ordered(unsigned num_iterations, DeckOrdered* d1, Process& proc)
{
    auto results = proc.evaluate(num_iterations);
    print_score_info(results, proc.factors);
    double current_score = compute_score(results, proc.factors);
    double best_score = current_score;
    // Non-commander cards
    auto non_commander_cards = boost::join(boost::join(proc.cards.player_assaults, proc.cards.player_structures), proc.cards.player_actions);
    const Card* best_commander = d1->commander;
    std::vector<const Card*> best_cards = d1->cards;
    bool deck_has_been_improved = true;
    bool eval_commander = true;
    while(deck_has_been_improved && best_score < 1.0)
    {
        deck_has_been_improved = false;
        std::set<unsigned> remaining_cards;
        for(unsigned i = 0; i < best_cards.size(); ++i)
        {
            remaining_cards.insert(i);
        }
        while(!remaining_cards.empty())
        {
            unsigned current_slot(*remaining_cards.begin());
            remaining_cards.erase(remaining_cards.begin());
            if(eval_commander && !keep_commander)
            {
                for(const Card* commander_candidate: proc.cards.player_commanders)
                {
                    if(best_score == 1.0) { break; }
                    // Various checks to check if the card is accepted
                    assert(commander_candidate->m_type == CardType::commander);
                    if(commander_candidate == best_commander) { continue; }
                    if(!suitable_commander(commander_candidate)) { continue; }
                    // Place it in the deck
                    d1->commander = commander_candidate;
                    // Evaluate new deck
                    auto compare_results = proc.compare(num_iterations, best_score);
                    current_score = compute_score(compare_results, proc.factors);
                    // Is it better ?
                    if(current_score > best_score)
                    {
                        // Then update best score/commander, print stuff
                        best_score = current_score;
                        best_commander = commander_candidate;
                        deck_has_been_improved = true;
                        std::cout << "Deck improved: commander -> " << commander_candidate->m_name << ": ";
                        print_score_info(compare_results, proc.factors);
                    }
                }
                // Now that all commanders are evaluated, take the best one
                d1->commander = best_commander;
                eval_commander = false;
            }
            for(const Card* card_candidate: non_commander_cards)
            {
                if(best_score == 1.0) { break; }
                // Various checks to check if the card is accepted
                assert(card_candidate->m_type != CardType::commander);
                for(unsigned slot_i(0); slot_i < d1->cards.size(); ++slot_i)
                {
                    // Various checks to check if the card is accepted
                    if(card_candidate == best_cards[slot_i]) { continue; }
                    if(!suitable_non_commander(*d1, current_slot, card_candidate)) { continue; }
                    // Place it in the deck
                    d1->cards.erase(d1->cards.begin() + current_slot);
                    d1->cards.insert(d1->cards.begin() + slot_i, card_candidate);
                    // Evaluate new deck
                    auto compare_results = proc.compare(num_iterations, best_score);
                    current_score = compute_score(compare_results, proc.factors);
                    // Is it better ?
                    if(current_score > best_score)
                    {
                        // Then update best score/slot, print stuff
                        std::cout << "Deck improved: " << current_slot << " " << best_cards[current_slot]->m_name << " -> " << slot_i << " " << card_candidate->m_name << ": ";
                        best_score = current_score;
                        best_cards.erase(best_cards.begin() + current_slot);
                        best_cards.insert(best_cards.begin() + slot_i, card_candidate);
                        eval_commander = true;
                        deck_has_been_improved = true;
                        print_score_info(compare_results, proc.factors);
                    }
                    d1->cards = best_cards;
                }
            }
            // Now that all cards are evaluated, take the best one
            // d1->cards[slot_i] = best_cards[slot_i];
        }
    }
    std::cout << "Best deck: " << best_score * 100.0 << "%\n";
    std::cout << best_commander->m_name;
    for(const Card* card: best_cards)
    {
        std::cout << ", " << card->m_name;
    }
    std::cout << "\n";
}
//------------------------------------------------------------------------------
// Implements iteration over all combination of k elements from n elements.
// parameter firstIndexLimit: this is a ugly hack used to implement the special condition that
// a deck could be expected to contain at least 1 assault card. Thus the first element
// will be chosen among the assault cards only, instead of all cards.
// It works on the condition that the assault cards are sorted first in the list of cards,
// thus have indices 0..firstIndexLimit-1.
class Combination
{
public:
    Combination(unsigned all_, unsigned choose_, unsigned firstIndexLimit_ = 0) :
        all(all_),
        choose(choose_),
        firstIndexLimit(firstIndexLimit_ == 0 ? all_ - choose_ : firstIndexLimit_),
        indices(choose_, 0),
        indicesLimits(choose_, 0)
    {
        assert(choose > 0);
        assert(choose <= all);
        assert(firstIndexLimit <= all);
        indicesLimits[0] = firstIndexLimit;
        for(unsigned i(1); i < choose; ++i)
        {
            indices[i] = i;
            indicesLimits[i] =  all - choose + i;
        }
    }

    const std::vector<unsigned>& getIndices()
    {
        return(indices);
    }

    bool next()
    {
        for(index = choose - 1; index >= 0; --index)
        {
            if(indices[index] < indicesLimits[index])
            {
                ++indices[index];
                for(nextIndex = index + 1; nextIndex < choose; nextIndex++)
                {
                    indices[nextIndex] = indices[index] - index + nextIndex;
                }
                return(false);
            }
        }
        return(true);
    }

private:
    unsigned all;
    unsigned choose;
    unsigned firstIndexLimit;
    std::vector<unsigned> indices;
    std::vector<unsigned> indicesLimits;
    int index;
    int nextIndex;
};
//------------------------------------------------------------------------------
static unsigned total_num_combinations_test(0);
inline void try_all_ratio_combinations(unsigned deck_size, unsigned var_k, unsigned num_iterations, const std::vector<unsigned>& card_indices, std::vector<const Card*>& cards, const Card* commander, Process& proc, double& best_score, boost::optional<DeckRandom>& best_deck)
{
    assert(card_indices.size() > 0);
    assert(card_indices.size() <= deck_size);
    unsigned num_cards_to_combine(deck_size);
    std::vector<const Card*> unique_cards;
    std::vector<const Card*> cards_to_combine;
    bool legendary_found(false);
    for(unsigned card_index: card_indices)
    {
        const Card* card(cards[card_index]);
        if(card->m_unique || card->m_rarity == 4)
        {
            if(card->m_rarity == 4)
            {
                if(legendary_found) { return; }
                legendary_found = true;
            }
            --num_cards_to_combine;
            unique_cards.push_back(card);
        }
        else
        {
            cards_to_combine.push_back(card);
        }
    }
    // all unique or legendaries, quit
    if(cards_to_combine.size() == 0) { return; }
    if(cards_to_combine.size() == 1)
    {
        std::vector<const Card*> deck_cards = unique_cards;
        std::vector<const Card*> combined_cards(num_cards_to_combine, cards_to_combine[0]);
        deck_cards.insert(deck_cards.end(), combined_cards.begin(), combined_cards.end());
        DeckRandom deck(commander, deck_cards);
        (*dynamic_cast<DeckRandom*>(proc.att_deck)) = deck;
        auto new_results = proc.compare(num_iterations, best_score);
        double new_score = compute_score(new_results, proc.factors);
        if(new_score > best_score)
        {
            best_score = new_score;
            best_deck = deck;
            print_score_info(new_results, proc.factors);
            print_deck(deck);
            std::cout << std::flush;
        }
        //++num;
        // num_cards = num_cards_to_combine ...
    }
    else
    {
        var_k = cards_to_combine.size() - 1;
        Combination cardAmounts(num_cards_to_combine-1, var_k);
        bool finished(false);
        while(!finished)
        {
            const std::vector<unsigned>& indices = cardAmounts.getIndices();
            std::vector<unsigned> num_cards(var_k+1, 0);
            num_cards[0] = indices[0] + 1;
            for(unsigned i(1); i < var_k; ++i)
            {
                num_cards[i] = indices[i] - indices[i-1];
            }
            num_cards[var_k] = num_cards_to_combine - (indices[var_k-1] + 1);
            std::vector<const Card*> deck_cards = unique_cards;
            //std::cout << "num cards: ";
            for(unsigned num_index(0); num_index < num_cards.size(); ++num_index)
            {
                //std::cout << num_cards[num_index] << " ";
                for(unsigned i(0); i < num_cards[num_index]; ++i) { deck_cards.push_back(cards[card_indices[num_index]]); }
            }
            //std::cout << "\n" << std::flush;
            //std::cout << std::flush;
            assert(deck_cards.size() == deck_size);
            DeckRandom deck(commander, deck_cards);
            *proc.att_deck = deck;
            auto new_results = proc.compare(num_iterations, best_score);
            double new_score = compute_score(new_results, proc.factors);
            if(new_score > best_score)
            {
                best_score = new_score;
                best_deck = deck;
                print_score_info(new_results, proc.factors);
                print_deck(deck);
                std::cout << std::flush;
            }
            ++total_num_combinations_test;
            finished = cardAmounts.next();
        }
    }
}
//------------------------------------------------------------------------------
void exhaustive_k(unsigned num_iterations, unsigned var_k, Process& proc)
{
    std::vector<const Card*> ass_structs;
    for(const Card* card: proc.cards.player_assaults)
    {
        if(card->m_rarity >= 3) { ass_structs.push_back(card); }
    }
    for(const Card* card: proc.cards.player_structures)
    {
        if(card->m_rarity >= 3) { ass_structs.push_back(card); }
    }
    //std::vector<Card*> ass_structs; = cards.player_assaults;
    //ass_structs.insert(ass_structs.end(), cards.player_structures.begin(), cards.player_structures.end());
    unsigned var_n = ass_structs.size();
    assert(var_k <= var_n);
    unsigned num(0);
    Combination cardIndices(var_n, var_k);
    const std::vector<unsigned>& indices = cardIndices.getIndices();
    bool finished(false);
    double best_score{0};
    boost::optional<DeckRandom> best_deck;
    unsigned num_cards = ((DeckRandom*)proc.att_deck)->cards.size();
    while(!finished)
    {
        if(keep_commander)
        {
            try_all_ratio_combinations(num_cards, var_k, num_iterations, indices, ass_structs, ((DeckRandom*)proc.att_deck)->commander, proc, best_score, best_deck);
        }
        else
        {
            // Iterate over all commanders
            for(unsigned commanderIndex(0); commanderIndex < proc.cards.player_commanders.size() && !finished; ++commanderIndex)
            {
                const Card* commander(proc.cards.player_commanders[commanderIndex]);
                if(!suitable_commander(commander)) { continue; }
                try_all_ratio_combinations(num_cards, var_k, num_iterations, indices, ass_structs, commander, proc, best_score, best_deck);
            }
        }
        finished = cardIndices.next();
    }
    std::cout << "done " << num << "\n";
}
//------------------------------------------------------------------------------
enum Operation {
    bruteforce,
    climb,
    fightonce
};
//------------------------------------------------------------------------------
// void print_raid_deck(DeckRandom& deck)
// {
//         std::cout << "--------------- Raid ---------------\n";
//         std::cout << "Commander:\n";
//         std::cout << "  " << deck.m_commander->m_name << "\n";
//         std::cout << "Always include:\n";
//         for(auto& card: deck.m_cards)
//         {
//             std::cout << "  " << card->m_name << "\n";
//         }
//         for(auto& pool: deck.m_raid_cards)
//         {
//             std::cout << pool.first << " from:\n";
//             for(auto& card: pool.second)
//             {
//                 std::cout << "  " << card->m_name << "\n";
//             }
//         }
// }
//------------------------------------------------------------------------------
void print_available_decks(const Decks& decks)
{
    std::cout << "Mission decks:\n";
    for(auto it: decks.mission_decks_by_name)
    {
        std::cout << "  " << it.first << "\n";
    }
    std::cout << "Raid decks:\n";
    for(auto it: decks.raid_decks_by_name)
    {
        std::cout << "  " << it.first << "\n";
    }
    std::cout << "Custom decks:\n";
    for(auto it: decks.custom_decks)
    {
        std::cout << "  " << it.first << "\n";
    }
}

void usage(int argc, char** argv)
{
    std::cout << "usage: " << argv[0] << " <attack deck> <defense decks list> [optional flags] [brute <num1> <num2>] [climb <num>]\n";
    std::cout << "\n";
    std::cout << "<attack deck>: the deck name of a custom deck.\n";
    std::cout << "<defense decks list>: semicolon separated list of defense decks, syntax:\n";
    std::cout << "  deckname1[:factor1];deckname2[:factor2];...\n";
    std::cout << "  where deckname is the name of a mission, raid, or custom deck, and factor is optional. The default factor is 1.\n";
    std::cout << "  example: \'fear:0.2;slowroll:0.8\' means fear is the defense deck 20% of the time, while slowroll is the defense deck 80% of the time.\n";
    std::cout << "\n";
    std::cout << "Flags:\n";
    std::cout << "  -c: don't try to optimize the commander.\n";
    std::cout << "  -o: restrict hill climbing to the owned cards listed in \"ownedcards.txt\".\n";
    std::cout << "  -r: the attack deck is played in order instead of randomly (respects the 3 cards drawn limit).\n";
    std::cout << "  -s: use surge (default is fight).\n";
    std::cout << "  -t <num>: set the number of threads, default is 4.\n";
    std::cout << "  -turnlimit <num>: set the number of turns in a battle, default is 50 (can be used for speedy achievements).\n";
    std::cout << "Operations:\n";
    std::cout << "brute <num1> <num2>: find the best combination of <num1> different cards, using up to <num2> battles to evaluate a deck.\n";
    std::cout << "climb <num>: perform hill-climbing starting from the given attack deck, using up to <num> battles to evaluate a deck.\n";
}

int main(int argc, char** argv)
{
    if(argc == 1) { usage(argc, argv); return(0); }
    debug_print = getenv("DEBUG_PRINT");
    unsigned num_threads = (debug_print || getenv("DEBUG")) ? 1 : 4;
    gamemode_t gamemode = fight;
    bool ordered = false;
    Cards cards;
    read_cards(cards);
    read_owned_cards(cards, owned_cards);
    Decks decks;
    load_decks_xml(decks, cards);
    load_decks(decks, cards);
    fill_skill_table();

    if(argc <= 2)
    {
        print_available_decks(decks);
        return(4);
    }
    std::string att_deck_name{argv[1]};
    std::vector<DeckIface*> def_decks;
    std::vector<double> def_decks_factors;
    auto deck_list_parsed = parse_deck_list(argv[2]);
    for(auto deck_parsed: deck_list_parsed)
    {
        DeckIface* def_deck = find_deck(decks, deck_parsed.first);
        if(def_deck != nullptr)
        {
            def_decks.push_back(def_deck);
            def_decks_factors.push_back(deck_parsed.second);
        }
        else
        {
            std::cout << "The deck " << deck_parsed.first << " was not found. Available decks:\n";
            print_available_decks(decks);
            return(5);
        }
    }
    std::vector<std::tuple<unsigned, unsigned, Operation> > todo;
    for(unsigned argIndex(3); argIndex < argc; ++argIndex)
    {
        if(strcmp(argv[argIndex], "-c") == 0)
        {
            keep_commander = true;
        }
        else if(strcmp(argv[argIndex], "-o") == 0)
        {
            use_owned_cards = true;
        }
        else if(strcmp(argv[argIndex], "-r") == 0)
        {
            ordered = true;
        }
        else if(strcmp(argv[argIndex], "-s") == 0)
        {
            gamemode = surge;
        }
        else if(strcmp(argv[argIndex], "-t") == 0)
        {
            num_threads = atoi(argv[argIndex+1]);
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "-turnlimit") == 0)
        {
            turn_limit = atoi(argv[argIndex+1]);
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "brute") == 0)
        {
            todo.push_back(std::make_tuple((unsigned)atoi(argv[argIndex+1]), (unsigned)atoi(argv[argIndex+2]), bruteforce));
            argIndex += 2;
        }
        else if(strcmp(argv[argIndex], "climb") == 0)
        {
            todo.push_back(std::make_tuple((unsigned)atoi(argv[argIndex+1]), 0u, climb));
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "debug") == 0)
        {
            debug_print = true;
            num_threads = 1;
            todo.push_back(std::make_tuple(0u, 0u, fightonce));
        }
    }

    unsigned attacker_wins(0);
    unsigned defender_wins(0);

    DeckIface* att_deck{nullptr};
    auto custom_deck_it = decks.custom_decks.find(att_deck_name);
    if(custom_deck_it != decks.custom_decks.end())
    {
        att_deck = custom_deck_it->second;
    }
    else
    {
        std::cout << "The deck " << att_deck_name << " was not found. Available decks:\n";
        std::cout << "Custom decks:\n";
        for(auto it: decks.custom_decks)
        {
            std::cout << "  " << it.first << "\n";
        }
        return(5);
    }
    print_deck(*att_deck);

    std::shared_ptr<DeckOrdered> att_deck_ordered;
    if(ordered)
    {
        att_deck_ordered = std::make_shared<DeckOrdered>(*att_deck);
    }

    Process p(num_threads, cards, decks, ordered ? att_deck_ordered.get() : att_deck, def_decks, def_decks_factors, gamemode);
    {
        //ScopeClock timer;
        for(auto op: todo)
        {
            switch(std::get<2>(op))
            {
            case bruteforce: {
                exhaustive_k(std::get<1>(op), std::get<0>(op), p);
                break;
            }
            case climb: {
                if(!ordered)
                {
                    hill_climbing(std::get<0>(op), att_deck, p);
                }
                else
                {
                    hill_climbing_ordered(std::get<0>(op), att_deck_ordered.get(), p);
                }
                break;
            }
            case fightonce: {
                p.evaluate(1);
                break;
            }
            }
        }
    }
    return(0);
}
