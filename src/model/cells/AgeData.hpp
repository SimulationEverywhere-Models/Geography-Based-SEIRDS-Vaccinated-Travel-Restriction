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
    public:
        // Helps identify which type of data
        // the object contains
        enum PopType
        {
            NVAC,
            DOSE1,
            DOSE2
        };
    private:
        // Proportion Vectors for timestep t+1
        // These will be at a current age segment index so only one vector of doubles
        vecDouble& m_susceptible;
        vecDouble& m_exposed;
        vecDouble& m_infected;
        vecDouble& m_recovered;

        // Reduces the amount of math that is done twice.
        // The values will be added in these when first done
        // then accessed later by other equations
        vecDouble m_newFatalities;
        vecDouble m_newRecoveries;
        vecDouble m_newVacFromRec;
        vecDouble m_newExposed;

        // Keeps track of the totals for the current
        // day in the simulation which saves time having
        // to compute the totals at the end of each loop
        // in local compute
        double m_totalSusceptible;
        double m_totalExposed;
        double m_totalInfected;
        double m_totalFatalities;
        double m_totalRecoveries;

        // Proportion Vectors for timestep t
        /* Since the original vectors are being changed by the equations 
        *   and later equations will need their values BEFORE they are 
        *   changed let's copy them before they are change in seperate vectors.
        *   This way we don't overcount and we don't have to do the math twice in
        *   certain cases (ex: any equation that needs F(q) can just reference this
        *   list instead of calculating it again).
        */
        vecDouble m_OriginalSusceptible;
        vecDouble m_OriginalExposed;
        vecDouble m_OriginalInfected;
        vecDouble m_OriginalRecovered;

        // Config Vectors
        vecDouble const& m_incubRates;
        vecDouble const& m_recovRates;
        vecDouble const& m_fatalRates;
        vecDouble const& m_vacRates;
        vecDouble const& m_immuneRates;

        // Phase Lengths
        unsigned int m_susceptiblePhase;
        unsigned int m_exposedPhase;
        unsigned int m_infectedPhase;
        unsigned int m_recoveredPhase;

        PopType m_popType;

    public:
        AgeData(unsigned int age, vecVecDouble& susc, vecVecDouble& exp, vecVecDouble& inf,
                vecVecDouble& rec, vecVecDouble const& incub_r, vecVecDouble const& rec_r,
                vecVecDouble const& fat_r, vecDouble const& vac_r, vecDouble const& immu_r, PopType type=PopType::NVAC) :
            m_susceptible(susc.at(age)),
            m_exposed(exp.at(age)),
            m_infected(inf.at(age)),
            m_recovered(rec.at(age)),
            m_newFatalities(inf.at(age).size(), 0.0),
            m_newRecoveries(inf.at(age).size(), 0.0),
            m_newVacFromRec(rec.at(age).size(), 0.0),
            m_newExposed(susc.at(age).size(), 0.0),
            m_totalSusceptible(0.0),
            m_totalExposed(0.0),
            m_totalInfected(0.0),
            m_totalFatalities(0.0),
            m_totalRecoveries(0.0),
            m_OriginalSusceptible(susc.at(age)),
            m_OriginalExposed(exp.at(age)),
            m_OriginalInfected(inf.at(age)),
            m_OriginalRecovered(rec.at(age)),
            m_incubRates(incub_r.at(age)),
            m_recovRates(rec_r.at(age)),
            m_fatalRates(fat_r.at(age)),
            m_vacRates(vac_r),     // Don't .at() this one since it may be EMPTY_VEC
            m_immuneRates(immu_r), // This one too may be EMPTY_VEC
            m_popType(type)
        {
            // -1 so for loops are easier
            m_susceptiblePhase = m_susceptible.size() - 1;
            m_exposedPhase     = m_exposed.size()     - 1;
            m_infectedPhase    = m_infected.size()    - 1;
            m_recoveredPhase   = m_recovered.size()   - 1;

            m_OriginalExposed.reserve(m_exposedPhase + 1);
            m_OriginalInfected.reserve(m_infectedPhase + 1);
            m_OriginalRecovered.reserve(m_recoveredPhase + 1);
            m_newFatalities.reserve(m_infectedPhase + 1);
        }

        // Non-Vaccinated
        //  No vaccination or immunity rates
        AgeData(unsigned int age, vecVecDouble& susc, vecVecDouble& exp, vecVecDouble& inf,
            vecVecDouble& rec, vecVecDouble const& incub_r, vecVecDouble const& rec_r, vecVecDouble const& fat_r) :
            AgeData(age, susc, exp, inf, rec, incub_r, rec_r, fat_r, EMPTY_VEC, EMPTY_VEC)
        { }

        // GETTERS
        double GetSusceptibleBack()     { return m_susceptible.back();         }
        double GetRecoveredBack()       { return m_recovered.back();           }
        double GetNewFatalitiesBack()   { return m_newFatalities.back();       }
        double GetNewRecoveredBack()    { return m_newRecoveries.back();       }
        double GetOrigSusceptibleBack() { return m_OriginalSusceptible.back(); }
        double GetOrigInfectedBack()    { return m_OriginalInfected.back();    }
        double GetOrigRecoveredBack()   { return m_OriginalRecovered.back();   }

        double GetTotalSusceptible() { return m_totalSusceptible; }
        double GetTotalExposed()     { return m_totalExposed;     }
        double GetTotalInfected()    { return m_totalInfected;    }
        double GetTotalRecovered()   { return m_totalRecoveries;  }
        double GetTotalFatalities()  { return m_totalFatalities;  }

        double GetNewFatalities(int index)   { return m_newFatalities.at(index);       }
        double GetNewRecovered(int index)    { return m_newRecoveries.at(index);       }
        double GetVacFromRec(int index)      { return m_newVacFromRec.at(index);       }
        double GetNewExposed(int index)      { return m_newExposed.at(index);          }

        double GetOrigSusceptible(int index) { return m_OriginalSusceptible.at(index); }
        double GetOrigExposed(int index)     { return m_OriginalExposed.at(index);     }
        double GetOrigInfected(int index)    { return m_OriginalInfected.at(index);    }
        double GetOrigRecovered(int index)   { return m_OriginalRecovered.at(index);   }

        double GetIncubationRate(int index)  { return m_incubRates.at(index);    }
        double GetRecoveryRate(int index)    { return m_recovRates.at(index);    }
        double GetFatalityRate(int index)    { return m_fatalRates.at(index);    }
        double GetVaccinationRate(int index) { return m_vacRates.at(index);      }
        double GetImmunityRate(int index)    { return m_immuneRates.at(index);   }

        unsigned int GetSusceptiblePhase() { return m_susceptiblePhase; }
        unsigned int GetExposedPhase()     { return m_exposedPhase;     }
        unsigned int GetInfectedPhase()    { return m_infectedPhase;    }
        unsigned int GetRecoveredPhase()   { return m_recoveredPhase;   }

        PopType& GetType() { return m_popType; }

        // SETTERS
        void SetNewRecovered(unsigned int q, double value)  { m_newRecoveries.at(q) = value;  }
        void SetVacFromRec(unsigned int q, double value)    { m_newVacFromRec.at(q) = value;  }
        void SetNewFatalities(unsigned int q, double value) { m_newFatalities.at(q) = value;  }
        void SetNewExposed(unsigned int q, double value)    { m_newExposed.at(q)    = value;  }
        void SetTotalFatalities(double fatals)              { m_totalFatalities     = fatals; }

        /**
         * @brief Sets the value on the specified day
         * and increments the total
         * 
         * @param q Index
         * @param value New value to set on day q
        */
        void SetSusceptible(unsigned int q, double value)
        {
            m_susceptible.at(q) = value;
            m_totalSusceptible += value;
        }

        /**
         * @brief Sets the value on the specified day
         * and increments the total
         * 
         * @param q Index
         * @param value New value to set on day q
        */
        void SetExposed(unsigned int q, double value)
        {
            m_exposed.at(q) = value;
            m_totalExposed += value;
        }

        /**
         * @brief Sets the value on the specified day
         * and increments the total
         * 
         * @param q Index
         * @param value New value to set on day q
        */
        void SetInfected(unsigned int q, double value)
        {
            m_infected.at(q) = value;
            m_totalInfected += value;
        }

        /**
         * @brief Sets the value on the specified day
         * and increments the total
         * 
         * @param q Index
         * @param value New value to set on day q
        */
        void SetRecovered(unsigned int q, double value)
        {
            m_recovered.at(q)  = value;
            m_totalRecoveries += value;
        }
};

#endif // AGE_DATA_HPP