// Created by binybrion - 06/30/20
// Modified by Glenn    - 02/07/20

#ifndef PANDEMIC_HOYA_2002_SEIRD_HPP
#define PANDEMIC_HOYA_2002_SEIRD_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include "hysteresis_factor.hpp"
#include "../Helpers/Assert.hpp"

using namespace std;
using namespace Assert;

/**
 * Keeps track of the model data and is initially
 * populated by what is store under the "state"
 * param found in default.json.
*/
struct sevirds
{
    using proportionVector = vector<vector<double>>;    // { {doubles}, {doubles},   ......... }
                                                        //   ageGroup1  ageGroup2    ageGroup#

    double population;
    vector<double> age_group_proportions;

    // Susceptible
    proportionVector susceptible;
    proportionVector vaccinatedD1;
    proportionVector vaccinatedD2;

    // Exposed
    proportionVector exposed;
    proportionVector exposedD1;
    proportionVector exposedD2;

    // Infected
    proportionVector infected;
    proportionVector infectedD1;
    proportionVector infectedD2;

    // Recovered
    proportionVector recovered;
    proportionVector recoveredD1;
    proportionVector recoveredD2;

    // Fatalities
    vector<double> fatalities;

    // Modifiers
    double disobedient;
    double hospital_capacity;
    double fatality_modifier;

    // Vaccines
    proportionVector immunityD1_rate;
    proportionVector immunityD2_rate;
    unsigned int min_interval_doses;
    unsigned int min_interval_recovery_to_vaccine;

    unordered_map<string, hysteresis_factor> hysteresis_factors;
    unsigned int num_age_groups;

    bool vaccines;       // Are vaccines being modelled?
    double prec_divider; // Precision divider

    // 1 divided by precision divider
    // Divisions cost more then multiplication
    // so do it once at the start then multiply by the decimal value
    double one_over_prec_divider;

    // Required for the JSON library, as types used with it must be default-constructable.
    // The overloaded constructor results in a default constructor having to be manually written.
    sevirds()
    {
        vaccines              = false;
        prec_divider          = 0;
        one_over_prec_divider = 0;
    };

    sevirds(proportionVector sus, proportionVector vac1, proportionVector vac2,
            proportionVector exp, proportionVector exp1, proportionVector exp2,
            proportionVector inf, proportionVector inf1, proportionVector inf2,
            proportionVector rec, proportionVector rec1, proportionVector rec2,
            vector<double> fat, double dis, double hcap, double fatm, proportionVector immuD1, unsigned int min_interval,
            proportionVector immuD2, double divider, bool vac=false) :
                susceptible{move(sus)},
                vaccinatedD1{move(vac1)},
                vaccinatedD2{move(vac2)},
                exposed{move(exp)},
                exposedD1{move(exp1)},
                exposedD2(move(exp2)),
                infected{move(inf)},
                infectedD1{move(inf1)},
                infectedD2{move(inf2)},
                recovered{move(rec)},
                recoveredD1{move(rec1)},
                recoveredD2{move(rec2)},
                fatalities{move(fat)},
                disobedient{dis},
                hospital_capacity{hcap},
                fatality_modifier{fatm},
                immunityD1_rate{move(immuD1)},
                immunityD2_rate{move(immuD2)},
                min_interval_doses{min_interval},
                vaccines(vac),
                prec_divider(divider),
                one_over_prec_divider(1.0 / divider)
    { num_age_groups = age_group_proportions.size(); }

    // GETTERS
    unsigned int get_num_age_segments() const       { return num_age_groups;                }
    unsigned int get_num_exposed_phases() const     { return exposed.front().size();        }
    unsigned int get_num_infected_phases() const    { return infected.front().size();       }
    unsigned int get_num_recovered_phases() const   { return recovered.front().size();      }
    unsigned int get_num_vaccinated1_phases() const { return vaccinatedD1.front().size();   }
    unsigned int get_num_vaccinated2_phases() const { return vaccinatedD2.front().size();   }
    unsigned int get_immunity1_num_weeks() const    { return immunityD1_rate.size();        }
    unsigned int get_immunity2_num_weeks() const    { return immunityD2_rate.size();        }

