// Created by binybrion - 07/03/20
// Modified by Glenn    - 02/07/20

#ifndef PANDEMIC_HOYA_2002_SIMULATION_CONFIG_HPP
#define PANDEMIC_HOYA_2002_SIMULATION_CONFIG_HPP

#include <nlohmann/json.hpp>

struct simulation_config
{
    int prec_divider;
    using phase_rates = std::vector<std::vector<double>>;

    phase_rates virulence_rates;
    phase_rates incubation_rates;
    phase_rates recovery_rates;
    phase_rates mobility_rates;
    phase_rates fatality_rates;
    std::vector<double> vac_rates_dose1;
    phase_rates vac_rates_dose2;

    bool SIIRS_model, is_vaccination;
};

void from_json(const nlohmann::json& json, simulation_config& v)
{
    json.at("precision").get_to(v.prec_divider);
    json.at("virulence_rates").get_to(v.virulence_rates);
    json.at("incubation_rates").get_to(v.incubation_rates);
    json.at("recovery_rates").get_to(v.recovery_rates);
    json.at("mobility_rates").get_to(v.mobility_rates);
    json.at("fatality_rates").get_to(v.fatality_rates);
    json.at("vaccination_rates_dose1").get_to(v.vac_rates_dose1);
    json.at("vaccination_rates_dose2").get_to(v.vac_rates_dose2);
    json.at("SIIRS_model").get_to(v.SIIRS_model);
    json.at("Vaccinations").get_to(v.is_vaccination);

    unsigned int age_groups     = v.recovery_rates.size();
    unsigned int recovery_days  = v.recovery_rates.at(0).size();

    for (unsigned int i = 0; i < age_groups; ++i)
    {
        std::vector<double>& v_recovery_rates = v.recovery_rates.at(i);
        std::vector<double>& v_fatality_rates = v.fatality_rates.at(i);

        for (unsigned int k = 0; k < recovery_days; ++k)
            // A sum of greater than one refers to more than the entire population of an infection stage.
            assert(v_recovery_rates.at(k) + v_fatality_rates.at(k) <= 1.0f && "The recovery rate + fatality rate cannot exceed 1!");

        assert(v_fatality_rates.back() <= 1.0f && "The fatality rate cannot exceed one!"); // Assert because the recovery rate has
                                                                                           // one less entry than the fatality rates.
    }
}

#endif //PANDEMIC_HOYA_2002_SIMULATION_CONFIG_HPP