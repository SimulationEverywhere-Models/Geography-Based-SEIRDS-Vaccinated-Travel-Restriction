# Written by Eric and based on scripts made by Glenn
# This script compiles the model and assumes the environment running this script includes python and python geopandas

# <Colors>  #
    RESET="\033[0m"
    RED="\033[31m"
    GREEN="\033[32m"
    YELLOW="\033[33m"
    BLUE="\033[36m"
    BOLD="\033[1m"
    ITALIC="\033[3m"
# </Colors> #

# <Helpers> #
    GenerateScenario()
    {
        mkdir -p Scripts/Input_Generator/output

        # Generate a scenario json file for model input, save it in the config folder
        echo "Generating Scenario:"
        cd Scripts/Input_Generator
        python3 generateScenario.py $AREA $PROGRESS
        ErrorCheck $? # Check for build errors
        mv output/scenario_${AREA}.json ../../config
        rm -rf output
        cd ../..
    }

    # $1: Gen Per Region?
    # $2: Gen Aggregate?
    # $3: Path to logs folder
    GenerateGraphs()
    {
        echo "Generating graphs and stats:"

        LOG_FOLDER=$3
        GEN_FOLDER=Scripts/Graph_Generator/

        if [[ $3 == "" ]]; then
            rm -rf logs/stats
            mkdir -p logs/stats
            LOG_FOLDER="logs"
        fi

        # Per region
        if [[ $1 == "Y" ]]; then
            python3 ${GEN_FOLDER}graph_per_regions.py $PROGRESS-ld=$LOG_FOLDER
            ErrorCheck $? # Check for build errors
        fi

        # Aggregate
        if [[ $2 == "Y" ]]; then
            python3 ${GEN_FOLDER}graph_aggregates.py $PROGRESS-ld=$LOG_FOLDER
            ErrorCheck $? # Check for build errors
        fi
    }
# </Helpers> #

