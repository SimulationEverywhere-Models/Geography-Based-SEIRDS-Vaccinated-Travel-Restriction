Understanding the Files
---
default.json - Is the default cell and simulation config, used as an input to the scenario generator

fields.json - Describes the format of the state object written to logs. It is inserted outside of "cells", it ised used by the GIS web viewer v2

infectedCell.json - Describes a cell ID and infected state object where the infection should begin. The scenario json is first generated with each cell having the default.json values, and then the infected cell's state object is overwritten with the values in infectedCell.json
Currently one one cell in this file is supported, and this file must exist for the post processing to work.

pre_processing.json - Is the output of generate_cadmium_json.py, the user should never have to edit this, it is an intermediate step

Understanding the Fields
---
default
* **delay**: _N/A_
* **cell_type**: The type of cell used for simulating regions. Best to leave this as _zhong_ unless you know what you're doing
* **age_group_proportions**: Splits the population into 5 age groups from youngest to eldest (left to right)
* **vaccinatedD1**: Proportion of each age group (follows the same order as _age_group_proportions_) that have their first dose
* **vaccinatedD2**: Proportion of each age group that have their second dose (**_Note:_** The population cannot be in D2 and D1 at the same time and cannnot regress)
* **susceptible**: How susceptible each age group is to Covid 19
* **exposed**: There are as many lists here as there are age groups and their lengths represent the number of days the population is in the exposed state (so 14)
* **infected**: Same as _exposed_ but this state only lasts 12 days (after that a new state is calculated)
* **recovered**: Same as the previous two but recovery state lasts a lot longer. Therefore, if re-susceptibility is turned on there's a grace period before some of the population can get re-infected
* **fatalities**: Fatalities for each age group
* **disobedient**: Percentage of population that will disobey Covid 19 guidelines and measures
* **hospital_capacity**: Percentage of population that can be housed in hospitals
* **fatality_modifiers**: Comes into effect when the hospitals are full and increases fatality rates
* **immunityD1**: Immunity received from 1 dose
* **immunityD2**: Immunity received from 2 doses

config
* **precision**: Used to get cleaner decimal points. Best not to touch this unless you know what you're doing
* **virulence_rates**: Each list represents each age group and the length represents the days of exposure. Usually the virulence rate does not change as the days of exposure increase
* **incubation_rates**: Same as previous but usually the incubation rates increase as time in this state increases
* **mobility_rates**: Same as the prvious but the length represents the days of infection. The proportion of population moving around may change as the time in the infected state carries on
* **recovery_rates**: Same as previous but handles the chance of recoveries
* **fatality_rates**: Same as previous but with the chance of fatalities
* **SIIRS_model**: Can recovered individuals become re-susceptible
* **has_vaccine**: Are vaccines in effect
* **has_exposed_state**: Is there an _exposed_ state, where the proportions of the population may or may not become infected?

neighborhood
* **default_cell_id**: _N/A_
    * **correlation**: _N/A_
    * **infection_correction_factors**: _N/A_