    /**
     * @brief Sums all the values in a vector
     * 
     * @param state_vector Vector to be summed
     * @return double
    */
    static double sum_state_vector(const vector<double>& state_vector) { return accumulate(state_vector.begin(), state_vector.end(), 0.0); }

    /**
     * @brief Get the total susceptible population count. This includes those who are
     * vaccinated unless specified with the bool.
     * 
     * @param getNVac Used when only wanting to get the non-vaccinated susceptible population.
     * @return double
    */
    double get_total_susceptible(bool getNVac=false, int age_group=-1) const
    {
        double total_susceptible = 0;

        if (age_group == -1)
        {
            // Loop for the age groups
            for (unsigned int i = 0; i < num_age_groups; ++i)
            {
                // Total non-vaccinated
                total_susceptible += susceptible.at(i).front() * age_group_proportions.at(i);

                // Total vaccianted (Dose1 + Dose2)
                if (vaccines && !getNVac)
                {
                    total_susceptible += sum_state_vector(vaccinatedD1.at(i)) * age_group_proportions.at(i);
                    total_susceptible += sum_state_vector(vaccinatedD2.at(i)) * age_group_proportions.at(i);
                }
            }
        }
        else
        {
            total_susceptible = susceptible.at(age_group).front();

            if (vaccines)
            {
                total_susceptible += sum_state_vector(vaccinatedD1.at(age_group));
                total_susceptible += sum_state_vector(vaccinatedD2.at(age_group));
            }
        }

        return total_susceptible;
    }

    /**
     * @brief Gets the total susceptible group with their first dose
     * 
     * @param age_group Will only return the total for that age group
     * @return double 
     */
    double get_total_vaccinatedD1(int age_group=-1) const
    {
        double total_vaccinatedD1 = 0;

        if (age_group == -1)
        {
            for (unsigned int i = 0; i < num_age_groups; ++i)
            {
                // Total vaccinated Dose 1
                total_vaccinatedD1 += sum_state_vector(vaccinatedD1.at(i)) * age_group_proportions.at(i);
            }
        }
        else
            total_vaccinatedD1 = sum_state_vector(vaccinatedD1.at(age_group));

        return total_vaccinatedD1;
    }

    /**
     * @brief Gets the total susceptible group with their second dose
     * 
     * @param age_group Returns the total for those in that age group
     * @return double 
     */
    double get_total_vaccinatedD2(int age_group=-1) const
    {
        double total_vaccinatedD2 = 0;

        if (age_group == -1)
        {
            for (unsigned int i = 0; i < num_age_groups; ++i)
            {
                // Total vaccinated Dose 2
                total_vaccinatedD2 += sum_state_vector(vaccinatedD2.at(i)) * age_group_proportions.at(i);
            }
        }
        else
            total_vaccinatedD2 = sum_state_vector(vaccinatedD2.at(age_group));

        return total_vaccinatedD2;
    }

    /**
     * @brief Gets the total of those exposed including those vaccinated
     * 
     * @param age_group Returns only the total for the specified age group 
     * @return double 
     */
    double get_total_exposed(int age_group=-1) const
    {
        double total_exposed = 0;

        if (age_group == -1)
        {
            for (unsigned int i = 0; i < num_age_groups; ++i)
            {
                // Total non-vaccinated exposed
                total_exposed += sum_state_vector(exposed.at(i)) * age_group_proportions.at(i);

                // Total vaccinated exposed (Dose1 + Dose2)
                if (vaccines)
                {
                    total_exposed += sum_state_vector(exposedD1.at(i)) * age_group_proportions.at(i);
                    total_exposed += sum_state_vector(exposedD2.at(i)) * age_group_proportions.at(i);
                }
            }
        }
        else
        {
            total_exposed += sum_state_vector(exposed.at(age_group));

            if (vaccines)
            {
                total_exposed += sum_state_vector(exposedD1.at(age_group));
                total_exposed += sum_state_vector(exposedD2.at(age_group));
            }
        }

        return total_exposed;
    }