# Runs the model and saves the results in the GIS_Viewer directory
Main()
{
    # Defining commands used
    SIMULATE="$VALGRIND ./pandemic-geographical_model ../config/scenario_${AREA}.json $DAYS $PROGRESS"
    PARSE_MSG_LOGS="java -jar sim.converter.glenn.jar "input" "output""

    # Defining directory to save results
    # Always creates a new directory instead of replacing a previous one
    if [[ $NAME != "" ]]; then
        VISUALIZATION_DIR="${VISUALIZATION_DIR}${NAME}"
    else
        declare -i RUN_INDEX=1
        while true; do
            # Creates a new run
            if [[ ! -d "${VISUALIZATION_DIR}run${RUN_INDEX}" ]]; then
                VISUALIZATION_DIR="${VISUALIZATION_DIR}run${RUN_INDEX}"
                break;
            fi
            RUN_INDEX=$((RUN_INDEX + 1))
        done
    fi

    # Make directories if they don't exist
    mkdir -p logs
    mkdir -p $VISUALIZATION_DIR

    # Generate scenario
    GenerateScenario

    # Run the model
    cd bin
    echo; echo "Executing model for $DAYS days:"
    $SIMULATE
    ErrorCheck $? # Check for build errors
    cd ..
    echo

    # Generate SEVIRDS graphs
    GenerateGraphs $GRAPH_REGIONS "Y"

    # Copy the message log + scenario to message log parser's input
    # Note this deletes the contents of input/output folders of the message log parser before executing
    echo; echo "Copying simulation results to message log parser:"
    mkdir -p Scripts/Msg_Log_Parser/input
    mkdir -p Scripts/Msg_Log_Parser/output
    cp config/scenario_${AREA}.json Scripts/Msg_Log_Parser/input
    cp logs/pandemic_messages.txt Scripts/Msg_Log_Parser/input

    # Run the message log parser
    echo "Running message log parser:"
    cd Scripts/Msg_Log_Parser
    $PARSE_MSG_LOGS > log 2>&1
    ErrorCheck $? log # Check for build errors
    unzip "output\pandemic_messages.zip" -d output
    cd ../..

    # Copy the converted message logs to GIS Web Viewer Folder
    echo; echo; echo -e "Copying converted files to: ${BLUE}${VISUALIZATION_DIR}${RESET}"
    mv Scripts/Msg_Log_Parser/output/messages.log $VISUALIZATION_DIR
    mv Scripts/Msg_Log_Parser/output/structure.json $VISUALIZATION_DIR
    rm -rf Scripts/Msg_Log_Parser/input
    rm -rf Scripts/Msg_Log_Parser/output
    rm -f Scripts/Msg_Log_Parser/*.zip
    cp GIS_Viewer/${AREA}/${AREA}.geojson $VISUALIZATION_DIR
    cp GIS_Viewer/${AREA}/visualization.json $VISUALIZATION_DIR
    mv logs $VISUALIZATION_DIR

    BUILD_TIME=$SECONDS
    echo -en "${GREEN}Simulation Complete and it took "
    if [[ $BUILD_TIME -ge 3600 ]]; then
        echo -en "$((BUILD_TIME / 3600))h"
        BUILD_TIME=$((BUILD_TIME % 3600))
    fi
    if [[ $BUILD_TIME -ge 60 ]]; then echo -en "$((BUILD_TIME / 60))m"; fi
    if [[ $((BUILD_TIME % 60)) > 0 ]]; then echo -en "$((BUILD_TIME % 60))s"; fi
    echo -e " to complete${RESET}"

    echo -e "View results using the files in ${BOLD}${BLUE}run${RUN_INDEX}${RESET} and this web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
}

# <Helpers>  #
    # Helps clean up past simulation runs
    Clean()
    {
        # Delete all the sims for the selected area if no number specified
        if [[ $RUN == -1 ]]; then
            echo -e "Removing ${YELLOW}all${RESET} runs for ${YELLOW}${AREA}${RED}"
            rm -rfv $VISUALIZATION_DIR
        # Otherwise delete the run that matches the number passed in
        else
            echo -e "Removing ${YELLOW}${RUN}${RESET} for ${YELLOW}${AREA}${RED}"
            rm -rfdv ${VISUALIZATION_DIR}${RUN}
        fi

        echo -e "${BOLD}${GREEN}Done.${RESET}" # Reset the colors
    }

    # Checks and handles build errors
    ErrorCheck()
    {
        # Catch any build errors
        if [[ "$1" -ne 0 ]]; then
            if [[ "$2" == "log" ]]; then cat $2; fi # Print any error messages
            echo -e "${RED}Build Failed${RESET}"
            exit -1 # And exit if any exist
        fi

        if [[ -f "log" ]]; then
            echo -en "$YELLOW"
            cat $2;
            echo -en "$RESET"
            rm -f log;
        fi
    }

    # Displays the help
    Help()
    {
        if [[ $1 == 1 ]]; then
            echo -e "${YELLOW}Flags:${RESET}"
            echo -e " ${YELLOW}--clean|-c|--clean=*|-c=*${RESET} \t Cleans all simulation runs for the selected area if no # is set, \n \t\t\t\t otherwise cleans the specified run using the folder name inputed such as 'clean=run1'"
            echo -e " ${YELLOW}--days=#|-d=#${RESET} \t\t\t Sets the number of days to run a simulation (default=500)"
            echo -e " ${YELLOW}--flags, -f${RESET}\t\t\t Displays all flags"
            echo -e " ${YELLOW}--gen-scenario, -gn${RESET}\t\t Generates a scenario json file (an area flag needs to be set)"
            echo -e " ${YELLOW}--gen-region-graphs=*, -grg=*${RESET}\t Generates graphs per region for previously completed simulation. Folder name set after '=' and area flag needed"
            echo -e " ${YELLOW}--graph-region, -gr${RESET}\t\t Generates graphs per region (default=off)"
            echo -e " ${YELLOW}--help, -h${RESET}\t\t\t Displays the help"
            echo -e " ${YELLOW}--no-progress, -np${RESET}\t\t Turns off the progress bars and loading animations"
            echo -e " ${YELLOW}--Ontario, --ontario, -On, -on${RESET}  Ontario AREA Flag. Runs a simulation for Ontario when used on it's own"
            echo -e " ${YELLOW}--Ottawa, --ottawa, -Ot, -ot${RESET}\t Ottawa AREA Flag. Runs a simulation for Ottawa when used on it's own"
            echo -e " ${YELLOW}--profile, -p${RESET}\t\t\t Builds using the ${ITALIC}pg${RESET} profiler tool, runs the model, then exports the results in a text file"
            echo -e " ${YELLOW}--rebuild, -r${RESET}\t\t\t Rebuilds the model"
            echo -e " ${YELLOW}--valgrind|-v${RESET}\t\t\t Runs using valgrind, a memory error and leak check tool"
            echo -e " ${YELLOW}--Wall|-w${RESET}\t\t\t Displays build warnings"
        else
            echo -e "${BOLD}Usage:${RESET}"
            echo -e " ./run_simulation.sh ${ITALIC}<area flag>${RESET}"
            echo -e " where ${ITALIC}<area flag>${RESET} is either --Ottawa ${BOLD}OR${RESET} --Ontario"
            echo -e " example: ./run_simulation.sh --Ottawa"
            echo -e "Use \033[1;33m--flags${RESET} to see a list of all the flags and their meanings"
        fi
    }
# </Helpers> #

# Displays the help if no flags were set
if [[ $1 == "" ]]; then Help;
else
    CLEAN=N # Default to not clean the sim runs
    WALL="-DWALL=N"
    PROFILE=N
    NAME=""
    DAYS="500"
    GRAPH_REGIONS="N"
    GENERATE="N"

    # Loop through the flags
    while test $# -gt 0; do
        case "$1" in
            --clean*|-c*)
                if [[ $1 == *"="* ]]; then
                    RUN=`echo $1 | sed -e 's/^[^=]*=//g'`; # Get the run to remove
                else RUN="-1"; fi # -1 => Delete all runs
                CLEAN=Y
                shift
            ;;
            --days=*|-d=*)
                if [[ $1 == *"="* ]]; then
                    DAYS=`echo $1 | sed -e 's/^[^=]*=//g'`;
                fi
                shift
            ;;
            --flags|-f)
                Help 1;
                exit 1;
            ;;
            --gen-scenario|-gn)
                GENERATE="S"
                shift
            ;;
            --graph-regions|-gr)
                GRAPH_REGIONS="Y"
                shift
            ;;
            --gen-region-graphs=*|-grg=*)
                if [[ $1 == *"="* ]]; then
                    NAME=`echo $1 | sed -e 's/^[^=]*=//g'`; # Set custom folder name
                fi
                GENERATE="R"
                shift
            ;;
            --help|-h)
                Help;
                exit 1;
            ;;
            --name=*|-n=*)
                if [[ $1 == *"="* ]]; then
                    NAME=`echo $1 | sed -e 's/^[^=]*=//g'`; # Set custom folder name
                fi
                shift
            ;;
            --no-progress|-np)
                PROGRESS="-np"
                shift
            ;;
            --Ontario|--ontario|-On|-on)
                AREA="ontario"
                AREA_FILE="${AREA}_phu"
                shift;
            ;;
            --Ottawa|--ottawa|-Ot|-ot)
                # Set the global variables used in other parts of the script like Main()
                AREA="ottawa"
                AREA_FILE="${AREA}_da"
                shift
            ;;
            --profile|-p)
                PROFILE=Y
                shift
            ;;
            --rebuild|-r)
                # Delete old model and it will be built further down
                rm -f bin/pandemic-geographical_model
                rm -rf CMakeFiles/
                rm -f cmake_install.cmake
                rm -f CMakeCache.txt
                rm -f Makefile
                shift;
            ;;
            --valgrind|-val)
                VALGRIND="valgrind --leak-check=yes -s"
                shift
            ;;
            --Wall|-w)
                WALL="-DWALL=Y"
                shift
            ;;
            *)
                echo -e "${RED}Unknown parameter: ${YELLOW}${1}${RESET}"
                Help;
                exit -1;
            ;;
        esac
    done

    # Check for Cadmium
    if [[ ${cadmium} == "" && ! -d "../cadmium" ]]; then
        echo -e "${RED}Could not find Cadmium. Make sure it's in the parent folder${RESET}"
        exit -1
    fi

    # Compile the model if it does not exist
    if [[ ! -f "bin/pandemic-geographical_model" ]]; then
        echo "Building Model"
        cmake CMakeLists.txt $WALL "-DPROFILER=${PROFILE}" > log 2>&1
        ErrorCheck $? log
        make > log 2>&1
        ErrorCheck $? log # Check for build errors

        echo -e "${GREEN}Build Completed${RESET}"

        if [[ $AREA == "" ]]; then exit 1; fi
    fi

    # If not are is set or is set incorrectly, then exit
    if [[ $AREA == "" ]]; then echo -e "${RED}Please set a valid area flag... ${RESET}Use ${YELLOW}--flags${RESET} to see them"; exit -1; fi

    # Used both in Clean() and Main() so we set it here
    VISUALIZATION_DIR="GIS_Viewer/${AREA}/simulation_runs/"

    if [[ $CLEAN == "Y" ]]; then Clean;
    elif [[ $GENERATE == "S" ]]; then GenerateScenario;
    elif [[ $GENERATE == "R" ]]; then
        VISUALIZATION_DIR="${VISUALIZATION_DIR}${NAME}"
        GenerateGraphs "Y" "N" "$VISUALIZATION_DIR/logs";
    else
        if [[ $NAME != "" && -d ${VISUALIZATION_DIR}${NAME} ]]; then
            echo -e "${RED}'${NAME}' Already exists!${RESET}"
            exit -1
        fi

        if [[ $PROFILE == "Y" ]]; then
            # TODO: Investigate Valgrind Profiling
            cd bin
            valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes --collect-atstart=no ./pandemic-geographical_model ../config/scenario_${AREA}.json $DAYS $PROGRESS
            ErrorCheck $?
            echo -e "Check ${GREEN}bin\callgrind.out${RESET} for profiler results"
        else
            Main;
        fi
    fi
fi