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
#include "AgeData.hpp"

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

        using phase_rates = vector<     // The age sub_division
                            vecDouble>; // The stage of infection

        phase_rates virulence_rates;
        phase_rates incubationD1_rates;
        phase_rates incubationD2_rates;
        phase_rates incubation_rates;
        phase_rates recovery_rates;
        phase_rates mobility_rates;
        phase_rates fatality_rates;
        phase_rates vac1_rates;
        phase_rates vac2_rates;

        // To make the parameters of the correction_factors variable more obvious
        using infection_threshold           = float;
        using mobility_correction_factor    = array<float, 2>;  // array<mobility correction factor, hysteresis factor>;

        int prec_divider;
        double one_over_prec_divider;
        bool SIIRS_model, is_vaccination;

        unsigned int age_segments;
        unsigned int vac1_phases;
        unsigned int vac2_phases;
        unsigned int exposed_phases;
        unsigned int infected_phases;
        unsigned int recovered_phases;

        geographical_cell() : cell<T, string, sevirds, vicinity>() {}

        geographical_cell(string const& cell_id, cell_unordered<vicinity> const& neighborhood,
                            sevirds const& initial_state, string const& delay_id, simulation_config config) :
            cell<T, string, sevirds, vicinity>(cell_id, neighborhood, initial_state, delay_id)
        {
            for (const auto& i : neighborhood)
                state.current_state.hysteresis_factors.insert({i.first, hysteresis_factor{}});

            is_vaccination                  = config.is_vaccination;
            state.current_state.vaccines    = is_vaccination;

            virulence_rates     = move(config.virulence_rates);
            incubation_rates    = move(config.incubation_rates);
            recovery_rates      = move(config.recovery_rates);
            mobility_rates      = move(config.mobility_rates);
            fatality_rates      = move(config.fatality_rates);

            // Multiplication is always faster then division so set this up to be 1/prec_divider to be multiplied later
            one_over_prec_divider   = 1.0 / (double)config.prec_divider;
            prec_divider            = config.prec_divider;
            SIIRS_model             = config.SIIRS_model;
            age_segments            = initial_state.get_num_age_segments();

            exposed_phases      = initial_state.get_num_exposed_phases();
            infected_phases     = initial_state.get_num_infected_phases();
            recovered_phases    = initial_state.get_num_recovered_phases();

            if (is_vaccination)
            {
                vac1_rates = move(config.vac1_rates);
                vac2_rates = move(config.vac2_rates);

                vac1_phases = initial_state.get_num_vaccinated1_phases();
                vac2_phases = initial_state.get_num_vaccinated2_phases();

                incubationD1_rates = move(config.incubationD1_rates);
                incubationD1_rates = move(config.incubationD2_rates);
            }

            assert(virulence_rates.size() == recovery_rates.size() && virulence_rates.size() == mobility_rates.size() &&
                virulence_rates.size() == incubation_rates.size() &&
                "\n\nThere must be an equal number of age segments between all configuration rates.\n\n");
        }

        // Whenever referring to a "population", it is meant the current age group's population.
        // The state of each age group's population is calculated individually.
        sevirds local_computation() const override
        {
            sevirds res = state.current_state;
            unique_ptr<AgeData> age_data_nvac = nullptr;
            unique_ptr<AgeData> age_data_vac1 = nullptr;
            unique_ptr<AgeData> age_data_vac2 = nullptr;
            double new_s;

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

                new_s = 1;

                age_data_nvac.reset(new AgeData(age_segment_index, res.susceptible, res.exposed, res.infected,
                                            res.recovered, res.fatalities, incubation_rates, recovery_rates,
                                            fatality_rates, mobility_rates, virulence_rates
                                            ));

                if (is_vaccination)
                {
                    age_data_vac1.reset(new AgeData(age_segment_index, res.vaccinatedD1, res.exposedD1, res.infectedD1,
                                                res.recoveredD1, res.fatalities, incubationD1_rates, recovery_rates,
                                                fatality_rates, vac1_rates.at(age_segment_index), mobility_rates,
                                                virulence_rates, res.immunityD1_rate.at(age_segment_index)
                                                ));
                    age_data_vac1.reset(new AgeData(age_segment_index, res.vaccinatedD2, res.exposedD2, res.infectedD2,
                                                res.recoveredD2, res.fatalities, incubationD2_rates, recovery_rates,
                                                fatality_rates, vac2_rates.at(age_segment_index), mobility_rates,
                                                virulence_rates, res.immunityD2_rate.at(age_segment_index)
                                                ));

                    compute_vaccinated(age_data_vac1, age_data_vac2, age_data_nvac, res, new_s);
                }

                compute_not_vaccinated(age_data_nvac, res, new_s);

                if (new_s > -0.001 && new_s < 0) new_s = 0; // double precision issues
                assert(new_s >= 0);

                age_data_nvac.get()->GetSusceptible().at(0) = new_s;
            } //for(age_groups)

            return res;
        } //local_computation()

        // It returns the delay to communicate cell's new state.
        T output_delay(sevirds const &cell_state) const override { return 1; }

        /**
         * Vaccine Dose 1: Equation 1
         * @param res_susceptible: Proportion of non-vaccinated susceptible for the current age group
         * @param curr_vac1_rates: Rate at which the current age group is getting their first dose
         * @param res_recovered: Proportion of recovered non-vaccinated for the current age group and at any phase.
         *                       Each index in the vector is a day in the recoverd phase
         * @return: New proportion with dose 1
        */
        double new_vaccinated1(unique_ptr<AgeData>& age_data_vac, unique_ptr<AgeData>& age_data_nvac) const
        {
            // Vaccination rate with those who are susceptible
            double new_vac1 = age_data_vac.get()->GetVaccinationRates().at(0) * age_data_nvac.get()->GetSusceptible().at(0);

            // And those who are in the recovery phase
            // TODO: Need to investigate when, after recovering, someone can get vaccinated
            for (unsigned int day = 0; day < recovered_phases; ++day)
                new_vac1 += age_data_vac.get()->GetVaccinationRates().at(0) * age_data_vac.get()->GetRecovered().at(day);

            return new_vac1;
        }

        /**
         * Vaccine Dose 2: Equation 1
         * @param res_vacciantedD1: Proportion of dose1 susceptible in the current age group
         * @param current_sevirds: Current state of the model (e.g., what phase each state is in)
         * @param curr_vac2_rates: Rate at which the current age group is getting their second dose.
         *                          Works in phases since some may be eligible for earlier vaccination (ex: nurses, doctors...)
         *
         * @return: New proportion with dose 2
        */
        double new_vaccinated2(unique_ptr<AgeData>& age_data_vac1, unique_ptr<AgeData>& age_data_vac2, sevirds const& res) const
        {
            double vac2 = 0;

            // The interval is the time between the first dose and when the second dose
            //  minus 1 because everybody on the last day moves to dose 2 regardless
            unsigned interval = age_data_vac1.get()->GetSusceptible().size() - 1 - res.min_interval_doses;

            // Everybody on the last day is moved to dose 2
            vac2 = age_data_vac1.get()->GetSusceptible().back(); // V1(td1)

            // Some people are eligible to receive their second dose sooner
            for (unsigned int q = 0; q < interval; ++q)
                vac2 += age_data_vac2.get()->GetVaccinationRates().at(q) // v
                        * age_data_vac1.get()->GetSusceptible().at(res.min_interval_doses + q); // V1(q)

            // TODO: v * RV1(q)

            return vac2;
        }

        /**
         * Exposed: E(1), EV1(1), or EV2(1)
         *  Calculates proportion of new exposures from either non-vac or vac (dose 1 or 2) population
         * @param current_sevirds: State machine object that holds simulation config data
         * @param age_data: Pointer to current simulation data
         * @param non_vac: Are we calculating with the non-vaccianted population?
        */
        double new_exposed(sevirds& current_sevirds, unique_ptr<AgeData>& age_data, bool non_vac=true) const
        {
            double expos = 0;

            // Calculate the correction factor of the current cell.
            // The current cell must be part of its own neighborhood for this to work!
            vicinity self_vicinity = state.neighbors_vicinity.at(cell_id);
            double current_cell_correction_factor = current_sevirds.disobedient
                                                    + (1 - current_sevirds.disobedient)
                                                    * movement_correction_factor(self_vicinity.correction_factors,
                                                                                state.neighbors_state.at(cell_id).get_total_infections(),
                                                                                current_sevirds.hysteresis_factors.at(cell_id));

            double neighbor_correction;

            // External exposed
            for(auto neighbor: neighbors)
            {
                sevirds const nstate = state.neighbors_state.at(neighbor);
                vicinity v = state.neighbors_vicinity.at(neighbor);

                // Disobedient people have a correction factor of 1. The rest of the population is affected by the movement_correction_factor
                neighbor_correction = nstate.disobedient
                                        + (1 - nstate.disobedient)
                                        * movement_correction_factor(v.correction_factors,
                                                                    nstate.get_total_infections(),
                                                                    current_sevirds.hysteresis_factors.at(neighbor));

                // Logically makes sense to require neighboring cells to follow the movement restriction that is currently
                // in place in the current cell if the current cell has a more restrictive movement.
                neighbor_correction = min(current_cell_correction_factor, neighbor_correction);

                double n_total_infections = nstate.get_total_infections();

                for (unsigned int q = 0; q < infected_phases; ++q)
                {
                    expos += v.correlation                                  // Variable Cij
                            * age_data.get()->GetMobilityRates().at(q)      // mu
                            * age_data.get()->GetVirulenceRates().at(q)     // lambda
                            * age_data.get()->GetSusceptible().at(0)        // V1i TODO: the .at() is incorrect for vaccinated susceptible
                            * n_total_infections                            // ITj
                            * neighbor_correction                           // New exposed may be slightly fewer if there are mobility restrictions
                            / (age_data.get()->GetImmunityRates().empty() ?  // Immunity Rate. Which changes week by week so multiply q by 1/7
                                1 :  age_data.get()->GetImmunityRates().at( (int)(q * 0.14f) )) // Non-vaccinated won'e have an immunity rate so set to 1
                    ;
                }
            }

            return non_vac ? min(age_data.get()->GetSusceptible().at(0), expos) : expos;
        } //new_exposed()

        /**
         * Infection: I(1), IV1(1), or IV2(1)
         *  Calculates proportion of new infections from either non-vac or vac (dose 1 or 2) population
         * @param age_data: Pointer to current simulation data
        */
        double new_infections(unique_ptr<AgeData>& age_data) const
        {
            double inf = age_data.get()->GetExposed().back(); // E(Te), EV1(Te), or EV2(Te)

            unsigned int exposed_size = age_data.get()->GetExposed().size() - 1;

            // Scan through all exposed day except last and calculate exposed.at(age).at(i)
            // Note: age_data.get()->GetExposed() == exposed.at(age)
            for (unsigned int i = 0; i < exposed_size; ++i)
                inf += age_data.get()->GetIncubationRates().at(i) // ε(q), εV1(q), or εV2(q)
                       * age_data.get()->GetExposed().at(i);      // E(q), EV1(q), or EV2(q)

            return precision_correction((double)inf);
        }

        /**
         * TODO: Comment
        */
        vecDouble new_recoveries(unique_ptr<AgeData>& age_data, vecDouble const& fatalities) const
        {
            vecDouble recovered(infected_phases, 0.0f);

            // Assume that any individuals that are not fatalities on the last stage of infection recover
            recovered.back() = age_data.get()->GetInfected().back() * (1 - fatalities.back());
            double new_recoveries, maximum_possible_recoveries;

            for (unsigned int q = 0; q < infected_phases - 2; ++q)
            {
                // Calculate all of the new recovered- for every day that a population is infected, some recover.
                new_recoveries = precision_correction(age_data.get()->GetInfected().at(q) * age_data.get()->GetRecoveryRates().at(q));

                // There can't be more recoveries than those who have died
                maximum_possible_recoveries = age_data.get()->GetInfected().at(q) - fatalities.at(q);

                recovered.at(q) = min(new_recoveries, maximum_possible_recoveries);
            }

            return recovered;
        }

        // TODO: Implement
        vecDouble new_recoveries_vac1()
        {
            vecDouble recovered;
            return recovered;
        }

        // TODO: Implement
        vecDouble new_recoveries_vac2()
        {
            vecDouble recovered;
            return recovered;
        }

        /**
         * TODO: Comment
        */
        vecDouble new_fatalities(sevirds const& current_state, unique_ptr<AgeData>& age_data) const
        {
            vecDouble fatalities(infected_phases, 0.0f);

            // Calculate all those who have died during an infection stage.
            for (unsigned int i = 0; i < infected_phases; ++i) {
                fatalities.at(i) += precision_correction(age_data.get()->GetInfected().at(i) * age_data.get()->GetFatalityRates().at(i));

                if (current_state.get_total_infections() > current_state.hospital_capacity)
                    fatalities.at(i) *= current_state.fatality_modifier;

                // There can't be more fatalities than the number of people who are infected at a stage
                fatalities.at(i) = min(fatalities.at(i), age_data.get()->GetInfected().at(i));
            }

            return fatalities;
        }

        /**
         * TODO: Comment
        */
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

        /**
         * Computes all the equations related to vaccines
         * @param age_data_vac1: Contains data of proportion with 1 dose
         * @param age_data_vac2: Data for those with 2 doses
         * @param res: The current state of the geographical cell
        */
        void compute_vaccinated(unique_ptr<AgeData>& age_data_vac1, unique_ptr<AgeData>& age_data_vac2, unique_ptr<AgeData>& age_data_nvac, 
                                sevirds& res, double& new_s) const
        {
            double curr_vac1, curr_vac2;

            // <VACCINATED DOSE 1>
                // Calculate the number of new vaccinated dose 1
                double new_vac1 = precision_correction(new_vaccinated1(age_data_vac1, age_data_nvac));

                // Advance all vaccinated dose 1 foward a day, with some moving to dose 2
                for (unsigned int q = vac1_phases - 1; q > 0; --q)
                {
                    // Calculate new vaccinated dose 1 base on the previous day minus those who moved to dose 2
                    // Vaccine Dose 1: Equation 2
                    if (q < vac1_phases - 2 && q != recovered_phases - 1)
                    {
                        curr_vac1 = age_data_vac1.get()->GetSusceptible().at(q - 1)
                                    - ( vaccination2_rate(age_data_vac2.get()->GetVaccinationRates(), q - 1, res)
                                        * age_data_vac1.get()->GetSusceptible().at(q - 1) )
                                    - age_data_vac1.get()->GetExposed().at(0);
                    }
                    // Vaccine Dose 1: Equation 3
                    else if (q == recovered_phases - 1 && recovered_phases < vac1_phases)
                    {
                        curr_vac1 = age_data_vac1.get()->GetSusceptible().at(q - 1)
                                    - ( vaccination2_rate(age_data_vac2.get()->GetVaccinationRates(), q - 1, res)
                                        * age_data_vac1.get()->GetSusceptible().at(q - 1) )
                                    + age_data_vac1.get()->GetRecovered().at(q)
                                    - age_data_vac1.get()->GetExposed().at(q);
                    }
                    else continue; // Should only be the first time through the loop but this is still sketchy

                    curr_vac1 = precision_correction(curr_vac1);

                    // While those who are vaccinated are still susceptiple, the moment they are vaccinated
                    // they are tracked a differently from those who are not so the variable curr_vac1 can be
                    // viewed as 'suscetpible vaccinated with 1 dose'
                    new_s -= curr_vac1;

                    age_data_vac1.get()->GetSusceptible().at(q) = curr_vac1;
                }

                // Vaccine Dose 1: Equation 4
                if (recovered_phases - 1 >= res.min_interval_doses)
                {
                    age_data_vac2.get()->GetSusceptible().at(vac1_phases - 2) = precision_correction( age_data_vac1.get()->GetSusceptible().at(vac1_phases - 2)
                                                                                - ( vaccination2_rate(age_data_vac2.get()->GetVaccinationRates(), vac1_phases - 2, res)
                                                                                    * age_data_vac1.get()->GetSusceptible().at(vac1_phases - 2) )
                                                                                + age_data_vac1->GetRecovered().at(recovered_phases - 1));
                }

                // Set the new dose1 proportion to the beginning of the phase
                age_data_vac1.get()->GetSusceptible().at(0) = min(new_vac1, age_data_nvac.get()->GetSusceptible().at(0));
                new_s -= new_vac1;
            // </VACCINATED DOSE 1>

            // <VACCINATED DOSE 2>
                // Calculate the number of new vaccinated dose 2
                double new_vac2 = precision_correction(new_vaccinated2(age_data_vac1, age_data_vac2, res));

                // Vaccine Dose 2: Equation 2
                for (unsigned int q = vac2_phases - 2; q > 0; --q)
                {
                    curr_vac2 = precision_correction(age_data_vac2.get()->GetSusceptible().at(q - 1)); // V2(q - 1)

                    new_s -= curr_vac2;

                    age_data_vac2.get()->GetSusceptible().at(q) = curr_vac2;
                }

                // Vaccine Dose 2: Equation 3
                age_data_vac2.get()->GetSusceptible().at(vac2_phases - 1) = precision_correction(age_data_vac2.get()->GetSusceptible().at(vac2_phases - 2)     // V2(td2 - 1)
                                                                                                + age_data_vac2.get()->GetSusceptible().at(vac2_phases - 1));  // V2(td2)

                age_data_vac2.get()->GetSusceptible().at(0) = min(new_vac2, age_data_nvac.get()->GetSusceptible().at(0));
                new_s -= new_vac2;
            // </VACCINATED DOSE 2>

            // <EXPOSED DOSE 1>
                double new_exposedV1 = precision_correction(new_exposed(res, age_data_vac1, false));
                new_s -= new_exposedV1;
            // </EXPOSED DOSE 1>

            // <EXPOSED DOSE 2>
                double new_exposedV2 = precision_correction(new_exposed(res, age_data_vac2, false));
                new_s -= new_exposedV2;
            // </EXPOSED DOSE 2>
        }

        /**
         * TODO: Comment
        */
        double compute_not_vaccinated(unique_ptr<AgeData>& age_data, sevirds& res, double& new_s) const
        {
            // Get these ahead of time for performance
            double curr_expos, curr_inf;

            // Calculate the vector of fatalities entered from each infection day
            vecDouble fatalities = new_fatalities(res, age_data);

            // Calculate the vector of new recoveries entering from each infection day 1:num_infection_phases,
            vecDouble recovered = new_recoveries(age_data, fatalities);

            // <EXPOSED>
                // Calculate the total number of new exposed entering exposed(0)
                double new_e = precision_correction(new_exposed(res, age_data));

                // Advance all exposed forward a day, with some proportion leaving exposed(q-1) and entering infected(1)
                for (unsigned int i = exposed_phases - 1; i > 0; --i)
                {
                    // Calculate new exposed based on the incubation rate and the previous days exposed
                    curr_expos = precision_correction(age_data.get()->GetExposed().at(i - 1) * (1 - age_data.get()->GetIncubationRates().at(i - 1)));

                    // The susceptible population does not include the exposed population
                    new_s -= curr_expos;

                    age_data.get()->GetExposed().at(i) = curr_expos;
                }

                age_data.get()->GetExposed().at(0) = new_e;
                new_s -= new_e;
            // </EXPOSED>

            // <INFECTED>
                // Calculate the total number new infected, exposed last day + exposed other days becoming infected
                double new_i = precision_correction(new_infections(age_data));

                // Advance all infected q = 0 to q = Ti-1 one day forward
                for (unsigned int i = infected_phases - 1; i > 0; --i)
                {
                    // *** Calculate proportion of infected on a given day of the infection ***

                    // The previous day of infection
                    curr_inf = age_data.get()->GetInfected().at(i - 1);

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

                    age_data.get()->GetInfected().at(i) = curr_inf;
                }

                // The people on the first day of infection
                age_data.get()->GetInfected().at(0) = new_i;

                // The susceptible population does not include those that just became exposed
                new_s -= new_i;
            // </INFECTED>

            // <FATALITIES>
                age_data.get()->GetFatalities() += accumulate(fatalities.begin(), fatalities.end(), 0.0f);

                // The susceptible population is smaller due to previous deaths
                new_s -= age_data.get()->GetFatalities();

                // So far, it was assumed that on the last day of infection, all recovered. But this is not true- have to account
                //  for those who died on the last day of infection.
                recovered.back() -= fatalities.back();
            // </FATALITIES>

            unsigned int recovered_index = recovered_phases - 1;

            // <RE-SUSCEPTIBLE>
                if (!SIIRS_model)
                {
                    // Add the population on the second last day of recovery to the population on the last day of recovery.
                    // This entire population on the last day of recovery is then subtracted from the susceptible population
                    // to take into account that the population on the last day of recovery will not be subtracted from the susceptible
                    // population in the Equation 6a for loop.
                    age_data.get()->GetRecovered().back() += age_data.get()->GetRecovered().at(recovered_phases - 2);
                    new_s -= age_data.get()->GetRecovered().back();

                    // Avoid processing the population on the last day of recovery in the equation 6a for loop. This will
                    // update all stages of recovery population except the last one, which grows with every time step
                    // as it is only added to from the population on the second last day of recovery.
                    recovered_index -= 1;
                }
            // </RE-SUSCEPTIBLE>

            // <RECOVERED>
                // Equation 6a
                for (unsigned int i = recovered_index; i > 0; --i)
                {
                    // Each day of the recovered is the value of the previous day. The population on the last day is
                    // now susceptible (assuming a SIIRS model); this is implicitly done already as the susceptible value was set to 1.0 and the
                    // population on the last day of recovery is never subtracted from the susceptible value.
                    age_data.get()->GetRecovered().at(i) = age_data.get()->GetRecovered().at(i - 1);
                    new_s -= age_data.get()->GetRecovered().at(i);
                }

                    // The people on the first day of recovery are those that were on the last stage of infection (minus those who died;
                    // already accounted for) in the previous time step plus those that recovered early during an infection stage.
                    age_data.get()->GetRecovered().at(0) = accumulate(recovered.begin(), recovered.end(), 0.0f);

                    // The susceptible population does not include the recovered population
                    new_s -= accumulate(recovered.begin(), recovered.end(), 0.0f);
                // </RECOVERIES>

                return 0;
        }

        double precision_correction(double proportion) const { return round(proportion * prec_divider) * one_over_prec_divider; }

        double vaccination2_rate(const vecDouble& curr_vac2_rates, unsigned int day, sevirds& current_sevirds) const
        {
            if (day < current_sevirds.min_interval_doses) return 0;
            return curr_vac2_rates.at(day - current_sevirds.min_interval_doses);
        }
}; //class geographical_cell{}

#endif //PANDEMIC_HOYA_2002_ZHONG_CELL_HPP