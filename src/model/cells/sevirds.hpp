// Created by binybrion - 06/30/20
// Modified by Glenn    - 02/07/20

#ifndef PANDEMIC_HOYA_2002_SEIRD_HPP
#define PANDEMIC_HOYA_2002_SEIRD_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include "hysteresis_factor.hpp"

using namespace std;

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

    //* Susceptible
    proportionVector susceptible;
    proportionVector vaccinatedD1;
    proportionVector vaccinatedD2;

    //* Exposed
    proportionVector exposed;
    proportionVector exposedD1;
    proportionVector exposedD2;

    //* Infected
    proportionVector infected;
    proportionVector infectedD1;
    proportionVector infectedD2;

    //* Recovered
    proportionVector recovered;
    proportionVector recoveredD1;
    proportionVector recoveredD2;

    //* Fatalities
    vector<double> fatalities;

    double disobedient;
    double hospital_capacity;
    double fatality_modifier;

    proportionVector immunityD1_rate;
    proportionVector immunityD2_rate;
    unsigned int min_interval_doses;

    unordered_map<string, hysteresis_factor> hysteresis_factors;
    unsigned int num_age_groups;

    bool vaccines; // Are vaccines being modeled?

    // Required for the JSON library, as types used with it must be default-constructable.
    // The overloaded constructor results in a default constructor having to be manually written.
    sevirds() = default;

    sevirds(proportionVector sus, proportionVector vac1, proportionVector vac2,
            proportionVector exp, proportionVector exp1, proportionVector exp2,
            proportionVector inf, proportionVector inf1, proportionVector inf2,
            proportionVector rec, proportionVector rec1, proportionVector rec2,
            vector<double> fat, double dis, double hcap, double fatm, proportionVector immuD1, unsigned int min_interval,
            proportionVector immuD2, bool vac=false) :
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
                vaccines{vac}
    { num_age_groups = age_group_proportions.size(); }

    unsigned int get_num_age_segments() const       { return num_age_groups;                }
    unsigned int get_num_exposed_phases() const     { return exposed.front().size();        }
    unsigned int get_num_infected_phases() const    { return infected.front().size();       }
    unsigned int get_num_recovered_phases() const   { return recovered.front().size();      }
    unsigned int get_num_vaccinated1_phases() const { return vaccinatedD1.front().size();   }
    unsigned int get_num_vaccinated2_phases() const { return vaccinatedD2.front().size();   }
    unsigned int get_immunity1_num_weeks() const    { return immunityD1_rate.size();        }
    unsigned int get_immunity2_num_weeks() const    { return immunityD2_rate.size();        }

    static double sum_state_vector(const vector<double>& state_vector) { return accumulate(state_vector.begin(), state_vector.end(), 0.0f); }

    /**
     * @param getNVac: Used when only wanting to get the non-vaccinated susceptible population.
    */
    double get_total_susceptible(bool getNVac=false) const
    {
        double total_susceptible = 0;

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

        return total_susceptible;
    }

    double get_total_vaccinatedD1() const
    {
        double total_vaccinatedD1 = 0;

        for (unsigned int i = 0; i < num_age_groups; ++i)
        {
            // Total vaccinated Dose 1
            total_vaccinatedD1 += sum_state_vector(vaccinatedD1.at(i)) * age_group_proportions.at(i);
        }

        return total_vaccinatedD1;
    }

    double get_total_vaccinatedD2() const
    {
        double total_vaccinatedD2 = 0;

        for (unsigned int i = 0; i < num_age_groups; ++i)
        {
            // Total vaccinated Dose 2
            total_vaccinatedD2 += sum_state_vector(vaccinatedD2.at(i)) * age_group_proportions.at(i);
        }

        return total_vaccinatedD2;
    }

    double get_total_exposed() const
    {
        double total_exposed = 0;

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

        return total_exposed;
    }

    double get_total_infections() const
    {
        double total_infections = 0;

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

        return total_infections;
    }

    double get_total_recovered() const
    {
        double total_recoveries = 0;

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

        return total_recoveries;
    }

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
}; //struct servids{}

