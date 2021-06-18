// Created by binybrion - 06/29/20

#ifndef CELL_DEVS_ZHONG_DEVEL_VICINITY_H
#define CELL_DEVS_ZHONG_DEVEL_VICINITY_H

#include <functional>
#include <cmath>
#include <nlohmann/json.hpp>
#include "hysteresis_factor.hpp"

using namespace std;

struct vicinity
{
    using infection_threshold = float;
    using mobility_correction_factor = array<float, 2>; // The first value is the mobility correction factor;
                                                             // The second one is the hysteresis factor.

    map<infection_threshold, mobility_correction_factor> correction_factors;

    double correlation = 1.0f;

    explicit vicinity(double correlation) : correlation{correlation} { }

    vicinity() { }
};

void from_json(const nlohmann::json& json, vicinity& vicinity)
{
    json.at("correlation").get_to(vicinity.correlation);

    map<string, array<float, 2>> unparsed_infection_correction_factors;

    json.at("infection_correction_factors").get_to(unparsed_infection_correction_factors);

    for (const auto& i : unparsed_infection_correction_factors)
    {
        float infection_threshold;
        try {
            infection_threshold = stof(i.first);
        }
        catch(invalid_argument& e)
        {
            // If the key is invalid, the default error message from the exception is not informative.
            // Therefore it is re-thrown with a more descriptive message.
            throw invalid_argument{"Failed to parse infection correction factor key: " + i.first};
        }

        if (infection_threshold < 0.0f || infection_threshold > 1.0f)
        {
            string error_message = "Invalid key specified (a values must be in the range of [0, 1]:";
            error_message += "Key: " + i.first;

            throw invalid_argument{error_message};
        }

        if (i.second.front() < 0.0f || i.second.front() > 1.0f)
        {
            string error_message = "For the key: " + i.first + " the mobility factor must be in the range of: [0, 1].";
            error_message += " Erroneous value: " + to_string(i.second.front());

            throw invalid_argument{error_message};
        }

        if (i.second.back() < 0.0f || i.second.back() > infection_threshold)
        {
            string error_message = "Invalid hysteresis specified for the key: " + i.first;
            error_message += " The hysteresis value (" + to_string(i.second.back()) + ") must not exceed:";
            error_message += "(" + i.first + ")";

            throw runtime_error{error_message};
        }

        vicinity.correction_factors.insert({infection_threshold, i.second});
    }
} //from_json()

#endif //CELL_DEVS_ZHONG_DEVEL_VICINITY_H