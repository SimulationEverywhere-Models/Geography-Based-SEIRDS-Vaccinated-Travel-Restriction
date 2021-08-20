Understanding the Files
===
default.json - Is the default cell and simulation config, used as an input to the scenario generator

fields.json - Describes the format of the state object written to logs. It is inserted outside of "cells", it ised used by the GIS web viewer v2

infectedCell.json - Describes a cell ID and infected state object where the infection should begin. The scenario json is first generated with each cell having the default.json values, and then the infected cell's state object is overwritten with the values in infectedCell.json. Currently only one cell in this file is supported, and this file must exist for the post processing to work.

Understanding the Fields
===
default.json
---
default
* **delay**: How many days each simulation step represents
* **cell_type**: The type of cell used for simulating regions. Best to leave this as _zhong_ unless you know what you're doing
* **population**: _N/A_
* **age_group_proportions**: Splits the population into 5 age groups from youngest to eldest (left to right)
* **susceptible**: Proportion of each age group that are susceptible to Covid 19
* **vaccinatedD1**: Proportion of each age group (follows the same order as _age_group_proportions_) that have their first dose. Length represents time in days till dose 2
* **vaccinatedD2**: Proportion of each age group that have their second dose (**_Note:_** The population cannot be in D2 and D1 at the same time and cannnot regress). Length represents time in days till fully immunized
* **exposed**: Proportion of each age group (represented by it's own list) that is in the exposed state and the length of the list represents the number of days the population is in the exposed state
* **exposedD1**: Exposed proportions for those who have their first dose
* **exposedD2**: Exposed proportions for those who have their second dose
* **infected**: Same as _exposed_ but this state only lasts 12 days (after that a new state is calculated) and represents the proportion of those infected
* **infectedD1**: Infected proportion for those who have 1 dose
* **infectedD2**: Infected proportion for those who have 2 doses
* **recovered**: Same as the previous two but recovery state lasts a lot longer. Therefore, if re-susceptibility is turned on there's a grace period before some of the population can get re-infected
* **recoveredD1**: Recovered proportions for those with their first dose
* **recoveredD1**: Recovered proportions for those with their second dose
* **fatalities**: Proportion of fatalities for each age group
* **disobedient**: Proportion of population that will disobey Covid 19 guidelines and measures
* **hospital_capacity**: Proportion of population that can be housed in hospitals
* **fatality_modifiers**: Comes into effect when the hospitals are full and increases fatality rates
* **immunityD1**: Immunity received from 1 dose per age group. Length represents weeks (i.e., each week the immunity rises)
* **immunityD2**: Immunity received from 2 doses per age group. Lenght represents weeks
* **min_interval_between_doses**: Minimum time between dose 1 and dose 2. The average interval is longer due to supply shortage but a small proportion of the population are eligible to receive their second shot sooner (ex: elderly, public health workers...) yet there is still a recommended wait time before the first and second dose

config
* **precision**: Used to get cleaner decimal points. Best not to touch this unless you know what you're doing
* **virulence_rates**: Each list represents each age group and the length represents the days of exposure. Usually the virulence rate does not change as the days of exposure increase
* **incubation_rates**: Same as previous but usually the incubation rates increase as time in this state increases
* **incubation_rates_dose1**: Incubation rates for those with their first dose of the vaccine
* **incubation_rates_dose2**: Incubation rates for those with their second dose of the vaccine
* **mobility_rates**: Same as the prvious but the length represents the days of infection. The proportion of population moving around may change as the time in the infected state carries on
* **recovery_rates**: Same as previous but handles the chance of recoveries
* **fatality_rates**: Same as previous but with the chance of fatalities
* **vaccinaterd_rates_dose1** : Rate at which each age group is being vaccinated daily
* **vaccinaterd_rates_dose2** : Rate at which each age group is being vaccinated, varying on a daily basis as some people are eligible to get their second dose sooner. The length of each list represents the length of days set in **vaccinatedD1** minus the **min_interval_between_doses** (the first index in the list is the minimal interval and the last index is the interval of those with dose 1)
* **Re-Susceptibility**: Can recovered individuals become re-susceptible? ```true``` or ```false```
* **Vaccinations**: Toggle for whether the effect of vaccines should be simulated. Set this to ```false``` to simulate a scenario where vaccines don't exist

neighborhood
* **default_cell_id**: _N/A_
    * **correlation**: _N/A_
    * **infection_correction_factors**: _N/A_