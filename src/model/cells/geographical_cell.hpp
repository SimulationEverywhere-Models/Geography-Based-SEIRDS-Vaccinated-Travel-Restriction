// Created by binybrion - 06/29/20
// Modified by Glenn    - 02/07/20

#ifndef PANDEMIC_HOYA_2002_ZHONG_CELL_HPP
#define PANDEMIC_HOYA_2002_ZHONG_CELL_HPP

#include <cmath>
#include <iostream>
#include <vector>
#include <cadmium/celldevs/cell/cell.hpp>
#include <iomanip>
#include "vicinity.hpp"
#include "sevirds.hpp"
#include "simulation_config.hpp"

using namespace std;
using namespace cadmium::celldevs;

template <typename T>
class geographical_cell : public cell<T, string, sevirds, vicinity>
{
    public:
        template <typename X>
        using cell_unordered = unordered_map<string, X>;

        using cell<T, string, sevirds, vicinity>::simulation_clock;
        using cell<T, string, sevirds, vicinity>::state;
        using cell<T, string, sevirds, vicinity>::neighbors;
        using cell<T, string, sevirds, vicinity>::cell_id;

        using config_type = simulation_config;

        using phase_rates = vector<             // The age sub_division
                            vector<double>>;    // The stage of infection

        phase_rates virulence_rates;
        phase_rates incubation_rates;
        phase_rates recovery_rates;
        phase_rates mobility_rates;
        phase_rates fatality_rates;

        vector<double>  vac1_rates;
        phase_rates     vac2_rates;

        // To make the parameters of the correction_factors variable more obvious
        using infection_threshold           = float;
        using mobility_correction_factor    = array<float, 2>;    // The first value is the mobility correction factor;
                                                                    // The second one is the hysteresis factor.

        int prec_divider;
        double one_over_prec_divider;
        bool SIIRS_model, is_vaccination;

        geographical_cell() : cell<T, string, sevirds, vicinity>() {}

        geographical_cell(string const& cell_id, cell_unordered<vicinity> const& neighborhood,
                            sevirds const& initial_state, string const& delay_id, simulation_config config) :
            cell<T, string, sevirds, vicinity>(cell_id, neighborhood, initial_state, delay_id)
        {
            for (const auto& i : neighborhood)
                state.current_state.hysteresis_factors.insert({i.first, hysteresis_factor{}});

            virulence_rates     = move(config.virulence_rates);
            incubation_rates    = move(config.incubation_rates);
            recovery_rates      = move(config.recovery_rates);
            mobility_rates      = move(config.mobility_rates);
            fatality_rates      = move(config.fatality_rates);

            vac1_rates = move(config.vac1_rates);
            vac2_rates = move(config.vac2_rates);

            one_over_prec_divider   = 1.0 / (double)config.prec_divider; // Multiplication is always faster then division so set this up to be 1/prec_divider to be multiplied later
            prec_divider            = config.prec_divider;
            SIIRS_model             = config.SIIRS_model;
            is_vaccination          = config.is_vaccination;

            assert(virulence_rates.size() == recovery_rates.size() && virulence_rates.size() == mobility_rates.size() &&
                virulence_rates.size() == incubation_rates.size() &&
                "\n\nThere must be an equal number of age segments between all configuration rates.\n\n");
        }

