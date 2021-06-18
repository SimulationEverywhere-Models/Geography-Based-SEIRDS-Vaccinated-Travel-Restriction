// Created by binybrion - 06/30/20
// Modified by Glenn    - 02/07/20

#ifndef PANDEMIC_HOYA_2002_SEIRD_HPP
#define PANDEMIC_HOYA_2002_SEIRD_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include "hysteresis_factor.hpp"

using namespace std;

struct sevirds
{
    vector<double> age_group_proportions;
    vector<double> susceptible;
    vector<vector<double>> exposed;
    vector<double> vaccinatedD1;
    vector<double> vaccinatedD2;
    vector<vector<double>> infected;
    vector<vector<double>> recovered;
    vector<double> fatalities;
    float immunityD1_rate;
    float immunityD2_rate;
    unordered_map<string, hysteresis_factor> hysteresis_factors;
    double population;

    double disobedient;
    double hospital_capacity;
    double fatality_modifier;

    unsigned int num_age_groups;

    // Required for the JSON library, as types used with it must be default-constructable.
    // The overloaded constructor results in a default constructor having to be manually written.
    sevirds() = default;

    sevirds(vector<double> sus, vector<double> exp, double vac1, double vac2, vector<double> inf, vector<double> rec,
        double fat, double dis, double hcap, double fatm) :
            susceptible{move(sus)}, exposed{move(exp)}, vaccinatedD1{vac1}, vaccinatedD2{vac2},
            infected{move(inf)}, recovered{move(rec)}, fatalities{fat}, disobedient{dis},
            hospital_capacity{hcap}, fatality_modifier{fatm} { num_age_groups = age_group_proportions.size(); }

    unsigned int get_num_age_segments() const       { return susceptible.size();        }
    unsigned int get_num_exposed_phases() const     { return exposed.front().size();    }
    unsigned int get_num_infected_phases() const    { return infected.front().size();   }
    unsigned int get_num_recovered_phases() const   { return recovered.front().size();  }

    static double sum_state_vector(const vector<double>& state_vector) {
        return accumulate(state_vector.begin(), state_vector.end(), 0.0f);
    }

    double get_total_fatalities() const
    {
        double total_fatalities = 0.0f;

        for (int i = 0; i < age_group_proportions.size(); ++i)
            total_fatalities += fatalities.at(i) * age_group_proportions.at(i);

        return total_fatalities;
    }

    double get_total_exposed() const
    {
        float total_exposed = 0.0f;

        for (int i = 0; i < age_group_proportions.size(); ++i)
            total_exposed += sum_state_vector(exposed.at(i)) * age_group_proportions.at(i);

        return total_exposed;
    }

    // Gets the total of both 1 and 2 doses
    double get_total_vaccinated() const
    {
        float total_vaccinated = 0.0f;

        for (int i = 0; i < age_group_proportions.size(); ++i)
            total_vaccinated += get_total_vaccinatedD1() + get_total_vaccinatedD2();

        return total_vaccinated;
    }

    double get_total_vaccinatedD1() const
    {
        float total_vaccinatedD1 = 0.0f;

        for (int i = 0; i < age_group_proportions.size(); ++i)
            total_vaccinatedD1 += vaccinatedD1.at(i) * age_group_proportions.at(i);

        return total_vaccinatedD1;
    }

    double get_total_vaccinatedD2() const
    {
        float total_vaccinatedD2 = 0.0f;

        for (int i = 0; i < age_group_proportions.size(); ++i)
            total_vaccinatedD2 += vaccinatedD2.at(i) * age_group_proportions.at(i);

        return total_vaccinatedD2;
    }

    double get_total_infections() const
    {
        float total_infections = 0.0f;

        for (int i = 0; i < age_group_proportions.size(); ++i)
            total_infections += sum_state_vector(infected.at(i)) * age_group_proportions.at(i);

        return total_infections;
    }

    double get_total_recovered() const
    {
        double total_recoveries = 0.0f;

        for(int i = 0; i < age_group_proportions.size(); ++i)
            total_recoveries += sum_state_vector(recovered.at(i)) * age_group_proportions.at(i);

        return total_recoveries;
    }