bool operator<(const sevirds& lhs, const sevirds& rhs) { return true; }

// outputs <population, S, E, VD1, VD2, I, R, new E, new I, new R, D>
ostream &operator<<(ostream& os, const sevirds& sevirds)
{
    double new_exposed      = 0;
    double new_infections   = 0;
    double new_recoveries   = 0;

    double age_group_proportion;

    for (unsigned int i = 0; i < sevirds.num_age_groups; ++i)
    {
        age_group_proportion = sevirds.age_group_proportions.at(i);

        new_exposed    += sevirds.exposed.at(i).at(0) * age_group_proportion;   // Non-vaccinated exposed
        new_infections += sevirds.infected.at(i).at(0) * age_group_proportion;  // Non-vaccinated infected
        new_recoveries += sevirds.recovered.at(i).at(0) * age_group_proportion; // Non-vaccinated recovered

        if (sevirds.vaccines)
        {
            // Dose 1
            new_exposed    += sevirds.exposedD1.at(i).at(0) * age_group_proportion;
            new_infections += sevirds.infectedD1.at(i).at(0) * age_group_proportion;
            new_recoveries += sevirds.recoveredD1.at(i).at(0) * age_group_proportion;

            // Dose 2
            new_exposed    += sevirds.exposedD2.at(i).at(0) * age_group_proportion;
            new_infections += sevirds.infectedD2.at(i).at(0) * age_group_proportion;
            new_recoveries += sevirds.recoveredD2.at(i).at(0) * age_group_proportion;
        }
    }

    os << "<" << sevirds.population << "," << sevirds.get_total_susceptible(true) << "," << sevirds.get_total_exposed() << "," << sevirds.get_total_vaccinatedD1()
        << "," << sevirds.get_total_vaccinatedD2() << "," << sevirds.get_total_infections() << "," << sevirds.get_total_recovered() << "," << new_exposed
        << "," << new_infections << "," << new_recoveries << "," << sevirds.get_total_fatalities() << ">";
    return os;
}

void from_json(const nlohmann::json &json, sevirds &current_sevirds)
{
    json.at("population").get_to(current_sevirds.population);
    json.at("age_group_proportions").get_to(current_sevirds.age_group_proportions);

    json.at("susceptible").get_to(current_sevirds.susceptible);
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
    json.at("min_interval_between_doses").get_to(current_sevirds.min_interval_doses);
    json.at("immunityD2").get_to(current_sevirds.immunityD2_rate);

    current_sevirds.num_age_groups = current_sevirds.age_group_proportions.size();

    assert(current_sevirds.age_group_proportions.size() == current_sevirds.susceptible.size() && current_sevirds.age_group_proportions.size() == current_sevirds.exposed.size() &&
            current_sevirds.age_group_proportions.size() == current_sevirds.infected.size() && current_sevirds.age_group_proportions.size() == current_sevirds.recovered.size() &&
            "There must be an equal number of age groups between age_group_proportions, susceptible, exposed, infected, and recovered!\n");

    // Three options: unvaccinated, dose 1 or dose 2. Can only be in one of those groups
    for (unsigned int i = 0; i < current_sevirds.num_age_groups; ++i)
        if ( current_sevirds.vaccines && current_sevirds.get_total_vaccinatedD1() + current_sevirds.get_total_vaccinatedD2() > 1.0f )
        {
            cout << "\033[1;31mASSERT: \033[0;31mPeople can only be in one of three groups: Unvaccinated, Vaccinated-Dose1, or Vaccinated-Dose2. The proportion of people with dose 1 plus those with dose 2 cannot be greater then 1\033[0m" << endl;
            assert(false);
        }
}

#endif //PANDEMIC_HOYA_2002_SEIRD_HPP