        // Whenever referring to a "population", it is meant the current age group's population.
        // The state of each age group's population is calculated individually.
        sevirds local_computation() const override
        {
            sevirds res = state.current_state;

            vector<double> fatalities, recovered;
            double new_s, new_vac1, new_vac2, new_e, new_i, curr_vac1, curr_vac2;
            double curr_expos, curr_inf, res_fatalities;
            unsigned int recovered_index;
            unsigned int age_segments       = res.get_num_age_segments();
            unsigned int vac1_phases        = res.get_num_vaccinated1_phases() - 1;
            unsigned int vac2_phases        = res.get_num_vaccinated2_phases() - 1;
            unsigned int exposed_phases     = res.get_num_exposed_phases();
            unsigned int infected_phases    = res.get_num_infected_phases();
            unsigned int recovered_phases   = res.get_num_recovered_phases();

            // Calculate the next new sevirds variables for each age group
            for (unsigned int age_segment_index = 0; age_segment_index < age_segments; ++age_segment_index)
            {
                /* Note: Remember that these recoveries and fatalities are from the previous simulation cycle. Thus there is an ordering
                    issue- recovery rate and fatality rates specify a percentage of the infected at a certain stage. As a result
                    the code cannot for example, calculate recovery, change the infected stage population, and then calculate
                    fatalities, or vice-versa. This would change the meaning of the input.
                */

                /* Calculate fatalities BEFORE recoveries. This avoids the following problem: Due to the fatality modifier (when
                    hospitals are overwhelmed), a check to make sure fatalities do not exceed the infected population has to be done.
                    This conflicts with the recovery logic, where it is assumed that all infected on the last stage recover. As a result,
                    the logic to ensure that the recovered + fatalities for the last stage do not exceed the last stage infected population
                    results in no fatalities being possible on the last stage of infection (as fatalities would have to be capped at
                    infected population [for a stage] - recovered population [for a stage]. But the last stage of recovered
                    was already set to the population of last stage of infected- meaning fatalities is always 0 for the last stage).
                */

                // Get these ahead of time for performance
                res_fatalities                              = res.fatalities.at(age_segment_index);
                vector<double>& res_vaccinated1             = res.vaccinatedD1.at(age_segment_index);
                vector<double>& res_vaccinated2             = res.vaccinatedD2.at(age_segment_index);
                vector<double>& res_exposed                 = res.exposed.at(age_segment_index);
                vector<double>& res_infected                = res.infected.at(age_segment_index);
                vector<double>& res_recovered               = res.recovered.at(age_segment_index);
                const vector<double>& res_incubation_rates  = incubation_rates.at(age_segment_index);
                const double curr_vac1_rates                = vac1_rates.at(age_segment_index);
                const vector<double>& curr_vac2_rates       = vac2_rates.at(age_segment_index);

                if (is_vaccination)
                {
                    // Calculate the number of new vaccinated dose 1
                    new_vac1 = precision_correction(new_vaccinated1(age_segment_index, curr_vac1_rates));

                    // Calculate the number of new vaccinated dose 2
                    new_vac2 = precision_correction(new_vaccinated2(age_segment_index, res));
                }

                // Calculate the total number of new exposed entering exposed(0)
                new_e = precision_correction(new_exposed(age_segment_index, res));

                // Calculate the total number new infected, exposed last day + exposed other days becoming infected
                new_i = precision_correction(new_infections(age_segment_index, res));

                // Calculate the vector of fatalities entered from each infection day
                fatalities = new_fatalities(res, age_segment_index);

                // Calculate the vector of new recoveries entering from each infection day 1:num_infection_phases,
                recovered = new_recoveries(res, age_segment_index, fatalities);

                res_fatalities += accumulate(fatalities.begin(), fatalities.end(), 0.0f);

                // The susceptible population is smaller due to previous deaths
                new_s = 1 - res_fatalities;

                // So far, it was assumed that on the last day of infection, all recovered. But this is not true- have to account
                //  for those who died on the last day of infection.
                recovered.back() -= fatalities.back();

                if (is_vaccination)
                {
                    // Advance all vaccinated dose 1 foward a day, with some moving to dose 2
                    for (unsigned int i = vac1_phases; i > 0; --i)
                    {
                        // Calculate new vaccinated dose 1 base on the previous day minus those who moved to dose 2
                        curr_vac1 = precision_correction(res_vaccinated1.at(i - 1) - vaccination2_rate(curr_vac2_rates, i - 1, res) * res_vaccinated1.at(i - 1));

                        // While those who are vaccinated are still susceptiple, the moment they are vaccinated
                        //  they are tracked a differently from those who are not so the variable curr_vac1 can be
                        //  viewed as 'suscetpible vaccinated with 1 dose'
                        //new_s -= curr_vac1;

                        res_vaccinated1.at(i) = curr_vac1;
                    }

                    res_vaccinated1.at(0) = new_vac1;
                    //new_s -= new_vac1;

                    res_vaccinated2.at(vac2_phases) += res_vaccinated2.at(vac2_phases - 1);

                    for (unsigned int i = vac2_phases - 1; i > 0; --i)
                    {
                        curr_vac2 = res_vaccinated2.at(i - 1);

                        //new_s -= curr_vac2;

                        res_vaccinated2.at(i) = curr_vac2;
                    }

                    res_vaccinated2.at(0) = new_vac2;
                    //new_s -= new_vac2;
                }

                // Advance all exposed forward a day, with some proportion leaving exposed(q-1) and entering infected(1)
                for (unsigned int i = exposed_phases - 1; i > 0; --i)
                {
                    // Calculate new exposed based on the incubation rate and the previous days exposed
                    curr_expos = precision_correction(res_exposed.at(i - 1) * (1 - res_incubation_rates.at(i - 1)));

                    // The susceptible population does not include the exposed population
                    new_s -= curr_expos;

                    res_exposed.at(i) = curr_expos;
                }

                res_exposed.at(0) = new_e;
                new_s -= new_e;

                // Equation 6d
                // Advance all infected q = 0 to q = Ti-1 one day forward
                for (unsigned int i = infected_phases - 1; i > 0; --i)
                {
                    // *** Calculate proportion of infected on a given day of the infection ***

                    // The previous day of infection
                    curr_inf = res_infected.at(i - 1);

                    // The number of people in a stage of infection moving to the new infection stage do not include those
                    // who have died or recovered. Note: A subtraction must be done here as the recovery and mortality rates
                    // are given for the total population of an infection stage. Multiplying by (1 - respective rate) here will
                    // NOT work as the second multiplication done will effectively be of the infection stage population after
                    // the first multiplication, rather than the entire infection state population.
                    curr_inf -= recovered.at(i - 1);
                    curr_inf -= fatalities.at(i - 1);

                    curr_inf = precision_correction(curr_inf);

                    // The amount of susceptible does not include the infected population
                    new_s -= curr_inf;

                    res_infected.at(i) = curr_inf;
                }

                // The people on the first day of infection
                res_infected.at(0) = new_i;

                // The susceptible population does not include those that just became exposed
                new_s -= new_i;

                recovered_index = recovered_phases - 1;

                if (!SIIRS_model)
                {
                    // Add the population on the second last day of recovery to the population on the last day of recovery.
                    // This entire population on the last day of recovery is then subtracted from the susceptible population
                    // to take into account that the population on the last day of recovery will not be subtracted from the susceptible
                    // population in the Equation 6a for loop.
                    res_recovered.back() += res_recovered.at(recovered_phases - 2);
                    new_s -= res_recovered.back();
                    // Avoid processing the population on the last day of recovery in the equation 6a for loop. This will
                    // update all stages of recovery population except the last one, which grows with every time step
                    // as it is only added to from the population on the second last day of recovery.
                    recovered_index -= 1;
                }

                // Equation 6a
                for (unsigned int i = recovered_index; i > 0; --i)
                {
                    // Each day of the recovered is the value of the previous day. The population on the last day is
                    // now susceptible (assuming a SIIRS model); this is implicitly done already as the susceptible value was set to 1.0 and the
                    // population on the last day of recovery is never subtracted from the susceptible value.
                    res_recovered.at(i) = res_recovered.at(i - 1);
                    new_s -= res_recovered.at(i);
                }

                // The people on the first day of recovery are those that were on the last stage of infection (minus those who died;
                // already accounted for) in the previous time step plus those that recovered early during an infection stage.
                res_recovered.at(0) = accumulate(recovered.begin(), recovered.end(), 0.0f);

                // The susceptible population does not include the recovered population
                new_s -= accumulate(recovered.begin(), recovered.end(), 0.0f);

                if (new_s > -0.001 && new_s < 0) new_s = 0; // double precision issues
                assert(new_s >= 0);

                res.susceptible.at(age_segment_index) = new_s;
            } //for(age_groups)

            return res;
        } //local_computation()

