//
// Created by binybrion on 6/30/20.
// Modified by Glenn 02/07/20

#ifndef PANDEMIC_HOYA_2002_SEIRD_HPP
#define PANDEMIC_HOYA_2002_SEIRD_HPP

#include <iostream>
#include <nlohmann/json.hpp>
#include "hysteresis_factor.hpp"

struct seird {
    std::vector<double> age_group_proportions;
    std::vector<double> susceptible;
    std::vector<std::vector<double>> exposed;
    std::vector<std::vector<double>> infected;
    std::vector<std::vector<double>> recovered;
    std::vector<double> fatalities;
    std::unordered_map<std::string, hysteresis_factor> hysteresis_factors;
    double population;

    double disobedient;
    double hospital_capacity;
    double fatality_modifier;

    // Required for the JSON library, as types used with it must be default-constructable.
    // The overloaded constructor results in a default constructor having to be manually written.
    seird() = default;

    seird(std::vector<double> sus, std::vector<double> exp, std::vector<double> inf, std::vector<double> rec,
        double fat, double dis, double hcap, double fatm) :
            susceptible{std::move(sus)}, exposed{std::move(exp)}, infected{std::move(inf)},
            recovered{std::move(rec)}, fatalities{fat}, disobedient{dis},
            hospital_capacity{hcap}, fatality_modifier{fatm} {}

    unsigned int get_num_age_segments() const {
        return susceptible.size();
    }

    unsigned int get_num_exposed_phases() const {
        return exposed.front().size();
    }

    unsigned int get_num_infected_phases() const {
        return infected.front().size();
    }

    unsigned int get_num_recovered_phases() const {
        return recovered.front().size();
    }

    static double sum_state_vector(const std::vector<double> &state_vector) {
        return std::accumulate(state_vector.begin(), state_vector.end(), 0.0f);
    }

    double get_total_fatalities() const {
        double total_fatalities = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_fatalities += fatalities.at(i) * age_group_proportions.at(i);
        }
        return total_fatalities;
    }

    double get_total_exposed() const {
        float total_exposed = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_exposed += sum_state_vector(exposed.at(i)) * age_group_proportions.at(i);
        }
        return total_exposed;
    }
    
    double get_total_infections() const {
        float total_infections = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_infections += sum_state_vector(infected.at(i)) * age_group_proportions.at(i);
        }
        return total_infections;
    }

    double get_total_recovered() const {
        double total_recoveries = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_recoveries += sum_state_vector(recovered.at(i)) * age_group_proportions.at(i);
        }
        return total_recoveries;
    }

    double get_total_susceptible() const {
        double total_susceptible = 0.0f;
        for(int i = 0; i < age_group_proportions.size(); ++i) {
            total_susceptible += susceptible.at(i) * age_group_proportions.at(i);
        }
        return total_susceptible;
    }

    bool operator!=(const seird &other) const {
        return (susceptible != other.susceptible) || (exposed != other.exposed) || (infected != other.infected) || (recovered != other.recovered);
    }
};

bool operator<(const seird &lhs, const seird &rhs) { return true; }


// outputs <population, S, E, I, R, new I, new E, new R, D>
std::ostream &operator<<(std::ostream &os, const seird &seird) {

    double new_exposed = 0.0f;
    double new_infections = 0.0f;
    double new_recoveries = 0.0f;

    for(int i = 0; i < seird.age_group_proportions.size(); ++i) {
        new_exposed += seird.exposed.at(i).at(0) * seird.age_group_proportions.at(i);
        new_infections += seird.infected.at(i).at(0) * seird.age_group_proportions.at(i);
        new_recoveries += seird.recovered.at(i).at(0) * seird.age_group_proportions.at(i);
    }

    os << "<" << seird.population << "," << seird.get_total_susceptible()
        << "," << seird.get_total_exposed() << "," << seird.get_total_infections() << "," << seird.get_total_recovered() << "," 
        << new_exposed << "," << new_infections << "," << new_recoveries << "," << seird.get_total_fatalities() << ">";
    return os;
}

void from_json(const nlohmann::json &json, seird &current_seird) {
    json.at("age_group_proportions").get_to(current_seird.age_group_proportions);
    json.at("infected").get_to(current_seird.infected);
    json.at("recovered").get_to(current_seird.recovered);
    json.at("susceptible").get_to(current_seird.susceptible);
    json.at("exposed").get_to(current_seird.exposed);
    json.at("fatalities").get_to(current_seird.fatalities);
    json.at("disobedient").get_to(current_seird.disobedient);
    json.at("hospital_capacity").get_to(current_seird.hospital_capacity);
    json.at("fatality_modifier").get_to(current_seird.fatality_modifier);
    json.at("population").get_to(current_seird.population);
    
    assert(current_seird.age_group_proportions.size() == current_seird.susceptible.size() && current_seird.age_group_proportions.size() == current_seird.exposed.size() 
    	&& current_seird.age_group_proportions.size() == current_seird.infected.size() && current_seird.age_group_proportions.size() == current_seird.recovered.size() 
    	&& "There must be an equal number of age groups between age_group_proportions, susceptible, exposed, infected, and recovered!\n");
}

#endif //PANDEMIC_HOYA_2002_SEIRD_HPP
