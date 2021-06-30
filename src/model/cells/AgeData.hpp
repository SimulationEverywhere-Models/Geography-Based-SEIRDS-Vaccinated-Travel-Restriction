// Created by Eric - Jun/2021

#ifndef AGE_DATA_HPP
#define AGE_DATA_HPP

#include <vector>
#include "sevirds.hpp"

using namespace std;
using vecDouble = vector<double>;
using vecVecDouble = vector<vecDouble>;

static vecDouble EMPTY_VEC; // Used as a null

/**
 * Wrapper class that holds important simulation data
 * at each age segment index during local_compute()
*/
class AgeData
{
    // The current age the data is referencing
    unsigned int m_currAge;

    // Proportion Vectors
    // These will be at a current age segment index so only one vector of doubles
    vecDouble& m_susceptible;
    vecDouble& m_exposed;
    vecDouble& m_infected;
    vecDouble& m_recovered;
    double&    m_fatalities;

    // Config Vectors
    vecDouble const& m_incubRates;
    vecDouble const& m_recovRates;
    vecDouble const& m_fatalRates;
    vecDouble const& m_mobilityRates;
    vecDouble const& m_vacRates;
    vecDouble const& m_virulRates;
    vecDouble const& m_immuneRates;

    public:
        AgeData(unsigned int age, vecVecDouble& susc, vecVecDouble& exp, vecVecDouble& inf,
                vecVecDouble& rec, vecDouble& fat, vecVecDouble const& incub_r, vecVecDouble const& rec_r,
                vecVecDouble const& fat_r, vecDouble const& vac_r, vecVecDouble const& mob_r,
                vecVecDouble const& vir_r, vecDouble const& immu_r) :
            m_currAge(age),
            m_susceptible(susc.at(age)),
            m_exposed(exp.at(age)),
            m_infected(inf.at(age)),
            m_recovered(rec.at(age)),
            m_fatalities(fat.at(age)),
            m_incubRates(incub_r.at(age)),
            m_recovRates(rec_r.at(age)),
            m_fatalRates(fat_r.at(age)),
            m_mobilityRates(mob_r.at(age)),
            m_vacRates(vac_r),      // Don't .at() this one since it may be EMPTY_VEC
            m_virulRates(vir_r.at(age)),
            m_immuneRates(immu_r)   // This one too may be EMPTY_VEC
        { }

        // Non-Vaccinated
        //  so no vaccination rate or immunity rate
        AgeData(unsigned int age, vecVecDouble& susc, vecVecDouble& exp, vecVecDouble& inf,
            vecVecDouble& rec, vecDouble& fat, vecVecDouble const& incub_r, vecVecDouble const& rec_r,
            vecVecDouble const& fat_r, vecVecDouble const& mob_r, vecVecDouble const& vir_r) :
            AgeData(age, susc, exp, inf, rec, fat, incub_r, rec_r, fat_r, EMPTY_VEC, mob_r, vir_r, EMPTY_VEC)
        { }

        vecDouble&  GetSusceptible()    { return m_susceptible; }
        vecDouble&  GetExposed()        { return m_exposed;     }
        vecDouble&  GetInfected()       { return m_infected;    }
        vecDouble&  GetRecovered()      { return m_recovered;   }
        double&     GetFatalities()     { return m_fatalities;  }

        vecDouble const& GetIncubationRates()   { return m_incubRates;      }
        vecDouble const& GetRecoveryRates()     { return m_recovRates;      }
        vecDouble const& GetFatalityRates()     { return m_fatalRates;      }
        vecDouble const& GetMobilityRates()     { return m_mobilityRates;   }
        vecDouble const& GetVaccinationRates()  { return m_vacRates;        }
        vecDouble const& GetVirulenceRates()    { return m_virulRates;      }
        vecDouble const& GetImmunityRates()     { return m_immuneRates;     }
};

#endif // AGE_DATA_HPP