        // It returns the delay to communicate cell's new state.
        T output_delay(sevirds const &cell_state) const override { return 1; }

        double new_vaccinated1(unsigned int age_segment_index, double curr_vac1_rates) const
        {
            return curr_vac1_rates * state.current_state.susceptible.at(age_segment_index);
        }

        double new_vaccinated2(unsigned int age_segment_index, sevirds& current_sevirds) const
        {
            double vac2 = 0;

            // Get the list of vaccinated dose 1
            vector<double>& vac1 = current_sevirds.vaccinatedD1.at(age_segment_index);

            // The interval is the time between the first dose and when the second dose
            //  minus 1 because everybody on the last day moves to dose 2 regardless
            unsigned interval = vac1.size() - current_sevirds.min_interval_doses;

            // Everybody on the last day is moved to dose 2
            vac2 = vac1.back();

            // Some people are eligible to receive their second dose sooner
            for (unsigned int phase = 0; phase < interval; ++phase)
                vac2 += vac2_rates.at(age_segment_index).at(phase) * vac1.at(current_sevirds.min_interval_doses + phase);

            return vac2;
        }

        double new_exposed(unsigned int age_segment_index, sevirds& current_seird) const
        {
            double expos = 0;
            sevirds const cstate = state.current_state;

            // Calculate the correction factor of the current cell
            // The current cell must be part of its own neighborhood for this to work!
            vicinity self_vicinity = state.neighbors_vicinity.at(cell_id);
            double current_cell_correction_factor = cstate.disobedient
            + (1 - cstate.disobedient) * movement_correction_factor(self_vicinity.correction_factors, 
                                                        state.neighbors_state.at(cell_id).get_total_infections(),
                                                        current_seird.hysteresis_factors.at(cell_id));

            double neighbor_correction;
            unsigned int infected_phases;

            const vector<double>& res_mobility_rates  = mobility_rates.at(age_segment_index);
            const vector<double>& res_virulance_rates = virulence_rates.at(age_segment_index);
            double cstate_susceptible           = cstate.susceptible.at(age_segment_index);

            // External exposed
            for(auto neighbor: neighbors)
            {
                sevirds const nstate = state.neighbors_state.at(neighbor);
                vicinity v = state.neighbors_vicinity.at(neighbor);

                // disobedient people have a correction factor of 1. The rest of the population is affected by the movement_correction_factor
                neighbor_correction = nstate.disobedient + (1 - nstate.disobedient) * movement_correction_factor(v.correction_factors,
                                                                                                        nstate.get_total_infections(),
                                                                                                        current_seird.hysteresis_factors.at(neighbor));

                // Logically makes sense to require neighboring cells to follow the movement restriction that is currently
                // in place in the current cell if the current cell has a more restrictive movement.
                neighbor_correction = min(current_cell_correction_factor, neighbor_correction);

                infected_phases = nstate.get_num_infected_phases();
                double total_infections = nstate.get_total_infections();

                for (unsigned int i = 0; i < infected_phases; ++i)
                {
                    expos += v.correlation * res_mobility_rates.at(i) * // Variable Cij
                             res_virulance_rates.at(i) *                // Variable lambda
                             cstate_susceptible * total_infections *    // Variables Si and Ij, respectively
                             neighbor_correction;                       // New exposed may be slightly fewer if there are mobility restrictions
                }
            }

            return min(cstate.susceptible.at(age_segment_index), expos);
        } //new_exposed()