    double get_total_susceptible() const
    {
        double total_susceptible = 0.0f;

        for(int i = 0; i < age_group_proportions.size(); ++i)
            total_susceptible += susceptible.at(i) * age_group_proportions.at(i);

        return total_susceptible;
    }

    bool operator!=(const sevirds& other) const {
        return (susceptible != other.susceptible) || (exposed != other.exposed) || (vaccinatedD1 != other.vaccinatedD1) ||
                (vaccinatedD2 != other.vaccinatedD2) || (infected != other.infected) || (recovered != other.recovered);
    }
}; //struct servids{}

bool operator<(const sevirds& lhs, const sevirds& rhs) { return true; }

// outputs <population, S, E, VD1, VD2, I, R, new E, new I, new R, D>
ostream &operator<<(ostream& os, const sevirds& sevirds) {

    double new_exposed = 0.0f;
    double new_infections = 0.0f;
    double new_recoveries = 0.0f;

    for (int i = 0; i < sevirds.age_group_proportions.size(); ++i) 
    {
        new_exposed     += sevirds.exposed.at(i).at(0) * sevirds.age_group_proportions.at(i);
        new_infections  += sevirds.infected.at(i).at(0) * sevirds.age_group_proportions.at(i);
        new_recoveries  += sevirds.recovered.at(i).at(0) * sevirds.age_group_proportions.at(i);
    }

    os << "<" << sevirds.population << "," << sevirds.get_total_susceptible() << "," << sevirds.get_total_exposed() << "," << sevirds.get_total_vaccinatedD1()
        << "," << sevirds.get_total_vaccinatedD2() << "," << sevirds.get_total_infections() << "," << sevirds.get_total_recovered() << "," << new_exposed
        << "," << new_infections << "," << new_recoveries << "," << sevirds.get_total_fatalities() << ">";
    return os;
}

void from_json(const nlohmann::json &json, sevirds &current_sevirds)
{
    json.at("age_group_proportions").get_to(current_sevirds.age_group_proportions);
    json.at("infected").get_to(current_sevirds.infected);
    json.at("recovered").get_to(current_sevirds.recovered);
    json.at("susceptible").get_to(current_sevirds.susceptible);
    json.at("exposed").get_to(current_sevirds.exposed);
    json.at("vaccinatedD1").get_to(current_sevirds.vaccinatedD1);
    json.at("vaccinatedD2").get_to(current_sevirds.vaccinatedD2);
    json.at("fatalities").get_to(current_sevirds.fatalities);
    json.at("disobedient").get_to(current_sevirds.disobedient);
    json.at("hospital_capacity").get_to(current_sevirds.hospital_capacity);
    json.at("fatality_modifier").get_to(current_sevirds.fatality_modifier);
    json.at("population").get_to(current_sevirds.population);
    json.at("immunityD1").get_to(current_sevirds.immunityD1_rate);
    json.at("immunityD2").get_to(current_sevirds.immunityD2_rate);

    current_sevirds.num_age_groups = current_sevirds.age_group_proportions.size();

    assert(current_sevirds.age_group_proportions.size() == current_sevirds.susceptible.size() && current_sevirds.age_group_proportions.size() == current_sevirds.exposed.size() &&
            current_sevirds.age_group_proportions.size() == current_sevirds.infected.size() && current_sevirds.age_group_proportions.size() == current_sevirds.recovered.size() &&
            "There must be an equal number of age groups between age_group_proportions, susceptible, exposed, infected, and recovered!\n");

    // Three options: unvaccinated, dose 1 or dose 2. Can only be in one of those groups
    for (unsigned int i = 0; i < current_sevirds.num_age_groups; ++i)
        if ( !(current_sevirds.vaccinatedD1.at(i) + current_sevirds.vaccinatedD2.at(i) <= 1.0f) )
        {
            cout << "\033[1;31mASSERT: \033[0;31mPeople can only be in one of three groups: Unvaccinated, Vaccinated-Dose1, or Vaccinated-Dose2. The proportion of people with dose 1 plus those with dose 2 cannot be greater then 1\033[0m" << endl;
            assert(false);
        }
}

#endif //PANDEMIC_HOYA_2002_SEIRD_HPP