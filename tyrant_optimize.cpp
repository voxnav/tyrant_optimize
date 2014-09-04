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
#include <stack>
#include <tuple>
#include <chrono>
#include <boost/range/join.hpp>
#include <boost/optional.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/barrier.hpp>
#include <boost/math/distributions/binomial.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include "card.h"
#include "cards.h"
#include "deck.h"
#include "read.h"
#include "sim.h"
#include "tyrant.h"
#include "xml.h"
//#include "timer.hpp"

namespace {
    gamemode_t gamemode{fight};
    OptimizationMode optimization_mode{OptimizationMode::notset};
    std::map<unsigned, unsigned> owned_cards;
    bool use_owned_cards{true};
    unsigned min_deck_len{1};
    unsigned max_deck_len{10};
    unsigned fund{0};
    long double target_score{100};
    bool show_stdev{false};
    bool use_harmonic_mean{false};
}

using namespace std::placeholders;
//------------------------------------------------------------------------------
std::string card_id_name(const Card* card)
{
    std::stringstream ios;
    if(card)
    {
        ios << "[" << card->m_id << "] " << card->m_name;
    }
    else
    {
        ios << "-void-";
    }
    return ios.str();
}
std::string card_slot_id_names(const std::vector<std::pair<signed, const Card *>> card_list)
{
    if (card_list.empty())
    {
        return "-void-";
    }
    std::stringstream ios;
    std::string separator = "";
    for (const auto & card_it : card_list)
    {
        ios << separator;
        separator = ", ";
        if (card_it.first >= 0)
        { ios << card_it.first << " "; }
        ios << "[" << card_it.second->m_id << "] " << card_it.second->m_name;
    }
    return ios.str();
}
//------------------------------------------------------------------------------
Deck* find_deck(Decks& decks, const Cards& all_cards, std::string deck_name)
{
    auto it = decks.by_name.find(deck_name);
    if(it != decks.by_name.end())
    {
        it->second->resolve();
        return(it->second);
    }
    decks.decks.push_back(Deck{all_cards});
    Deck* deck = &decks.decks.back();
    deck->set(deck_name);
    deck->resolve();
    return(deck);
}
//---------------------- $80 deck optimization ---------------------------------
unsigned get_required_cards_before_upgrade(const std::vector<const Card *> & card_list, std::map<const Card*, unsigned> & num_cards)
{
    unsigned deck_cost = 0;
    std::set<const Card*> unresolved_cards;
    for (const Card * card : card_list)
    {
        ++ num_cards[card];
        unresolved_cards.insert(card);
    }
    // un-upgrade only if fund is used
    while (fund > 0 && !unresolved_cards.empty())
    {
        auto card_it = unresolved_cards.end();
        auto card = *(-- card_it);
        unresolved_cards.erase(card_it);
        if(owned_cards[card->m_id] < num_cards[card] && !card->m_recipe_cards.empty())
        {
            unsigned num_under = num_cards[card] - owned_cards[card->m_id];
            num_cards[card] = owned_cards[card->m_id];
//            std::cout << "-" << num_under << " " << card->m_name << "\n"; // XXX
            deck_cost += num_under * card->m_recipe_cost;
            for (auto recipe_it : card->m_recipe_cards)
            {
                num_cards[recipe_it.first] += num_under * recipe_it.second;
//                std::cout << "+" << num_under * recipe_it.second << " " << recipe_it.first->m_name << "\n"; // XXX
                unresolved_cards.insert(recipe_it.first);
            }
        }
    }
//    std::cout << "\n"; // XXX
    return deck_cost;
}

unsigned get_deck_cost(const Deck * deck)
{
    if (!use_owned_cards)
    { return 0; }
    std::map<const Card *, unsigned> num_in_deck;
    unsigned deck_cost = get_required_cards_before_upgrade({deck->commander}, num_in_deck);
    deck_cost += get_required_cards_before_upgrade(deck->cards, num_in_deck);
    for(auto it: num_in_deck)
    {
        unsigned card_id = it.first->m_id;
        if (it.second > owned_cards[card_id])
        {
            return UINT_MAX;
        }
    }
    return deck_cost;
}

// remove val from oppo if found, otherwise append val to self
template <typename C>
void append_unless_remove(C & self, C & oppo, typename C::const_reference val)
{
    for (auto it = oppo.begin(); it != oppo.end(); ++ it)
    {
        if (*it == val)
        {
            oppo.erase(it);
            return;
        }
    }
    self.push_back(val);
}

// insert card at to_slot into deck limited by fund; store deck_cost
// return true if affordable
bool adjust_deck(Deck * deck, const signed from_slot, const signed to_slot, const Card * card, unsigned fund, std::mt19937 & re, unsigned & deck_cost,
        std::vector<std::pair<signed, const Card *>> & cards_out, std::vector<std::pair<signed, const Card *>> & cards_in)
{
    cards_in.clear();
    if (card == nullptr)
    { // change commander or remove card
        if (to_slot < 0)
        { // change commander
            cards_in.emplace_back(-1, deck->commander);
        }
        deck_cost = get_deck_cost(deck);
        return (deck_cost <= fund);
    }
    bool is_random = deck->strategy == DeckStrategy::random;
    std::vector<const Card *> cards = deck->cards;
    deck->cards.clear();
    deck->cards.emplace_back(card);
    cards_in.emplace_back(is_random ? -1 : to_slot, card);
    {
        // try to add commander into the deck, defuse/downgrade it if necessary
        std::stack<const Card *> candidate_cards;
        const Card * old_commander = deck->commander;
        candidate_cards.emplace(deck->commander);
        while (! candidate_cards.empty())
        {
            const Card* card_in = candidate_cards.top();
            candidate_cards.pop();
            deck->commander = card_in;
            deck_cost = get_deck_cost(deck);
            if (deck_cost <= fund)
            { break; }
            for (auto recipe_it : card_in->m_recipe_cards)
            { candidate_cards.emplace(recipe_it.first); }
        }
        if (deck_cost > fund)
        {
            deck->commander = old_commander;
            return false;
        }
        else if (deck->commander != old_commander)
        {
            append_unless_remove(cards_out, cards_in, {-1, old_commander});
            append_unless_remove(cards_in, cards_out, {-1, deck->commander});
        }
    }
    if (is_random)
    { std::shuffle(cards.begin(), cards.end(), re); }
    for (signed i = 0; i < (signed)cards.size(); ++ i)
    {
        // try to add cards[i] into the deck, defuse/downgrade it if necessary
        auto saved_cards = deck->cards;
        auto in_it = deck->cards.end() - (i < to_slot);
        in_it = deck->cards.insert(in_it, nullptr);
        std::stack<const Card *> candidate_cards;
        candidate_cards.emplace(cards[i]);
        while (! candidate_cards.empty())
        {
            const Card* card_in = candidate_cards.top();
            candidate_cards.pop();
            *in_it = card_in;
            deck_cost = get_deck_cost(deck);
            if (deck_cost <= fund)
            { break; }
            for (auto recipe_it : card_in->m_recipe_cards)
            { candidate_cards.emplace(recipe_it.first); }
        }
        if (deck_cost > fund)
        {
            append_unless_remove(cards_out, cards_in, {is_random ? -1 : i + (i >= to_slot), cards[i]});
            deck->cards = saved_cards;
        }
        else if (*in_it != cards[i])
        {
            append_unless_remove(cards_out, cards_in, {is_random ? -1 : i + (i >= from_slot), cards[i]});
            append_unless_remove(cards_in, cards_out, {is_random ? -1 : i + (i >= to_slot), *in_it});
        }
    }
    deck_cost = get_deck_cost(deck);
    return true;
}