        double new_infections(unsigned int age_segment_index, sevirds &current_seird) const
        {
            double inf = 0;
            sevirds const cstate = state.current_state;
            const vector<double>& cstate_exposed = cstate.exposed.at(age_segment_index);
            inf = cstate_exposed.back();
            unsigned int exposed_size = cstate_exposed.size();
            const vector<double>& res_incubation_rates = incubation_rates.at(age_segment_index);

            // Scan through all exposed day except last and calculate exposed.at(asi).at(i)
            for (unsigned int i = 0; i < exposed_size - 1 ; i++)
                inf += cstate_exposed.at(i) * res_incubation_rates.at(i);

            inf = precision_correction((double)inf);
            return inf;
        }

        vector<double> new_recoveries(const sevirds &current_state, unsigned int age_segment_index, const vector<double> &fatalities) const
        {
            unsigned int infected_phases = current_state.get_num_infected_phases();
            vector<double> recovered(infected_phases, 0.0f);

            const vector<double>& cstate_infected = current_state.infected.at(age_segment_index);
            const vector<double>& res_recovery_rates = recovery_rates.at(age_segment_index);

            // Assume that any individuals that are not fatalities on the last stage of infection recover
            recovered.back() = cstate_infected.back() - fatalities.back();
            double new_recoveries, maximum_possible_recoveries;

            for (unsigned int i = 0; i < infected_phases - 1; ++i)
            {
                // Calculate all of the new recovered- for every day that a population is infected, some recover.
                new_recoveries = precision_correction(cstate_infected.at(i) * res_recovery_rates.at(i));

                // There can't be more recoveries than those who have died
                maximum_possible_recoveries = cstate_infected.at(i) - fatalities.at(i);

                recovered.at(i) = min(new_recoveries, maximum_possible_recoveries);
            }

            return recovered;
        }

