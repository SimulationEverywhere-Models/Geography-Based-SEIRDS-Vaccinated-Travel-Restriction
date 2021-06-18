// Created by binybrion - 06/29/20
// Modified by Glenn    - 02/07/20

#ifndef PANDEMIC_HOYA_2002_ZHONG_COUPLED_HPP
#define PANDEMIC_HOYA_2002_ZHONG_COUPLED_HPP

#include <nlohmann/json.hpp>
#include <cadmium/celldevs/coupled/cells_coupled.hpp>
#include "cells/geographical_cell.hpp"

template <typename T>
class geographical_coupled : public cadmium::celldevs::cells_coupled<T, string, sevirds, vicinity>
{
    public:
        explicit geographical_coupled(string const &id) : cells_coupled<T, string, sevirds, vicinity>(id) { }

        template<typename X>
        using cell_unordered = unordered_map<string, X>;

        void add_cell_json(string const& cell_type, string const& cell_id,
                            cell_unordered<vicinity> const& neighborhood,
                            sevirds initial_state,
                            string const& delay_id,
                            nlohmann::json const& config) override
        {
            if (cell_type == "zhong")
            {
                auto conf = config.get<typename geographical_cell<T>::config_type>();
                this->template add_cell<geographical_cell>(cell_id, neighborhood, initial_state, delay_id, conf);
            } else throw bad_typeid();
        }
};

#endif //PANDEMIC_HOYA_2002_ZHONG_COUPLED_HPP