void claim_cards(const std::vector<const Card*> & card_list)
{
    std::map<const Card *, unsigned> num_cards;
    get_required_cards_before_upgrade(card_list, num_cards);
    for(const auto & it: num_cards)
    {
        const Card * card = it.first;
        unsigned num_to_claim = safe_minus(it.second, owned_cards[card->m_id]);
        if(num_to_claim > 0)
        {
            owned_cards[card->m_id] += num_to_claim;
            if(debug_print > 0)
            {
                std::cout << "Claim " << card->m_name << " (" << num_to_claim << ")" << std::endl;
            }
        }
    }
}

//------------------------------------------------------------------------------
Results<long double> compute_score(const std::pair<std::vector<Results<uint64_t>> , unsigned>& results, std::vector<long double>& factors)
{
    Results<long double> final{0, 0, 0, 0, 0};
    for(unsigned index(0); index < results.first.size(); ++index)
    {
        final.wins += results.first[index].wins * factors[index];
        final.draws += results.first[index].draws * factors[index];
        final.losses += results.first[index].losses * factors[index];
        if(use_harmonic_mean)
        { final.points += factors[index] / results.first[index].points; }
        else
        { final.points += results.first[index].points * factors[index]; }
        final.sq_points += results.first[index].sq_points * factors[index] * factors[index];
    }
    long double factor_sum = std::accumulate(factors.begin(), factors.end(), 0.);
    final.wins /= factor_sum * (long double)results.second;
    final.draws /= factor_sum * (long double)results.second;
    final.losses /= factor_sum * (long double)results.second;
    if(use_harmonic_mean)
    { final.points = factor_sum / ((long double)results.second * final.points); }
    else
    { final.points /= factor_sum * (long double)results.second; }
    final.sq_points /= factor_sum * factor_sum * (long double)results.second;
    return final;
}
//------------------------------------------------------------------------------
volatile unsigned thread_num_iterations{0}; // written by threads
std::vector<Results<uint64_t>> thread_results; // written by threads
volatile unsigned thread_total{0}; // written by threads
volatile long double thread_prev_score{0.0};
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
    std::shared_ptr<Deck> your_deck;
    Hand your_hand;
    std::vector<std::shared_ptr<Deck>> enemy_decks;
    std::vector<Hand*> enemy_hands;
    std::vector<long double> factors;
    gamemode_t gamemode;
    enum Effect effect;
    SkillSpec bg_skill;

    SimulationData(unsigned seed, const Cards& cards_, const Decks& decks_, unsigned num_enemy_decks_, std::vector<long double> factors_, gamemode_t gamemode_,
            enum Effect effect_, SkillSpec bg_skill_) :
        re(seed),
        cards(cards_),
        decks(decks_),
        your_deck(),
        your_hand(nullptr),
        enemy_decks(num_enemy_decks_),
        factors(factors_),
        gamemode(gamemode_),
        effect(effect_),
        bg_skill(bg_skill_)
    {
        for (size_t i = 0; i < num_enemy_decks_; ++i)
        {
            enemy_hands.emplace_back(new Hand(nullptr));
        }
    }

    ~SimulationData()
    {
        for(auto hand: enemy_hands) { delete(hand); }
    }

    void set_decks(const Deck* const your_deck_, std::vector<Deck*> const & enemy_decks_)
    {
        your_deck.reset(your_deck_->clone());
        your_hand.deck = your_deck.get();
        for(unsigned i(0); i < enemy_decks_.size(); ++i)
        {
            enemy_decks[i].reset(enemy_decks_[i]->clone());
            enemy_hands[i]->deck = enemy_decks[i].get();
        }
    }

    inline std::vector<Results<uint64_t>> evaluate()
    {
        std::vector<Results<uint64_t>> res;
        for(Hand* enemy_hand: enemy_hands)
        {
            your_hand.reset(re);
            enemy_hand->reset(re);
            Field fd(re, cards, your_hand, *enemy_hand, gamemode, optimization_mode, effect != Effect::none ? effect : enemy_hand->deck->effect, bg_skill);
            Results<uint64_t> result(play(&fd));
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
                     const Process& p,
                     unsigned thread_id);
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
    Deck* your_deck;
    const std::vector<Deck*> enemy_decks;
    std::vector<long double> factors;
    gamemode_t gamemode;
    enum Effect effect;
    SkillSpec bg_skill;

    Process(unsigned num_threads_, const Cards& cards_, const Decks& decks_, Deck* your_deck_, std::vector<Deck*> enemy_decks_, std::vector<long double> factors_, gamemode_t gamemode_,
            enum Effect effect_, SkillSpec bg_skill_) :
        num_threads(num_threads_),
        main_barrier(num_threads+1),
        cards(cards_),
        decks(decks_),
        your_deck(your_deck_),
        enemy_decks(enemy_decks_),
        factors(factors_),
        gamemode(gamemode_),
        effect(effect_),
        bg_skill(bg_skill_)
    {
        destroy_threads = false;
        unsigned seed(time(0));
        for(unsigned i(0); i < num_threads; ++i)
        {
            threads_data.push_back(new SimulationData(seed + i, cards, decks, enemy_decks.size(), factors, gamemode, effect, bg_skill));
            threads.push_back(new boost::thread(thread_evaluate, std::ref(main_barrier), std::ref(shared_mutex), std::ref(*threads_data.back()), std::ref(*this), i));
        }
    }

    ~Process()
    {
        destroy_threads = true;
        main_barrier.wait();
        for(auto thread: threads) { thread->join(); }
        for(auto data: threads_data) { delete(data); }
    }

    std::pair<std::vector<Results<uint64_t>> , unsigned> evaluate(unsigned num_iterations)
    {
        thread_num_iterations = num_iterations;
        thread_results = std::vector<Results<uint64_t>>(enemy_decks.size());
        thread_total = 0;
        thread_compare = false;
        // unlock all the threads
        main_barrier.wait();
        // wait for the threads
        main_barrier.wait();
        return(std::make_pair(thread_results, thread_total));
    }

    std::pair<std::vector<Results<uint64_t>> , unsigned> compare(unsigned num_iterations, long double prev_score)
    {
        thread_num_iterations = num_iterations;
        thread_results = std::vector<Results<uint64_t>>(enemy_decks.size());
        thread_total = 0;
        thread_prev_score = prev_score;
        thread_compare = true;
        thread_compare_stop = false;
        // unlock all the threads
        main_barrier.wait();
        // wait for the threads
        main_barrier.wait();
        return(std::make_pair(thread_results, thread_total));
    }
};
//------------------------------------------------------------------------------
void thread_evaluate(boost::barrier& main_barrier,
                     boost::mutex& shared_mutex,
                     SimulationData& sim,
                     const Process& p,
                     unsigned thread_id)
{
    while(true)
    {
        main_barrier.wait();
        sim.set_decks(p.your_deck, p.enemy_decks);
        if(destroy_threads)
        { return; }
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
                std::vector<Results<uint64_t>> result{sim.evaluate()};
                shared_mutex.lock(); //<<<<
                std::vector<unsigned> thread_score_local(thread_results.size(), 0u); //!
                for(unsigned index(0); index < result.size(); ++index)
                {
                    thread_results[index] += result[index]; //!
                    thread_score_local[index] = thread_results[index].points; //!
                }
                ++thread_total; //!
                unsigned thread_total_local{thread_total}; //!
                shared_mutex.unlock(); //>>>>
                if(thread_compare && thread_id == 0 && thread_total_local > 1)
                {
                    unsigned score_accum = 0;
                    // Multiple defense decks case: scaling by factors and approximation of a "discrete" number of events.
                    if(result.size() > 1)
                    {
                        long double score_accum_d = 0.0;
                        for(unsigned i = 0; i < thread_score_local.size(); ++i)
                        {
                            score_accum_d += thread_score_local[i] * sim.factors[i];
                        }
                        score_accum_d /= std::accumulate(sim.factors.begin(), sim.factors.end(), .0);
                        score_accum = score_accum_d;
                    }
                    else
                    {
                        score_accum = thread_score_local[0];
                    }
                    bool compare_stop(false);
                    long double best_possible = 100;
                    // Get a loose (better than no) upper bound. TODO: Improve it.
                    compare_stop = (boost::math::binomial_distribution<>::find_upper_bound_on_p(thread_total_local, score_accum / best_possible, 0.01) * best_possible < thread_prev_score);
                    if(compare_stop)
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
void print_score_info(const std::pair<std::vector<Results<uint64_t>> , unsigned>& results, std::vector<long double>& factors)
{
    auto final = compute_score(results, factors);
    std::cout << final.points << " (";
    for(const auto & val: results.first)
    {
        switch(optimization_mode)
        {
            case OptimizationMode::raid:
                std::cout << val.points << " ";
                break;
            default:
                std::cout << val.points / 100 << " ";
                break;
        }
    }
    std::cout << "/ " << results.second << ")" << std::endl;
}
//------------------------------------------------------------------------------
void print_results(const std::pair<std::vector<Results<uint64_t>> , unsigned>& results, std::vector<long double>& factors)
{
    auto final = compute_score(results, factors);

    std::cout << "win%: " << final.wins * 100.0 << " (";
    for(const auto & val: results.first)
    {
        std::cout << val.wins << " ";
    }
    std::cout << "/ " << results.second << ")" << std::endl;

    std::cout << "stall%: " << final.draws * 100.0 << " (";
    for(const auto & val: results.first)
    {
        std::cout << val.draws << " ";
    }
    std::cout << "/ " << results.second << ")" << std::endl;

    std::cout << "loss%: " << final.losses * 100.0 << " (";
    for(const auto & val: results.first)
    {
        std::cout << val.losses << " ";
    }
    std::cout << "/ " << results.second << ")" << std::endl;

    switch(optimization_mode)
    {
        case OptimizationMode::raid:
            std::cout << "ard: " << final.points << " (";
            for(const auto & val: results.first)
            {
                std::cout << val.points << " ";
            }
            std::cout << "/ " << results.second << ")" << std::endl;
            if (show_stdev)
            {
                std::cout << "stdev: " << sqrt(final.sq_points - final.points * final.points) << std::endl;
            }
            break;
        default:
            break;
    }
}
//------------------------------------------------------------------------------
void print_deck_inline(const unsigned deck_cost, const Results<long double> score, Deck * deck)
{
    if(fund > 0)
    {
        std::cout << "$" << deck_cost << " ";
    }
    switch(optimization_mode)
    {
        case OptimizationMode::raid:
            std::cout << "(" << score.wins * 100 << "% win, " << score.draws * 100 << "% stall";
            if (show_stdev)
            {
                std::cout << ", " << sqrt(score.sq_points - score.points * score.points) << " stdev";
            }
            std::cout << ") ";
            break;
        case OptimizationMode::defense:
            std::cout << "(" << score.draws * 100.0 << "% stall) ";
            break;
        default:
            break;
    }
    std::cout << score.points << ": " << deck->commander->m_name;
    if (deck->strategy == DeckStrategy::random)
    {
        std::sort(deck->cards.begin(), deck->cards.end(), [](const Card* a, const Card* b) { return a->m_id < b->m_id; });
    }
    std::string last_name;
    unsigned num_repeat(0);
    for(const Card* card: deck->cards)
    {
        if(card->m_name == last_name)
        {
            ++ num_repeat;
        }
        else
        {
            if(num_repeat > 1)
            {
                std::cout << " #" << num_repeat;
            }
            std::cout << ", " << card->m_name;
            last_name = card->m_name;
            num_repeat = 1;
        }
    }
    if(num_repeat > 1)
    {
        std::cout << " #" << num_repeat;
    }
    std::cout << std::endl;
}
//------------------------------------------------------------------------------
void hill_climbing(unsigned num_iterations, Deck* d1, Process& proc, std::map<signed, char> card_marks)
{
    auto results = proc.evaluate(num_iterations);
    print_score_info(results, proc.factors);
    auto current_score = compute_score(results, proc.factors);
    auto best_score = current_score;
    std::map<std::multiset<unsigned>, unsigned> evaluated_decks{{d1->card_ids<std::multiset<unsigned>>(),  num_iterations}};
    // Non-commander cards
    auto non_commander_cards = proc.cards.player_assaults;
    non_commander_cards.insert(non_commander_cards.end(), proc.cards.player_structures.begin(), proc.cards.player_structures.end());
    non_commander_cards.insert(non_commander_cards.end(), proc.cards.player_actions.begin(), proc.cards.player_actions.end());
    non_commander_cards.insert(non_commander_cards.end(), std::initializer_list<Card *>{NULL,});
    const Card* best_commander = d1->commander;
    std::vector<const Card*> best_cards = d1->cards;
    unsigned deck_cost = get_deck_cost(d1);
    fund = std::max(fund, deck_cost);
    print_deck_inline(deck_cost, best_score, d1);
    std::mt19937 re(std::chrono::system_clock::now().time_since_epoch().count());
    bool deck_has_been_improved = true;
    unsigned long skipped_simulations = 0;
    std::vector<std::pair<signed, const Card *>> cards_out, cards_in;
    for(unsigned slot_i(0), dead_slot(0); (deck_has_been_improved || slot_i != dead_slot) && best_score.points - target_score < -1e-9; slot_i = (slot_i + 1) % std::min<unsigned>(max_deck_len, best_cards.size() + 1))
    {
        if(deck_has_been_improved)
        {
            dead_slot = slot_i;
            deck_has_been_improved = false;
        }
        if(!card_marks.count(-1))
        {
            for(const Card* commander_candidate: proc.cards.player_commanders)
            {
                // Various checks to check if the card is accepted
                assert(commander_candidate->m_type == CardType::commander);
                if(commander_candidate->m_name == best_commander->m_name)
                { continue; }
                d1->cards = best_cards;
                // Place it in the deck and restore other cards
                cards_out.clear();
                cards_out.emplace_back(-1, best_commander);
                cards_out = {{-1, best_commander}};
                d1->commander = commander_candidate;
                if (! adjust_deck(d1, -1, -1, nullptr, fund, re, deck_cost, cards_out, cards_in))
                { continue; }
                auto &&cur_deck = d1->card_ids<std::multiset<unsigned>>();
                if (evaluated_decks.count(cur_deck) > 0)
                {
                    skipped_simulations += evaluated_decks[cur_deck];
                    continue;
                }
                // Evaluate new deck
                auto compare_results = proc.compare(num_iterations, best_score.points);
                current_score = compute_score(compare_results, proc.factors);
                evaluated_decks[cur_deck] = compare_results.second;
                // Is it better ?
                if(current_score.points > best_score.points)
                {
                    std::cout << "Deck improved: " << d1->hash() << ": " << card_slot_id_names(cards_out) << " -> " << card_slot_id_names(cards_in) << ": ";
                    // Then update best score/commander, print stuff
                    best_score = current_score;
                    best_commander = d1->commander;
                    best_cards = d1->cards;
                    deck_has_been_improved = true;
                    print_score_info(compare_results, proc.factors);
                    print_deck_inline(deck_cost, best_score, d1);
                }
            }
            // Now that all commanders are evaluated, take the best one
            d1->commander = best_commander;
            d1->cards = best_cards;
        }
        std::shuffle(non_commander_cards.begin(), non_commander_cards.end(), re);
        for(const Card* card_candidate: non_commander_cards)
        {
            d1->commander = best_commander;
            d1->cards = best_cards;
            if (card_candidate ?
                    (slot_i < best_cards.size() && card_candidate->m_name == best_cards[slot_i]->m_name) // Omega -> Omega
                    :
                     (slot_i == best_cards.size()))  // void -> void
            { continue; }
            cards_out.clear();
            if (slot_i < d1->cards.size())
            {
                cards_out.emplace_back(-1, d1->cards[slot_i]);
                d1->cards.erase(d1->cards.begin() + slot_i);
            }
            if (! adjust_deck(d1, slot_i, slot_i, card_candidate, fund, re, deck_cost, cards_out, cards_in) ||
                    d1->cards.size() < min_deck_len)
            { continue; }
            auto &&cur_deck = d1->card_ids<std::multiset<unsigned>>();
            if (evaluated_decks.count(cur_deck) > 0)
            {
                skipped_simulations += evaluated_decks[cur_deck];
                continue;
            }
            // Evaluate new deck
            auto compare_results = proc.compare(num_iterations, best_score.points);
            current_score = compute_score(compare_results, proc.factors);
            evaluated_decks[cur_deck] = compare_results.second;
            // Is it better ?
            if(current_score.points > best_score.points)
            {
                std::cout << "Deck improved: " << d1->hash() << ": " << card_slot_id_names(cards_out) << " -> " << card_slot_id_names(cards_in) << ": ";
                // Then update best score/slot, print stuff
                best_score = current_score;
                best_commander = d1->commander;
                best_cards = d1->cards;
                deck_has_been_improved = true;
                print_score_info(compare_results, proc.factors);
                print_deck_inline(deck_cost, best_score, d1);
            }
            if(best_score.points - target_score > -1e-9)
            { break; }
        }
        d1->commander = best_commander;
        d1->cards = best_cards;
    }
    unsigned simulations = 0;
    for(auto evaluation: evaluated_decks)
    { simulations += evaluation.second; }
    std::cout << "Evaluated " << evaluated_decks.size() << " decks (" << simulations << " + " << skipped_simulations << " simulations)." << std::endl;
    std::cout << "Optimized Deck: ";
    print_deck_inline(get_deck_cost(d1), best_score, d1);
}
//------------------------------------------------------------------------------
void hill_climbing_ordered(unsigned num_iterations, Deck* d1, Process& proc, std::map<signed, char> card_marks)
{
    auto results = proc.evaluate(num_iterations);
    print_score_info(results, proc.factors);
    auto current_score = compute_score(results, proc.factors);
    auto best_score = current_score;
    std::map<std::vector<unsigned>, unsigned> evaluated_decks{{d1->card_ids<std::vector<unsigned>>(), num_iterations}};
    // Non-commander cards
    auto non_commander_cards = proc.cards.player_assaults;
    non_commander_cards.insert(non_commander_cards.end(), proc.cards.player_structures.begin(), proc.cards.player_structures.end());
    non_commander_cards.insert(non_commander_cards.end(), proc.cards.player_actions.begin(), proc.cards.player_actions.end());
    non_commander_cards.insert(non_commander_cards.end(), std::initializer_list<Card *>{NULL,});
    const Card* best_commander = d1->commander;
    std::vector<const Card*> best_cards = d1->cards;
    unsigned deck_cost = get_deck_cost(d1);
    fund = std::max(fund, deck_cost);
    print_deck_inline(deck_cost, best_score, d1);
    std::mt19937 re(std::chrono::system_clock::now().time_since_epoch().count());
    bool deck_has_been_improved = true;
    unsigned long skipped_simulations = 0;
    std::vector<std::pair<signed, const Card *>> cards_out, cards_in;
    for(unsigned from_slot(0), dead_slot(0); (deck_has_been_improved || from_slot != dead_slot) && best_score.points - target_score < -1e-9; from_slot = (from_slot + 1) % std::min<unsigned>(max_deck_len, d1->cards.size() + 1))
    {
        if(deck_has_been_improved)
        {
            dead_slot = from_slot;
            deck_has_been_improved = false;
        }
        if(!card_marks.count(-1))
        {
            for(const Card* commander_candidate: proc.cards.player_commanders)
            {
                if(best_score.points - target_score > -1e-9)
                { break; }
                // Various checks to check if the card is accepted
                assert(commander_candidate->m_type == CardType::commander);
                if(commander_candidate->m_name == best_commander->m_name)
                { continue; }
                d1->cards = best_cards;
                // Place it in the deck
                cards_out.clear();
                cards_out.emplace_back(-1, best_commander);
                d1->commander = commander_candidate;
                if (! adjust_deck(d1, -1, -1, nullptr, fund, re, deck_cost, cards_out, cards_in))
                { continue; }
                auto &&cur_deck = d1->card_ids<std::vector<unsigned>>();
                if (evaluated_decks.count(cur_deck) > 0)
                {
                    skipped_simulations += evaluated_decks[cur_deck];
                    continue;
                }
                // Evaluate new deck
                auto compare_results = proc.compare(num_iterations, best_score.points);
                current_score = compute_score(compare_results, proc.factors);
                evaluated_decks[cur_deck] = compare_results.second;
                // Is it better ?
                if(current_score.points > best_score.points)
                {
                    std::cout << "Deck improved: " << d1->hash() << ": " << card_slot_id_names(cards_out) << " -> " << card_slot_id_names(cards_in) << ": ";
                    // Then update best score/commander, print stuff
                    best_score = current_score;
                    best_commander = commander_candidate;
                    best_cards = d1->cards;
                    deck_has_been_improved = true;
                    print_score_info(compare_results, proc.factors);
                    print_deck_inline(deck_cost, best_score, d1);
                }
            }
            // Now that all commanders are evaluated, take the best one
            d1->commander = best_commander;
            d1->cards = best_cards;
        }
        std::shuffle(non_commander_cards.begin(), non_commander_cards.end(), re);
        for(const Card* card_candidate: non_commander_cards)
        {
            // Various checks to check if the card is accepted
            assert(!card_candidate || card_candidate->m_type != CardType::commander);
            for(unsigned to_slot(card_candidate ? 0 : best_cards.size() - 1); to_slot < best_cards.size() + (from_slot < best_cards.size() ? 0 : 1); ++to_slot)
            {
                d1->commander = best_commander;
                d1->cards = best_cards;
                if (card_candidate ?
                        (from_slot < best_cards.size() && (from_slot == to_slot && card_candidate->m_name == best_cards[to_slot]->m_name)) // 2 Omega -> 2 Omega
                        :
                         (from_slot == best_cards.size())) // void -> void
                { continue; }
                cards_out.clear();
                if (from_slot < d1->cards.size())
                {
                    cards_out.emplace_back(from_slot, d1->cards[from_slot]);
                    d1->cards.erase(d1->cards.begin() + from_slot);
                }
                if (! adjust_deck(d1, from_slot, to_slot, card_candidate, fund, re, deck_cost, cards_out, cards_in) ||
                        d1->cards.size() < min_deck_len)
                { continue; }
                auto &&cur_deck = d1->card_ids<std::vector<unsigned>>();
                if (evaluated_decks.count(cur_deck) > 0)
                {
                    skipped_simulations += evaluated_decks[cur_deck];
                    continue;
                }
                // Evaluate new deck
                auto compare_results = proc.compare(num_iterations, best_score.points);
                current_score = compute_score(compare_results, proc.factors);
                evaluated_decks[cur_deck] = compare_results.second;
                // Is it better ?
                if(current_score.points > best_score.points)
                {
                    // Then update best score/slot, print stuff
                    std::cout << "Deck improved: " << d1->hash() << ": " << card_slot_id_names(cards_out) << " -> " << card_slot_id_names(cards_in) << ": ";
                    best_score = current_score;
                    best_commander = d1->commander;
                    best_cards = d1->cards;
                    deck_has_been_improved = true;
                    print_score_info(compare_results, proc.factors);
                    print_deck_inline(deck_cost, best_score, d1);
                }
            }
            if(best_score.points - target_score > -1e-9)
            { break; }
        }
        d1->commander = best_commander;
        d1->cards = best_cards;
    }
    unsigned simulations = 0;
    for(auto evaluation: evaluated_decks)
    { simulations += evaluation.second; }
    std::cout << "Evaluated " << evaluated_decks.size() << " decks (" << simulations << " + " << skipped_simulations << " simulations)." << std::endl;
    std::cout << "Optimized Deck: ";
    print_deck_inline(get_deck_cost(d1), best_score, d1);
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
        // The end condition's a bit odd here, but index is unsigned.
        // The last iteration is when index = 0.
        // After that, index = max int, which is clearly >= choose.
        for(index = choose - 1; index < choose; --index)
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
    unsigned index;
    unsigned nextIndex;
};
//------------------------------------------------------------------------------
enum Operation {
    simulate,
    climb,
    reorder,
    debug,
    debuguntil
};
//------------------------------------------------------------------------------
void print_available_effects()
{
    std::cout << "Available effects besides \"<skill> X\":" << std::endl;
    for(int i(1); i < Effect::num_effects; ++ i)
    {
        std::cout << i << " \"" << effect_names[i] << "\"" << std::endl;
    }
}

void usage(int argc, char** argv)
{
    std::cout << "Tyrant Unleashed Optimizer (TUO) " << TYRANT_OPTIMIZER_VERSION << "\n"
        "usage: " << argv[0] << " Your_Deck Enemy_Deck [Flags] [Operations]\n"
        "\n"
        "Your_Deck:\n"
        "  the name/hash/cards of a custom deck.\n"
        "\n"
        "Enemy_Deck:\n"
        "  semicolon separated list of defense decks, syntax:\n"
        "  deck1[:factor1];deck2[:factor2];...\n"
        "  where deck is the name/hash/cards of a mission, raid, quest or custom deck, and factor is optional. The default factor is 1.\n"
        "  example: \'fear:0.2;slowroll:0.8\' means fear is the defense deck 20% of the time, while slowroll is the defense deck 80% of the time.\n"
        "\n"
        "Flags:\n"
        "  -e <effect>: set the battleground effect. effect is automatically set when applicable.\n"
        "  -r: the attack deck is played in order instead of randomly (respects the 3 cards drawn limit).\n"
        "  -s: use surge (default is fight).\n"
        "  -t <num>: set the number of threads, default is 4.\n"
        "  win:     simulate/optimize for win rate. default for non-raids.\n"
        "  defense: simulate/optimize for win rate + stall rate. can be used for defending deck or win rate oriented raid simulations.\n"
        "  raid:    simulate/optimize for average raid damage (ARD). default for raids.\n"
        "Flags for climb:\n"
        "  -c: don't try to optimize the commander.\n"
        "  -L <min> <max>: restrict deck size between <min> and <max>.\n"
        "  -o: restrict to the owned cards listed in \"data/ownedcards.txt\".\n"
        "  -o=<filename>: restrict to the owned cards listed in <filename>.\n"
        "  fund <num>: invest <num> SP to upgrade cards.\n"
        "  target <num>: stop as soon as the score reaches <num>.\n"
        "\n"
        "Operations:\n"
        "  sim <num>: simulate <num> battles to evaluate a deck.\n"
        "  climb <num>: perform hill-climbing starting from the given attack deck, using up to <num> battles to evaluate a deck.\n"
        "  reorder <num>: optimize the order for given attack deck, using up to <num> battles to evaluate an order.\n"
#ifndef NDEBUG
        "  debug: testing purpose only. very verbose output. only one battle.\n"
        "  debuguntil <min> <max>: testing purpose only. fight until the last fight results in range [<min>, <max>]. recommend to redirect output.\n"
#endif
        ;
}

std::string skill_description(const Cards& cards, const SkillSpec& s);

int main(int argc, char** argv)
{
    if (argc == 2 && strcmp(argv[1], "-version") == 0)
    {
        std::cout << "Tyrant Unleashed Optimizer " << TYRANT_OPTIMIZER_VERSION << std::endl;
        return 0;
    }
    if (argc <= 2)
    {
        usage(argc, argv);
        return 0;
    }

    unsigned opt_num_threads(4);
    DeckStrategy::DeckStrategy opt_your_strategy(DeckStrategy::random);
    DeckStrategy::DeckStrategy opt_enemy_strategy(DeckStrategy::random);
    std::string opt_forts, opt_enemy_forts;
    std::string opt_hand, opt_enemy_hand;
    std::vector<std::string> opt_owned_cards_str_list;
    bool opt_do_optimization(false);
    bool opt_keep_commander{false};
    std::vector<std::tuple<unsigned, unsigned, Operation>> opt_todo;
    std::string opt_effect;
    enum Effect opt_effect_id(Effect::none);
    SkillSpec opt_bg_skill{no_skill, 0, allfactions, 0, 0, no_skill, false};

    for(int argIndex = 3; argIndex < argc; ++argIndex)
    {
        // Codec
        if (strcmp(argv[argIndex], "ext_b64") == 0)
        {
            hash_to_ids = hash_to_ids_ext_b64;
            encode_deck = encode_deck_ext_b64;
        }
        else if (strcmp(argv[argIndex], "wmt_b64") == 0)
        {
            hash_to_ids = hash_to_ids_wmt_b64;
            encode_deck = encode_deck_wmt_b64;
        }
        else if (strcmp(argv[argIndex], "ddd_b64") == 0)
        {
            hash_to_ids = hash_to_ids_ddd_b64;
            encode_deck = encode_deck_ddd_b64;
        }
        // Base Game Mode
        else if (strcmp(argv[argIndex], "fight") == 0)
        {
            gamemode = fight;
        }
        else if (strcmp(argv[argIndex], "-s") == 0 || strcmp(argv[argIndex], "surge") == 0)
        {
            gamemode = surge;
        }
        // Base Scoring Mode
        else if (strcmp(argv[argIndex], "win") == 0)
        {
            optimization_mode = OptimizationMode::winrate;
        }
        else if (strcmp(argv[argIndex], "defense") == 0)
        {
            optimization_mode = OptimizationMode::defense;
        }
        else if (strcmp(argv[argIndex], "raid") == 0)
        {
            optimization_mode = OptimizationMode::raid;
        }
        // Mode Package
        else if (strcmp(argv[argIndex], "pvp") == 0)
        {
            gamemode = fight;
            optimization_mode = OptimizationMode::winrate;
        }
        else if (strcmp(argv[argIndex], "pvp-defense") == 0)
        {
            gamemode = surge;
            optimization_mode = OptimizationMode::defense;
        }
        else if (strcmp(argv[argIndex], "gw") == 0)
        {
            gamemode = surge;
            optimization_mode = OptimizationMode::winrate;
        }
        else if (strcmp(argv[argIndex], "gw-defense") == 0)
        {
            gamemode = fight;
            optimization_mode = OptimizationMode::defense;
        }
        // Others
        else if (strcmp(argv[argIndex], "keep-commander") == 0 || strcmp(argv[argIndex], "-c") == 0)
        {
            opt_keep_commander = true;
        }
        else if (strcmp(argv[argIndex], "effect") == 0 || strcmp(argv[argIndex], "-e") == 0)
        {
            opt_effect = argv[argIndex + 1];
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "-L") == 0)
        {
            min_deck_len = atoi(argv[argIndex + 1]);
            max_deck_len = atoi(argv[argIndex + 2]);
            argIndex += 2;
        }
        else if(strcmp(argv[argIndex], "-o-") == 0)
        {
            use_owned_cards = false;
        }
        else if(strcmp(argv[argIndex], "-o") == 0)
        {
            opt_owned_cards_str_list.push_back("data/ownedcards.txt");
            use_owned_cards = true;
        }
        else if(strncmp(argv[argIndex], "-o=", 3) == 0)
        {
            opt_owned_cards_str_list.push_back(argv[argIndex] + 3);
            use_owned_cards = true;
        }
        else if(strcmp(argv[argIndex], "fund") == 0)
        {
            fund = atoi(argv[argIndex+1]);
            argIndex += 1;
        }
        else if (strcmp(argv[argIndex], "random") == 0)
        {
            opt_your_strategy = DeckStrategy::random;
        }
        else if(strcmp(argv[argIndex], "-r") == 0 || strcmp(argv[argIndex], "ordered") == 0)
        {
            opt_your_strategy = DeckStrategy::ordered;
        }
        else if(strcmp(argv[argIndex], "exact-ordered") == 0)
        {
            opt_your_strategy = DeckStrategy::exact_ordered;
        }
        else if(strcmp(argv[argIndex], "enemy:ordered") == 0)
        {
            opt_enemy_strategy = DeckStrategy::ordered;
        }
        else if(strcmp(argv[argIndex], "enemy:exact-ordered") == 0)
        {
            opt_enemy_strategy = DeckStrategy::exact_ordered;
        }
        else if(strcmp(argv[argIndex], "threads") == 0 || strcmp(argv[argIndex], "-t") == 0)
        {
            opt_num_threads = atoi(argv[argIndex+1]);
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "target") == 0)
        {
            target_score = atof(argv[argIndex+1]);
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "turnlimit") == 0)
        {
            turn_limit = atoi(argv[argIndex+1]);
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "+stdev") == 0)
        {
            show_stdev = true;
        }
        else if(strcmp(argv[argIndex], "+hm") == 0)
        {
            use_harmonic_mean = true;
        }
        else if(strcmp(argv[argIndex], "-v") == 0)
        {
            -- debug_print;
        }
        else if(strcmp(argv[argIndex], "+v") == 0)
        {
            ++ debug_print;
        }
        else if(strcmp(argv[argIndex], "hand") == 0)  // set initial hand for test
        {
            opt_hand = argv[argIndex + 1];
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "enemy:hand") == 0)  // set enemies' initial hand for test
        {
            opt_enemy_hand = argv[argIndex + 1];
            argIndex += 1;
        }
        else if (strcmp(argv[argIndex], "yf") == 0 || strcmp(argv[argIndex], "yfort") == 0)  // set forts
        {
            opt_forts = std::string(argv[argIndex + 1]);
            argIndex += 1;
        }
        else if (strcmp(argv[argIndex], "ef") == 0 || strcmp(argv[argIndex], "efort") == 0)  // set enemies' forts
        {
            opt_enemy_forts = std::string(argv[argIndex + 1]);
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "sim") == 0)
        {
            opt_todo.push_back(std::make_tuple((unsigned)atoi(argv[argIndex + 1]), 0u, simulate));
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "climb") == 0)
        {
            opt_todo.push_back(std::make_tuple((unsigned)atoi(argv[argIndex + 1]), 0u, climb));
            opt_do_optimization = true;
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "reorder") == 0)
        {
            opt_todo.push_back(std::make_tuple((unsigned)atoi(argv[argIndex + 1]), 0u, reorder));
            argIndex += 1;
        }
        else if(strcmp(argv[argIndex], "debug") == 0)
        {
            opt_todo.push_back(std::make_tuple(0u, 0u, debug));
        }
        else if(strcmp(argv[argIndex], "debuguntil") == 0)
        {
            // output the debug info for the first battle that min_score <= score <= max_score.
            // E.g., 0 0: lose; 100 100: win (non-raid); 20 100: at least 20 damage (raid).
            opt_todo.push_back(std::make_tuple((unsigned)atoi(argv[argIndex + 1]), (unsigned)atoi(argv[argIndex + 2]), debuguntil));
            argIndex += 2;
        }
        else
        {
            std::cerr << "Error: Unknown option " << argv[argIndex] << std::endl;
            return 0;
        }
    }

    Cards all_cards;
    Decks decks;
    load_cards_xml(all_cards, "data/cards.xml");
    read_card_abbrs(all_cards, "data/cardabbrs.txt");
    load_decks_xml(decks, all_cards, "data/missions.xml", "data/raids.xml");
    load_custom_decks(decks, all_cards, "data/customdecks.txt");
    load_recipes_xml(all_cards, "data/fusion_recipes_cj2.xml");
    fill_skill_table();

    if (opt_do_optimization and use_owned_cards)
    {
        if (opt_owned_cards_str_list.empty())
        {  // load default file is specify no file
            opt_owned_cards_str_list.push_back("data/ownedcards.txt");
        }
        for (const auto & oc_str: opt_owned_cards_str_list)
        {
            read_owned_cards(all_cards, owned_cards, oc_str);
        }
    }

    if (! opt_effect.empty())
    {
        std::map<std::string, int> effect_map;
        for(unsigned i(0); i < Effect::num_effects; ++i)
        {
            effect_map[boost::to_lower_copy(effect_names[i])] = static_cast<enum Effect>(i);
            std::stringstream ss;
            ss << i;
            effect_map[ss.str()] = static_cast<enum Effect>(i);
        }
        std::vector<std::string> tokens;
        boost::split(tokens, opt_effect, boost::is_any_of(" -"));

        opt_bg_skill.id = skill_name_to_id(tokens[0], false);
        unsigned skill_index = 1;
        if (tokens.size() >= 2 && opt_bg_skill.id != no_skill)
        {
            try
            {
                if (skill_index < tokens.size() && tokens[skill_index] == "all")
                {
                    opt_bg_skill.all = true;
                    skill_index += 1;
                }
                if (skill_index < tokens.size())
                {
                    opt_bg_skill.s = skill_name_to_id(tokens[skill_index], false);
                    if (opt_bg_skill.s != no_skill)
                    {
                        skill_index += 1;
                    }
                }
                if (skill_index < tokens.size())
                {
                    opt_bg_skill.x = boost::lexical_cast<unsigned>(tokens[skill_index]);
                }
            }
            catch (const boost::bad_lexical_cast & e)
            {
                std::cerr << "Error: Expect a number in effect \"" << opt_effect << "\".\n";
                return 0;
            }
            catch (std::exception & e)
            {
                std::cerr << "Error: effect \"" << opt_effect << ": " << e.what() << "\".\n";
                return 0;
            }
        }
        else
        {
            const auto & x = effect_map.find(boost::to_lower_copy(opt_effect));
            if(x == effect_map.end())
            {
                std::cout << "Error: The effect \"" << opt_effect << "\" was not found.\n";
                print_available_effects();
                return 0;
            }
            opt_effect_id = static_cast<enum Effect>(x->second);
        }
    }

    std::string your_deck_name{argv[1]};
    std::string enemy_deck_list{argv[2]};
    auto && deck_list_parsed = parse_deck_list(enemy_deck_list, decks);

    Deck* your_deck{nullptr};
    std::vector<Deck*> enemy_decks;
    std::vector<long double> enemy_decks_factors;

    try
    {
        your_deck = find_deck(decks, all_cards, your_deck_name)->clone();
    }
    catch(const std::runtime_error& e)
    {
        std::cerr << "Error: Deck " << your_deck_name << ": " << e.what() << std::endl;
        return 0;
    }
    if(your_deck == nullptr)
    {
        std::cerr << "Error: Invalid attack deck name/hash " << your_deck_name << ".\n";
    }
    else if(!your_deck->raid_cards.empty())
    {
        std::cerr << "Error: Invalid attack deck " << your_deck_name << ": has optional cards.\n";
        your_deck = nullptr;
    }
    if(your_deck == nullptr)
    {
        usage(argc, argv);
        return 0;
    }

    your_deck->strategy = opt_your_strategy;
    if (!opt_forts.empty())
    {
        try
        {
            your_deck->set_forts(opt_forts + ",");
        }
        catch(const std::runtime_error& e)
        {
            std::cerr << "Error: yf " << opt_forts << ": " << e.what() << std::endl;
            return 0;
        }
    }
    your_deck->set_given_hand(opt_hand);
    if (opt_keep_commander)
    {
        your_deck->card_marks[-1] = '!';
    }

    for(auto deck_parsed: deck_list_parsed)
    {
        Deck* enemy_deck{nullptr};
        try
        {
            enemy_deck = find_deck(decks, all_cards, deck_parsed.first);
        }
        catch(const std::runtime_error& e)
        {
            std::cerr << "Error: Deck " << deck_parsed.first << ": " << e.what() << std::endl;
            return 0;
        }
        if(enemy_deck == nullptr)
        {
            std::cerr << "Error: Invalid defense deck name/hash " << deck_parsed.first << ".\n";
            usage(argc, argv);
            return 0;
        }
        if (optimization_mode == OptimizationMode::notset)
        {
            if (enemy_deck->decktype == DeckType::raid)
            {
                optimization_mode = OptimizationMode::raid;
            }
            else
            {
                optimization_mode = OptimizationMode::winrate;
            }
        }
        enemy_deck->strategy = opt_enemy_strategy;
        if (!opt_enemy_forts.empty())
        {
            try
            {
                enemy_deck->set_forts(opt_enemy_forts + ",");
            }
            catch(const std::runtime_error& e)
            {
                std::cerr << "Error: yf " << opt_forts << ": " << e.what() << std::endl;
                return 0;
            }
        }
        enemy_deck->set_given_hand(opt_enemy_hand);
        enemy_decks.push_back(enemy_deck);
        enemy_decks_factors.push_back(deck_parsed.second);
    }

    // Force to claim cards in your initial deck.
    if (opt_do_optimization and use_owned_cards)
    {
        claim_cards({your_deck->commander});
        claim_cards(your_deck->cards);
    }

    if (debug_print >= 0)
    {
        std::cout << "Your Deck: " << (debug_print > 0 ? your_deck->long_description() : your_deck->medium_description()) << std::endl;
        for (unsigned i(0); i < enemy_decks.size(); ++i)
        {
            std::cout << "Enemy's Deck:" << enemy_decks_factors[i] << ": " << (debug_print > 0 ? enemy_decks[i]->long_description() : enemy_decks[i]->medium_description()) << std::endl;
        }
        if(opt_effect_id != Effect::none)
        {
            std::cout << "Effect: " << effect_names[opt_effect_id] << std::endl;
        }
        else if(opt_bg_skill.id != no_skill)
        {
            std::cout << "Effect: " << skill_description(all_cards, opt_bg_skill) << std::endl;
        }
    }

    Process p(opt_num_threads, all_cards, decks, your_deck, enemy_decks, enemy_decks_factors, gamemode, opt_effect_id, opt_bg_skill);

    {
        //ScopeClock timer;
        for(auto op: opt_todo)
        {
            switch(std::get<2>(op))
            {
            case simulate: {
                auto results = p.evaluate(std::get<0>(op));
                print_results(results, p.factors);
                break;
            }
            case climb: {
                switch (opt_your_strategy)
                {
                case DeckStrategy::random:
                    hill_climbing(std::get<0>(op), your_deck, p, your_deck->card_marks);
                    break;
//                case DeckStrategy::ordered:
//                case DeckStrategy::exact_ordered:
                default:
                    hill_climbing_ordered(std::get<0>(op), your_deck, p, your_deck->card_marks);
                    break;
                }
                break;
            }
            case reorder: {
                your_deck->strategy = DeckStrategy::ordered;
                use_owned_cards = true;
                if (min_deck_len == 1 && max_deck_len == 10)
                {
                    min_deck_len = max_deck_len = your_deck->cards.size();
                }
                fund = 0;
                owned_cards.clear();
                claim_cards({your_deck->commander});
                claim_cards(your_deck->cards);
                hill_climbing_ordered(std::get<0>(op), your_deck, p, your_deck->card_marks);
                break;
            }
            case debug: {
                ++ debug_print;
                debug_str.clear();
                auto results = p.evaluate(1);
                print_results(results, p.factors);
                -- debug_print;
                break;
            }
            case debuguntil: {
                ++ debug_print;
                ++ debug_cached;
                while(1)
                {
                    debug_str.clear();
                    auto results = p.evaluate(1);
                    auto score = compute_score(results, p.factors);
                    if(score.points >= std::get<0>(op) && score.points <= std::get<1>(op))
                    {
                        std::cout << debug_str << std::flush;
                        print_results(results, p.factors);
                        break;
                    }
                }
                -- debug_cached;
                -- debug_print;
                break;
            }
            }
        }
    }
    return 0;
}