    /**
     * @brief Returns the total infected population inlcuding those who are vaccinated
     * 
     * @param age_group Specifies the age group to compute the total
     * @return double 
     */
    double get_total_infections(int age_group=-1) const
    {
        double total_infections = 0;

        if (age_group == -1)
        {
            for (unsigned int i = 0; i < num_age_groups; ++i)
            {
                // Total non-vaccinated infected
                total_infections += sum_state_vector(infected.at(i)) * age_group_proportions.at(i);

                // Total vaccinated infected (Dose1 + Dose2)
                if (vaccines)
                {
                    total_infections += sum_state_vector(infectedD1.at(i)) * age_group_proportions.at(i);
                    total_infections += sum_state_vector(infectedD2.at(i)) * age_group_proportions.at(i);
                }
            }
        }
        else
        {
            total_infections += sum_state_vector(infected.at(age_group));

            if (vaccines)
            {
                total_infections += sum_state_vector(infectedD1.at(age_group));
                total_infections += sum_state_vector(infectedD2.at(age_group));
            }
        }

        return total_infections;
    }

    /**
     * @brief Returns the total number of those in the recovery phase
     * including those who are vaccinated
     * 
     * @param age_group Returns the total for the specified age group
     * @return double 
     */
    double get_total_recovered(int age_group=-1) const
    {
        double total_recoveries = 0;

        if (age_group == -1)
        {
            for(unsigned int i = 0; i < num_age_groups; ++i)
            {
                // Total non-vaccinated recoveries
                total_recoveries += sum_state_vector(recovered.at(i)) * age_group_proportions.at(i);

                // Total vaccinated recoveries (Dose1 + Dose2)
                if (vaccines)
                {
                    total_recoveries += sum_state_vector(recoveredD1.at(i)) * age_group_proportions.at(i);
                    total_recoveries += sum_state_vector(recoveredD2.at(i)) * age_group_proportions.at(i);
                }
            }
        }
        else
        {
            total_recoveries += sum_state_vector(recovered.at(age_group));

            if (vaccines)
            {
                total_recoveries += sum_state_vector(recoveredD1.at(age_group));
                total_recoveries += sum_state_vector(recoveredD2.at(age_group));
            }
        }

        return total_recoveries;
    }

    /**
     * @brief Returns the total fataltities
     * 
     * @return double 
     */
    double get_total_fatalities() const
    {
        double total_fatalities = 0.0f;

        for (unsigned int i = 0; i < num_age_groups; ++i)
            total_fatalities += fatalities.at(i) * age_group_proportions.at(i);

        return total_fatalities;
    }

    bool operator!=(const sevirds& other) const
    {
        return  (susceptible != other.susceptible) || (vaccinatedD1 != other.vaccinatedD1) || (vaccinatedD2 != other.vaccinatedD2) ||
                (exposed != other.exposed) || (exposedD1 != other.exposedD1) || (exposedD2 != other.exposedD2) ||
                (infected != other.infected) || (infectedD1 != other.infectedD1) || (infectedD2 != other.infectedD2) ||
                (recovered != other.recovered) || (recoveredD1 != other.recoveredD1) || (recoveredD2 != other.recoveredD2) ||
                (fatalities != other.fatalities);
    }

    /**
     * @brief Handles setting the desired decimal point without using division
     * 
     * @param proportion Value to be corrected
     * @return double
     */
    double precision_divider(double proportion) const { return round(proportion * prec_divider) * one_over_prec_divider; }
}; //struct servids{}

/**
 * @brief Outputs <population, S, E, VD1, VD2, I, R, new E, new I, new R, D>
 * 
 * @param os Out stream object to pipe into
 * @param sevirds Current simulation data
 * @return ostream& 
 */
