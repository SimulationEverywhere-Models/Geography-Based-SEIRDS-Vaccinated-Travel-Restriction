// Created by binybrion - 06/29/20
// Modified by Glenn    - 02/07/20
// And by Eric          - June-July/2021

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

// Indices for a vector used in local_compute(), compute_vaccinated(),
// and compute_EIRD()
unsigned int const NVAC = 0;
unsigned int const VAC1 = 1;
unsigned int const VAC2 = 2;

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
        using infection_threshold        = float;
        using mobility_correction_factor = array<float, 2>;  // array<mobility correction factor, hysteresis factor>;

        bool reSusceptibility, is_vaccination;

        unsigned int age_segments;

        geographical_cell() : cell<T, string, sevirds, vicinity>() {}

        geographical_cell(string const& cell_id, cell_unordered<vicinity> const& neighborhood,
                            sevirds const& initial_state, string const& delay_id, simulation_config config) :
            cell<T, string, sevirds, vicinity>(cell_id, neighborhood, initial_state, delay_id)
        {
            for (const auto& i : neighborhood)
                state.current_state.hysteresis_factors.insert({i.first, hysteresis_factor{}});

            is_vaccination               = config.is_vaccination;
            state.current_state.vaccines = is_vaccination;

            state.current_state.prec_divider          = (double)config.prec_divider;
            state.current_state.one_over_prec_divider = 1.0 / (double)config.prec_divider;

            virulence_rates  = move(config.virulence_rates);
            incubation_rates = move(config.incubation_rates);
            recovery_rates   = move(config.recovery_rates);
            mobility_rates   = move(config.mobility_rates);
            fatality_rates   = move(config.fatality_rates);

            // Multiplication is always faster then division so set this up to be 1/prec_divider to be multiplied later
            reSusceptibility  = config.reSusceptibility;
            age_segments = initial_state.get_num_age_segments();

            if (is_vaccination)
            {
                vac1_rates = move(config.vac1_rates);
                vac2_rates = move(config.vac2_rates);

                incubationD1_rates = move(config.incubationD1_rates);
                incubationD2_rates = move(config.incubationD2_rates);
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

            // Three pointers to hold the simulation data for each group
            // of population (non-vaccinated, vaccinated dose 1, vaccinated dose 2)
            unique_ptr<AgeData> age_data_nvac = nullptr;
            unique_ptr<AgeData> age_data_vac1 = nullptr;
            unique_ptr<AgeData> age_data_vac2 = nullptr;

            vector<unique_ptr<AgeData>*> datas{&age_data_nvac};

            // Caution when changing the order these are pushed in
            // A lot of the code depends on the order staying the same
            if (is_vaccination)
            {
                datas.push_back(&age_data_vac1);
                datas.push_back(&age_data_vac2);
            }

            // Global new susceptible variable as the other equations
            // remove their proportions from this one leaving it with
            // the remaning susceptible proportion
            double new_s;
            double old_s;

            // Calculate the next new sevirds variables for each age group
            for (unsigned int age_segment_index = 0; age_segment_index < age_segments; ++age_segment_index)
            {
                // Reset for new susceptible equation
                new_s = 1;

                age_data_nvac.reset(new AgeData(age_segment_index, res.susceptible, res.exposed, res.infected,
                                                res.recovered, incubation_rates, recovery_rates, fatality_rates));

                if (is_vaccination)
                {
                    age_data_vac1.reset(new AgeData(age_segment_index, res.vaccinatedD1, res.exposedD1, res.infectedD1,
                                                    res.recoveredD1, incubationD1_rates, recovery_rates,
                                                    fatality_rates, vac1_rates.at(age_segment_index),
                                                    res.immunityD1_rate.at(age_segment_index), AgeData::PopType::DOSE1));
                    age_data_vac2.reset(new AgeData(age_segment_index, res.vaccinatedD2, res.exposedD2, res.infectedD2,
                                                    res.recoveredD2, incubationD2_rates, recovery_rates,
                                                    fatality_rates, vac2_rates.at(age_segment_index),
                                                    res.immunityD2_rate.at(age_segment_index), AgeData::PopType::DOSE2));

                    // Equations for Vaccinated population (eg. EV1, RV2...)
                    compute_vaccinated(datas, res);

                    new_s -= age_data_vac1.get()->GetTotalSusceptible(); // 1e
                    sanity_check(new_s);
                    new_s -= age_data_vac2.get()->GetTotalSusceptible(); // 2d
                    sanity_check(new_s);
                }

                // Computer the Exposed, Infected, Recovered, and Fatalities equations
                // for all population types
                compute_EIRD(datas, res);

                // Compute Susceptible
                for (unique_ptr<AgeData> *data : datas)
                {
                    new_s -= data->get()->GetTotalExposed();
                    sanity_check(new_s);
                    new_s -= data->get()->GetTotalInfected();
                    sanity_check(new_s);
                    new_s -= data->get()->GetTotalRecovered();
                    sanity_check(new_s);

                    res.fatalities.at(age_segment_index) += data->get()->GetTotalFatalities();
                    sanity_check(res.fatalities.at(age_segment_index));
                }

                sanity_check(new_s);
                old_s = new_s;
                new_s -= res.fatalities.at(age_segment_index);
                sanity_check(new_s);

                res.susceptible.at(age_segment_index).front() = new_s;
            } //for(age_groups)

            return res;
        } //local_computation()

        // It returns the delay to communicate cell's new state.
        T output_delay(sevirds const &cell_state) const override { return 1; }

        /**
         * @brief Vaccinated Dose 1 - Equation 1a
         * 
         * @param datas: Vector containing the three population types and their data
         * @return double: New proportion of vaccinated dose 1
         */
        double new_vaccinated1(vector<unique_ptr<AgeData> *> datas, sevirds const& res) const
        {
            // Vaccination rate with those who are susceptible
            // vd1 * S
            double new_vac1 = datas.at(VAC1)->get()->GetVaccinationRate(0) * datas.at(NVAC)->get()->GetOrigSusceptible(0);

            // And those who are in the recovery phase
            double sum = 0;
            // for (unsigned int q = res.min_interval_recovery_to_vaccine; q <= datas.at(NVAC)->get()->GetRecoveredPhase(); ++q)
            //     sum += datas.at(NVAC)->get()->GetOrigRecovered(q);

            return new_vac1 + sum;
        }

        /**
         * @brief Vaccinated Dose 2 - Equation 2a
         * 
         * @param datas: Vector containing the three population types with their respective data
         * @param res: Current state of the cell
         * @return double: New proportion of vaccinated dose 2
         */
        double new_vaccinated2(vector<unique_ptr<AgeData> *> datas, sevirds& res, vecDouble const& earlyVac2) const
        {
            unique_ptr<AgeData>& age_data_vac1 = *(datas.at(VAC1));
            unique_ptr<AgeData>& age_data_vac2 = *(datas.at(VAC2));

            // Everybody on the last day of dose 1 is moved to dose 2
            double vac2 = age_data_vac1.get()->GetOrigSusceptibleBack(); // V1(td1)

            // Some people are eligible to receive their second dose sooner
            // qϵ{mtd1...td1 - 1}
            vac2 += accumulate(earlyVac2.begin(), earlyVac2.end(), 0.0);

            // Some people are eligible to receive their second dose sooner from the dose 1 recovery pop
            // qϵ{mtd1...Tr}
            // for (unsigned int q = res.min_interval_recovery_to_vaccine; q <= age_data_vac1.get()->GetRecoveredPhase(); ++q)
            // {
            //     vac2 += age_data_vac2.get()->GetVaccinationRate(q - res.min_interval_recovery_to_vaccine) // v(q)
            //             * age_data_vac1.get()->GetOrigRecovered(q)                                        // RV1(q)
            //         ;
            // }

            // - V1(td1) * sum(1...k and 1...Ti))
            vac2 -= new_exposed(res, age_data_vac1, age_data_vac1.get()->GetSusceptiblePhase(), false);
            sanity_check(vac2);

            return vac2;
        }

        /**
         * @brief Calculates proportion of new exposures from either non-vac or vac (dose 1 or 2) population.
         * 1b, 1c, 1d, 1e, 1f, 2b, 2c, 2d, 2e, 3a, 3b and 3c use this
         * 
         * @param res: State machine object that holds simulation config data
         * @param age_data: Pointer to current simulation data
         * @param q: Index to compute equation
         * @param immunity: Whether immunity should be factored in (only works on vaccinated age_data types)
         * @return double: Number of new exposures
        */
        double new_exposed(sevirds &res, unique_ptr<AgeData> &age_data, int q = 0, bool immunity = true) const
        {
            double expos = 0, sum = 0, inner_sum, inner_sumV1, inner_sumV2;

            // Calculate the correction factor of the current cell.
            // The current cell must be part of its own neighborhood for this to work!
            vicinity self_vicinity = state.neighbors_vicinity.at(cell_id);
            double current_cell_correction_factor = res.disobedient
                                                    + (1 - res.disobedient)
                                                    * movement_correction_factor(self_vicinity.correction_factors,
                                                                                state.neighbors_state.at(cell_id).get_total_infections(),
                                                                                res.hysteresis_factors.at(cell_id));

            double neighbor_correction;

            // jϵ{1...k}
            for (string neighbor : neighbors)
            {
                sevirds const& nstate   = state.neighbors_state.at(neighbor);       // Cell j's state
                vicinity const& v       = state.neighbors_vicinity.at(neighbor);    // Holds cij and a correction factor used in kij

                // Disobedient people have a correction factor of 1. The rest of the population is affected by the movement_correction_factor
                neighbor_correction = nstate.disobedient
                                        + (1 - nstate.disobedient)
                                        * movement_correction_factor(v.correction_factors,
                                                                    nstate.get_total_infections(),
                                                                    res.hysteresis_factors.at(neighbor));

                // Logically makes sense to require neighboring cells to follow the movement restriction that is currently
                // in place in the current cell if the current cell has a more restrictive movement.
                neighbor_correction = min(current_cell_correction_factor, neighbor_correction);

                // Reset the inner sum for the next neighbor
                inner_sum = 0; inner_sumV1 = 0; inner_sumV2 = 0;

                // bϵ{1...A}
                for (unsigned int age_group = 0; age_group < nstate.num_age_groups; ++age_group)
                {

                    // nϵ{1...Ti}
                    for (unsigned int n = 0; n < nstate.infected.at(age_group).size(); ++n)
                    {
                        inner_sum +=
                                mobility_rates.at(age_group).at(n)    // μ(n)
                                * virulence_rates.at(age_group).at(n) // λ(n)
                                * nstate.infected.at(age_group).at(n) // I(n)
                            ;
                    }

                    if (is_vaccination)
                    {
                        // nϵ{1...Ti,V1}
                        for (unsigned int n = 0; n < nstate.infectedD1.at(age_group).size(); ++n)
                        {
                            inner_sumV1 +=
                                    mobility_rates.at(age_group).at(n)      // μ(n)
                                    * virulence_rates.at(age_group).at(n)   // λ(n)
                                    * nstate.infectedD1.at(age_group).at(n) // IV1(n)
                                ;
                        }

                        // nϵ{1...Ti,V2}
                        for (unsigned int n = 0; n < nstate.infectedD2.at(age_group).size(); ++n)
                        {
                            inner_sumV2 +=
                                mobility_rates.at(age_group).at(n)      // μ(n)
                                * virulence_rates.at(age_group).at(n)   // λ(n)
                                * nstate.infectedD2.at(age_group).at(n) // IV2(n)
                                ;
                        }
                    }

                    sum += v.correlation                                // cij
                           * neighbor_correction                        // kij
                           * (inner_sum + inner_sumV1 + inner_sumV2)    // sum(1...Ti)
                           * nstate.age_group_proportions.at(age_group) // Njb / Nj
                        ;
                }
            }

            expos = age_data.get()->GetOrigSusceptible(q) * sum; // S * sum(1...k)

            // if (immunity && age_data.get()->GetType() != AgeData::PopType::NVAC)
            //     expos *= 1.0 - age_data.get()->GetImmunityRate( (int)(q * 0.14f) ); // 1 - i(1)

            sanity_check(expos);
            return expos;
        } //new_exposed()

        /**
         * @brief Exposed: E(q), EV1(q), EV2(q)
         *  Advance all exposed forward a day, with some proportion leaving exposed(q-1) and entering infected(1)
         * 
         * @param age_data: Pointer to current simulation data for current age group (nvac, dose1, dose2) and age group
        */
        void increment_exposed(unique_ptr<AgeData>& age_data) const
        {
            double curr_expos;

            // qϵ{2...Te}
            for (unsigned int q = age_data.get()->GetExposedPhase(); q > 0; --q)
            {
                // Calculate new exposed based on the incubation rate and the previous days exposed
                curr_expos = (1 - age_data.get()->GetIncubationRate(q - 1)) // 1 - ε(q - 1)
                             * age_data.get()->GetOrigExposed(q - 1)        // * E(q - 1)
                    ;

                sanity_check(curr_expos);

                age_data.get()->SetExposed(q, curr_expos);
            }
        }

        /**
         * @brief Infection: I(1), IV1(1), or IV2(1)
         *  Calculates proportion of new infections from either non-vac or vac (dose 1 or 2) population
         * 
         * @param age_data: Reference to current simulation data
         * @return Number of new infections
        */
        double new_infections(unique_ptr<AgeData>& age_data) const
        {
            double inf = 0;

            /* Scan through all exposed days and calculate exposed.at(age).at(q)
            *   Incubation Rate on Te must be 1.0
            *   Note: age_data.get()->GetOrigExposed(i) == exposed.at(age).at(q) 
            *   and at timestep t not t+1
            *   qϵ{1...Te-1}
            */
            for (unsigned int q = 1; q <= age_data.get()->GetExposedPhase(); ++q)
            {
                inf += age_data.get()->GetIncubationRate(q) // ε(q), εV1(q), or εV2(q)
                       * age_data.get()->GetOrigExposed(q)  // E(q), EV1(q), or EV2(q)
                    ;
            }

            sanity_check(inf);
            return inf;
        }

        /**
         * @brief Infectd: I(q), IV1(q), IV2(q)
         *  Advances all infected forward a day, with some already moved to fatalities or recovered prior
         * 
         * @param age_data: Pointer to current simulation data for current age group (nvac, dose1, dose2) and age group
         * @param recovered: Vector of new recoveries from each day
        */
        void increment_infections(unique_ptr<AgeData>& age_data) const
        {
            double curr_inf;

            // qϵ{2...Ti}
            for (unsigned int q = age_data.get()->GetInfectedPhase(); q > 0; --q)
            {
                // The previous day of infection
                curr_inf = age_data.get()->GetOrigInfected(q - 1)    // I(q - 1)
                           - age_data.get()->GetNewFatalities(q - 1) // - D(q - 1)
                           - age_data.get()->GetNewRecovered(q - 1) // - R(q - 1)
                    ;

                sanity_check(curr_inf);

                age_data.get()->SetInfected(q, curr_inf);
            }
        }

        /**
         * @brief Recovered: R(1), RV1(1), or RV2(1)
         *  Calculates proportion of new new recoveries from either non-vac or vac (dose 1 or 2) population
         * 
         * @param age_data: Reference to simulation data
         * @return: The new proportion of R(1)
        */
        double new_recoveries(unique_ptr<AgeData>& age_data) const
        {
            // Assume that any individuals that are not fatalities on the last stage of infection recover
            double recoveries = age_data.get()->GetOrigInfectedBack()    // I(q)
                                - age_data.get()->GetNewFatalitiesBack() // - D(q)
                ;
            sanity_check(recoveries);
            age_data.get()->SetNewRecovered(age_data.get()->GetInfectedPhase(), recoveries);

            double sum;

            // qϵ{1...Ti - 1}
            for (unsigned int q = 0; q <= age_data.get()->GetInfectedPhase() - 1; ++q)
            {
                // Calculate all of the new recovered for every day that a population is infected, some recover.
                sum = age_data.get()->GetRecoveryRate(q)   // γ(q)
                      * age_data.get()->GetOrigInfected(q) // I(q)
                    ;

                recoveries += sum;
                age_data.get()->SetNewRecovered(q, sum);
            }

            sanity_check(recoveries);
            return recoveries;
        }

        /**
         * @brief Infectd: R(q), RV1(q), RV2(q)
         *  Advances all infected forward a day, with some already moved to fatalities or recovered prior
         * 
         * @param age_data: Pointer to current simulation data for current age group (nvac, dose1, dose2) and age group
         * @param recovered_index: If res-susc is turned on this will avoid processing the population on the last day
         * @param age_data_vac: Pointer to a vaccinated age_data object that is used for R(q) and RV1(q)
         * @param res: Used to get the minimum interval between doses needed in RV1(q)
        */
        void increment_recoveries(unique_ptr<AgeData>& age_data, unsigned int recovered_index,
                                unique_ptr<AgeData>* age_data_vac=nullptr, sevirds* res=nullptr) const
        {
            double vaccinated, curr_rec;

            // qϵ{2...Tr}
            for (unsigned int q = recovered_index; q > 0; --q)
            {
                vaccinated = 1;

                if (is_vaccination)
                {
                    // if (age_data.get()->GetType() == AgeData::PopType::NVAC && q > res->min_interval_recovery_to_vaccine)
                    // {
                    //     // Used in 5d
                    //     vaccinated -= age_data_vac->get()->GetVaccinationRate(0); // 1 - vd1
                    // }
                    // else if (age_data.get()->GetType() == AgeData::PopType::DOSE1 && q > res->min_interval_recovery_to_vaccine)
                    // {
                    //     // Used in 5e
                    //     vaccinated -= age_data_vac->get()->GetVaccinationRate(q - 1 - res->min_interval_recovery_to_vaccine); // 1 - vd2(q - 1)
                    // }
                }

                // Each day of the recovered phase is the value of the previous day. The population on the last day is
                // now susceptible (assuming a re-susceptible model); this is implicitly done already as the susceptible value was set to 1.0 and the
                // population on the last day of recovery is never subtracted from the susceptible value.
                // 5d, 5e, 5f
                curr_rec = age_data.get()->GetOrigRecovered(q - 1) * vaccinated; // R(q - 1) * (1 - vd(q - 1))
                sanity_check(curr_rec);

                age_data.get()->SetRecovered(q, curr_rec);
            }
        }

        /**
         * @brief New fatalities for one population group (Non-Vaccinated/Vaccinated Dose 1/Vaccinvated Dose 2)
         *  These are calculated individually as the vector of new fatalities is used by the functions
         *  that follow this one (see compute_vaccinated() and compute_not_vaccinated()). If we calculated
         *  the global number of fatalities later when we do something like dose1.infected().at(q) - fatalities.at(q)
         *  to get the maximum number of possible recoveries the fatality number will be including those from dose 2 and
         *  not vaccinated when in reality the maximum number of recoveries for dose1 is limited to those who are infected
         *  with dose 1 and still alive so we only want to remove those who are dose 1 fatality.
         *
         * @param current_state: State of the geographical cell (holds some global data)
         * @param age_data: Contains the data of the proportion. In this function the infections proportion as well as
         *                  the fatality rates are used from here
         * @return vector of doubles pertaining to the number of deaths per day in the infected phase
        */
        double new_fatalities(sevirds const& res, unique_ptr<AgeData>& age_data) const
        {
            double new_f = 0.0, sum;

            // Calculate all those who have died during an infection stage.
            for (unsigned int q = 0; q <= age_data.get()->GetInfectedPhase(); ++q)
            {
                sum = age_data.get()->GetFatalityRate(q) * age_data.get()->GetOrigInfected(q);

                // Amplify fatality rate if the hospitals are full
                if (res.get_total_infections() > res.hospital_capacity)
                    sum *= res.fatality_modifier;

                new_f += sum;
                age_data.get()->SetNewFatalities(q, sum);
            }

            sanity_check(new_f);
            return new_f;
        }

        /**
         * @brief TODO: Needs Comments
         * 
         * @param mobility_correction_factors:
         * @param infectious_population:
         * @param hysteresisFactor:
         * @return double:
         */
        double movement_correction_factor(const map<infection_threshold, mobility_correction_factor> &mobility_correction_factors,
                                        double infectious_population, hysteresis_factor &hysteresisFactor) const
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

            double correction = 1.0;
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
         * @brief Computes all the equations specific to the vaccinated population
         * 
         * @param datas: List of AgeData objects containing current age group data
         * @param res: The current state of the geographical cell
        */
        void compute_vaccinated(vector<unique_ptr<AgeData> *> datas, sevirds& res) const
        {
            double curr_vac1 = 0.0, curr_vac2 = 0.0, end, new_exp;

            unique_ptr<AgeData>& age_data_vac1 = *(datas.at(VAC1));
            unique_ptr<AgeData>& age_data_vac2 = *(datas.at(VAC2));

            vecDouble earlyVac2(age_data_vac1.get()->GetSusceptiblePhase(), 0.0);

            // <VACCINATED DOSE 1>
            // Calculate the number of new vaccinated dose 1
            double new_vac1 = new_vaccinated1(datas, res); // 1a

            // qϵ{2...td1}
            for (unsigned int q = age_data_vac1.get()->GetSusceptiblePhase() - 1; q > 0; --q)
            {
                // 1b
                curr_vac1 = age_data_vac1.get()->GetOrigSusceptible(q - 1) // V1(q - 1)
                            - new_exposed(res, age_data_vac1, q - 1)       // - ( V1(q - 1) * (1 - iv1(q - 1)) * sum(1..k and 1...Ti) )
                    ;

                if (q > res.min_interval_doses)
                {
                    // 1c substracts early dose2 vaccinations from 1b
                    earlyVac2.at(q - 1) = age_data_vac2.get()->GetVaccinationRate(q - 1 - res.min_interval_doses) // vd2(q - 1)
                                      * age_data_vac1.get()->GetOrigSusceptible(q - 1)                        // * V1(q - 1)
                        ;

                    curr_vac1 -= earlyVac2.at(q - 1);
                }

                sanity_check(curr_vac1);

                // Update the current day with the modified exposed from yesterday
                age_data_vac1.get()->SetSusceptible(q, curr_vac1);
            }

                end = (age_data_vac1.get()->GetOrigSusceptible(age_data_vac1.get()->GetSusceptiblePhase() - 1) // V1(q - 1)
                       * (1 - age_data_vac2.get()->GetVaccinationRate(age_data_vac1.get()->GetRecoveredPhase() - res.min_interval_recovery_to_vaccine))) // * (1 - vd2)
                    ;
                earlyVac2.back() = end;
                end -= new_exposed(res, age_data_vac1, age_data_vac1.get()->GetSusceptiblePhase() - 1); // - ( V1(q - 1) * (1 - iv1(q - 1)) * sum(1..k and 1...Ti) )

                // 1d
                if (reSusceptibility && age_data_vac1.get()->GetRecoveredPhase() >= age_data_vac1.get()->GetSusceptiblePhase())
                {
                    end = age_data_vac1.get()->GetOrigRecoveredBack()                                                                                      // RV1(Tr)
                          * (1 - age_data_vac2.get()->GetVaccinationRate(age_data_vac1.get()->GetRecoveredPhase() - res.min_interval_recovery_to_vaccine)) // * (1 - vd2(Tr))
                        ;

                    sanity_check(end);
                    age_data_vac1.get()->SetSusceptible(age_data_vac1.get()->GetSusceptiblePhase(), end);
                }

                // Set the new dose1 proportion to the beginning of the phase
                age_data_vac1.get()->SetSusceptible(0, new_vac1);
                sanity_check(age_data_vac1.get()->GetTotalSusceptible());
            // </VACCINATED DOSE 1>

            // <VACCINATED DOSE 2>
                // Calculate the number of new vaccinated dose 2
                double new_vac2 = new_vaccinated2(datas, res, earlyVac2);
                sanity_check(new_vac2);

                // qϵ{2...td2 - 1}
                for (unsigned int q = age_data_vac2.get()->GetSusceptiblePhase() - 1; q > 0; --q)
                {
                    // 2b
                    curr_vac2 = age_data_vac2.get()->GetOrigSusceptible(q - 1) // V2(q - 1)
                                - new_exposed(res, age_data_vac2, q - 1)       // - V2(q - 1) * (1 - iv2(q - 1)) * sum( jϵ{1…k}(cij * kij * sum(bϵ{1...A} and nϵ{1...Ti}[...])) )
                        ;

                    sanity_check(curr_vac2);
                    age_data_vac2.get()->SetSusceptible(q, curr_vac2);
                }

                // 2c
                end = age_data_vac2.get()->GetOrigSusceptible(age_data_vac2.get()->GetSusceptiblePhase() - 1) // V2(td2 - 1)
                      + age_data_vac2.get()->GetOrigSusceptibleBack()                                         // V2(td2)
                      - new_exposed(res, age_data_vac2, age_data_vac2.get()->GetSusceptiblePhase() - 1)       // - V2(td2 - 1) * (1 - iV2(td2 - 1)) * sum( jϵ{1...k}(cij * kij * sum(bϵ{1...A} and nϵ{1...Ti}[...]) )
                      - new_exposed(res, age_data_vac2, age_data_vac2.get()->GetSusceptiblePhase())           // - V2(td2) * (1 - iV2(td2)) * sum( jϵ{1...k}(cij * kij * sum(bϵ{1...A} and nϵ{1...Ti}[...]) )
                    ;

                if (reSusceptibility)
                    end += age_data_vac2.get()->GetOrigRecoveredBack(); // + RV2(Tr)

                sanity_check(end);
                age_data_vac2.get()->SetSusceptible(age_data_vac2.get()->GetSusceptiblePhase(), end);
                age_data_vac2.get()->SetSusceptible(0, new_vac2); // Set the first day of the phase
                sanity_check(age_data_vac2.get()->GetTotalSusceptible());
            // </VACCINATED DOSE 2>
        }

        /**
         * @brief Computes the exposed, infected, recovered, and dead equations for all population types
         * Setup the the datas vector to hold all the population types and they'll be looped through
         * 
         * @param datas: Vector of pointers holding the population states (i.e., NVac, Dose1, Dose2)
         * @param res: Current cell data
         */
        void compute_EIRD(vector<unique_ptr<AgeData> *> datas, sevirds& res) const
        {
            double new_expos, new_inf, new_rec;
            unsigned int recovered_index, phase;

            for (unique_ptr<AgeData>* age_data : datas)
            {
                // <FATALITIES>
                    age_data->get()->SetTotalFatalities(new_fatalities(res, *age_data));
                    sanity_check(age_data->get()->GetTotalFatalities());
                // </FATALITIES>

                // <RECOVERIES>
                    new_rec = new_recoveries(*age_data);
                // </RECOVERIES>

                // <EXPOSED>
                    new_expos = 0.0;

                    // qϵ{1...Td2}
                    for (unsigned int q = 0; q <= age_data->get()->GetSusceptiblePhase(); ++q)
                        new_expos += new_exposed(res, *age_data, q);

                    increment_exposed(*age_data);

                    age_data->get()->SetExposed(0, new_expos);
                // </EXPOSED>

                // <INFECTED>
                    new_inf = new_infections(*age_data);

                    increment_infections(*age_data);

                    age_data->get()->SetInfected(0, new_inf);
                // </INFECTED>

                // <RECOVERED>
                    recovered_index = age_data->get()->GetRecoveredPhase();

                    // If re-susceptibility is off
                    if (!reSusceptibility)
                    {
                        // Add the population on the second last day of recovery to the population on the last day of recovery.
                        // This entire population on the last day of recovery is then subtracted from the susceptible population
                        // to take into account that the population on the last day of recovery will not be subtracted from the susceptible
                        // population in the Equation 6a for loop.
                        age_data->get()->SetRecovered(
                            recovered_index,                                                                             // Tr
                            age_data->get()->GetRecoveredBack() + age_data->get()->GetOrigRecovered(recovered_index - 1) // R(Tr) + R(Tr - 1)
                        );

                        // Avoid processing the population on the last day of recovery in the equation 6a for loop. This will
                        // update all stages of recovery population except the last one, which grows with every time step
                        // as it is only added to from the population on the second last day of recovery.
                        recovered_index -= 1;
                    }

                    if (age_data->get()->GetType() == AgeData::PopType::DOSE1)
                        increment_recoveries(*age_data, recovered_index, datas.at(VAC2), &res);
                    else if (age_data->get()->GetType() == AgeData::PopType::DOSE2)
                        increment_recoveries(*age_data, recovered_index);
                    else
                        increment_recoveries(*age_data, recovered_index, (is_vaccination ? datas.at(VAC1) : nullptr), (is_vaccination ? &res : nullptr));

                    // The people on the first day of recovery are those that were on the last stage of infection (minus those who died;
                    // already accounted for) in the previous time step plus those that recovered early during an infection stage.
                    age_data->get()->SetRecovered(0, new_rec);
                // </RECOVERED>
            }
        }

        /**
         * @brief Basic check that the proportion is not
         * less then 0 or bigger then 1
         * 
         * @param value:   Proportion to check
         * @param message: Custom message to print when proportion fails checks
         */
        void sanity_check(double value, string message="") const
        {
            if (value < 0 || value > 1)
            {
                cout << "\n\033[31m" << value << "\033[0m is \033[33m"
                    << (value < 0 ? "less then zero" : "bigger then one") << "\033[0m on day "
                    << simulation_clock << endl;

                if (!message.empty())
                    cout << message;

                assert(false);
            }
        }
}; //class geographical_cell{}

#endif //PANDEMIC_HOYA_2002_ZHONG_CELL_HPP