        vector<double> new_fatalities(const sevirds &current_state, unsigned int age_segment_index) const
        {
            vector<double> fatalities(current_state.get_num_infected_phases(), 0.0f);

            const vector<double>& cstate_infected       = current_state.infected.at(age_segment_index);
            const vector<double>& res_fatality_rates    = fatality_rates.at(age_segment_index);

            unsigned int infected_phases = current_state.get_num_infected_phases();

            // Calculate all those who have died during an infection stage.
            for (unsigned int i = 0; i < infected_phases; ++i) {
                fatalities.at(i) += precision_correction(cstate_infected.at(i) * res_fatality_rates.at(i));

                if (current_state.get_total_infections() > current_state.hospital_capacity)
                    fatalities.at(i) *= current_state.fatality_modifier;

                // There can't be more fatalities than the number of people who are infected at a stage
                fatalities.at(i) = min(fatalities.at(i), cstate_infected.at(i));
            }

            return fatalities;
        }

        float movement_correction_factor(const map<infection_threshold, mobility_correction_factor> &mobility_correction_factors,
                                        float infectious_population, hysteresis_factor &hysteresisFactor) const
        {

            // For example, assume a correction factor of "0.4": [0.2, 0.1]. If the infection goes above 0.4, then the
            // correction factor of 0.2 will now be applied to total infection values above 0.3, no longer 0.4 as the
            // hysteresis is in effect.
            if (infectious_population > hysteresisFactor.infections_higher_bound)
                hysteresisFactor.in_effect = false;

            // This is uses the comparison '>', not '>=' ; otherwise if the lower bound is 0 there is no way for the hysteresis
            // to disappear as the infections can never go below 0
            if (hysteresisFactor.in_effect && infectious_population > hysteresisFactor.infections_lower_bound)
                return hysteresisFactor.mobility_correction_factor;

            hysteresisFactor.in_effect = false;

            float correction = 1.0f;
            for (auto const &pair: mobility_correction_factors)
            {
                if (infectious_population >= pair.first)
                {
                    correction = pair.second.front();

                    // A hysteresis factor will be in effect until the total infection goes below the hysteresis factor;
                    // until that happens the information required to return a movement factor must be kept in above variables.

                    // Get the threshold of the next correction factor; otherwise the current correction factor can remain in
                    // effect if the total infections never goes below the lower bound hysteresis factor, but also if it goes
                    // above the original total infection threshold!
                    auto next_pair_iterator = find(mobility_correction_factors.begin(), mobility_correction_factors.end(), pair);
                    assert(next_pair_iterator != mobility_correction_factors.end());

                    // If there is a next correction factor (for a higher total infection), then use it's total infection threshold
                    if ((long unsigned int) distance(mobility_correction_factors.begin(), next_pair_iterator) != mobility_correction_factors.size() - 1)
                        ++next_pair_iterator;

                    hysteresisFactor.in_effect                  = true;
                    hysteresisFactor.infections_higher_bound    = next_pair_iterator->first;
                    hysteresisFactor.infections_lower_bound     = pair.first - pair.second.back();
                    hysteresisFactor.mobility_correction_factor = pair.second.front();
                } else
                    break;
            }
            return correction;
        } //movement_correction_factor()

        double precision_correction(double proportion) const { return round(proportion * prec_divider) * one_over_prec_divider; }

        double vaccination2_rate(const vector<double>& curr_vac2_rates, int day, sevirds& current_sevirds) const
        {
            if (day < current_sevirds.min_interval_doses) return 0;
            return curr_vac2_rates.at(day - current_sevirds.min_interval_doses);
        }
}; //class geographical_cell{}

#endif //PANDEMIC_HOYA_2002_ZHONG_CELL_HPP