ostream &operator<<(ostream& os, const sevirds& sevirds)
{
    double new_exposed    = 0;
    double new_infections = 0;
    double new_recoveries = 0;

    double age_group_proportion;

    // Calculate the new exposures, infectsions and recoveries
    // on the first day of each respective phase
    for (unsigned int i = 0; i < sevirds.num_age_groups; ++i)
    {
        // Get the age group
        age_group_proportion = sevirds.age_group_proportions.at(i);

        // Non-Vaccinated
        new_exposed    += sevirds.exposed.at(i).front()   * age_group_proportion; // Exposed
        new_infections += sevirds.infected.at(i).front()  * age_group_proportion; // Infected
        new_recoveries += sevirds.recovered.at(i).front() * age_group_proportion; // Recovered

        // Vaccinated
        if (sevirds.vaccines)
        {
            // Dose 1
            new_exposed    += sevirds.exposedD1.at(i).front()   * age_group_proportion;
            new_infections += sevirds.infectedD1.at(i).front()  * age_group_proportion;
            new_recoveries += sevirds.recoveredD1.at(i).front() * age_group_proportion;

            // Dose 2
            new_exposed    += sevirds.exposedD2.at(i).front()   * age_group_proportion;
            new_infections += sevirds.infectedD2.at(i).front()  * age_group_proportion;
            new_recoveries += sevirds.recoveredD2.at(i).front() * age_group_proportion;
        }
    }

    // Precision corrrection
    new_exposed    = sevirds.precision_divider(new_exposed);
    new_infections = sevirds.precision_divider(new_infections);
    new_recoveries = sevirds.precision_divider(new_recoveries);

    // Calculate the totals from each day in every phase
    double total_susceptible = sevirds.precision_divider(sevirds.get_total_susceptible(true));
    double total_exposed     = sevirds.precision_divider(sevirds.get_total_exposed());
    double total_infected    = sevirds.precision_divider(sevirds.get_total_infections());
    double total_recovered   = sevirds.precision_divider(sevirds.get_total_recovered());
    double total_fatalities  = sevirds.precision_divider(sevirds.get_total_fatalities());

    // Susceptible Vaccinated
    double total_vaccinatedD1 = 0.0, total_vaccinatedD2 = 0.0;
    if (sevirds.vaccines)
    {
        total_vaccinatedD1 = sevirds.precision_divider(sevirds.get_total_vaccinatedD1());
        total_vaccinatedD2 = sevirds.precision_divider(sevirds.get_total_vaccinatedD2());
    }

    // Pipe all the data
    os << "<" << sevirds.population << "," << total_susceptible << "," << total_exposed << "," << total_vaccinatedD1
        << "," << total_vaccinatedD2 << "," << total_infected << "," << total_recovered << "," << new_exposed
        << "," << new_infections << "," << new_recoveries << "," << total_fatalities << ">";
    return os;
}

/**
 * @brief Reads the data from the json under the "default" parameter
 * 
 * @param json Contains the json file
 * @param current_sevirds Object to store the data
 */
void from_json(const nlohmann::json &json, sevirds &current_sevirds)
{
    json.at("population").get_to(current_sevirds.population);
    json.at("age_group_proportions").get_to(current_sevirds.age_group_proportions);

    try { json.at("susceptible").get_to(current_sevirds.susceptible); }
    catch(nlohmann::detail::type_error &e) { AssertLong(false, __FILE__, __LINE__, "Error reading the susceptible vector from either default.json OR infectedCell.json\nVerify the format is [[#], [#], ...] and NOT [#, #, ...]"); }

    json.at("vaccinatedD1").get_to(current_sevirds.vaccinatedD1);
    json.at("vaccinatedD2").get_to(current_sevirds.vaccinatedD2);

    json.at("exposed").get_to(current_sevirds.exposed);
    json.at("exposedD1").get_to(current_sevirds.exposedD1);
    json.at("exposedD2").get_to(current_sevirds.exposedD2);

    json.at("infected").get_to(current_sevirds.infected);
    json.at("infectedD1").get_to(current_sevirds.infectedD1);
    json.at("infectedD2").get_to(current_sevirds.infectedD2);

    json.at("recovered").get_to(current_sevirds.recovered);
    json.at("recoveredD1").get_to(current_sevirds.recoveredD1);
    json.at("recoveredD2").get_to(current_sevirds.recoveredD2);

    json.at("fatalities").get_to(current_sevirds.fatalities);

    json.at("disobedient").get_to(current_sevirds.disobedient);
    json.at("hospital_capacity").get_to(current_sevirds.hospital_capacity);
    json.at("fatality_modifier").get_to(current_sevirds.fatality_modifier);

    json.at("immunityD1").get_to(current_sevirds.immunityD1_rate);
    json.at("immunityD2").get_to(current_sevirds.immunityD2_rate);
    json.at("min_interval_between_doses").get_to(current_sevirds.min_interval_doses);
    json.at("min_interval_between_recoverie_and_vaccine").get_to(current_sevirds.min_interval_recovery_to_vaccine);

    current_sevirds.num_age_groups = current_sevirds.age_group_proportions.size();
    unsigned int age_groups        = current_sevirds.num_age_groups;

    AssertLong(accumulate(current_sevirds.age_group_proportions.begin(), current_sevirds.age_group_proportions.end(), 0.0) == 1,
                __FILE__, __LINE__,
                "The age group proportions need to add up to 1");

    // Checks if the phases have the correct number of age groups
    AssertLong(age_groups <= current_sevirds.susceptible.size() && age_groups <= current_sevirds.exposed.size() && age_groups <= current_sevirds.infected.size() &&
                    age_groups <= current_sevirds.recovered.size() && age_groups <= current_sevirds.fatalities.size() && age_groups <= current_sevirds.vaccinatedD1.size() &&
                    age_groups <= current_sevirds.vaccinatedD2.size() && age_groups <= current_sevirds.immunityD1_rate.size() && age_groups <= current_sevirds.immunityD2_rate.size() &&
                    age_groups <= current_sevirds.exposedD1.size() && age_groups <= current_sevirds.infectedD2.size() && age_groups <= current_sevirds.recoveredD2.size() &&
                    age_groups <= current_sevirds.exposedD2.size() && age_groups <= current_sevirds.infectedD2.size() && age_groups <= current_sevirds.recoveredD2.size(),
                __FILE__, __LINE__,
                "There must be at least " + to_string(age_groups) + " age groups for each of the lists under the 'states' parameter in default.json as well as in infectedCell.json");

    for (unsigned int a = 0; a < age_groups; ++a)
    {
        double pop = current_sevirds.susceptible.at(a).front()
                    + accumulate(current_sevirds.exposed.at(a).begin(),   current_sevirds.exposed.at(a).end(),   0.0)
                    + accumulate(current_sevirds.infected.at(a).begin(),  current_sevirds.infected.at(a).end(),  0.0)
                    + accumulate(current_sevirds.recovered.at(a).begin(), current_sevirds.recovered.at(a).end(), 0.0)
                    + current_sevirds.fatalities.at(a)
                    + accumulate(current_sevirds.vaccinatedD1.at(a).begin(), current_sevirds.vaccinatedD1.at(a).end(), 0.0)
                    + accumulate(current_sevirds.vaccinatedD2.at(a).begin(), current_sevirds.vaccinatedD2.at(a).end(), 0.0)
                    + accumulate(current_sevirds.exposedD1.at(a).begin(),    current_sevirds.exposedD1.at(a).end(),    0.0)
                    + accumulate(current_sevirds.exposedD2.at(a).begin(),    current_sevirds.exposedD2.at(a).end(),    0.0)
                    + accumulate(current_sevirds.infectedD1.at(a).begin(),   current_sevirds.infectedD1.at(a).end(),   0.0)
                    + accumulate(current_sevirds.infectedD2.at(a).begin(),   current_sevirds.infectedD2.at(a).end(),   0.0)
                    + accumulate(current_sevirds.recoveredD1.at(a).begin(),  current_sevirds.recoveredD1.at(a).end(),  0.0)
                    + accumulate(current_sevirds.recoveredD2.at(a).begin(),  current_sevirds.recoveredD2.at(a).end(),  0.0);

                    AssertLong(pop == 1.0, __FILE__, __LINE__, "The vectors don't add up to 1! " + to_string(pop) + " Double check the values in default.json AND infectedCell.json");
    }

    for (unsigned int i = 0; i < age_groups; ++i)
    {
        AssertLong(current_sevirds.get_total_vaccinatedD1() + current_sevirds.get_total_vaccinatedD2() <= 1.0f,
                    __FILE__, __LINE__,
                    "People can only be in one of three groups: Unvaccinated, Vaccinated-Dose1, or Vaccinated-Dose2.\nThe proportion of people with dose 1 plus those with dose 2 cannot be greater then 1");
    }

    // Recovered Dose 1 can't be smaller then Susceptible Vaccinated Dose 1
    AssertLong(current_sevirds.recoveredD1.front().size() >= current_sevirds.vaccinatedD1.front().size(),
                __FILE__, __LINE__,
                "The recovery phase for those vaccinated with their first dose needs to be smaller then vaccinatedD1!");
}

#endif //PANDEMIC_HOYA_2002_SEIRD_HPP