# Written by Eric and based on scripts made by Glenn
# This Assumes the environment running this script includes conda, matplotlib, numpy, and geopandas

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
        echo -e "Generating Scenario (${BLUE}${INPUT_DIR}${RESET})"
        cd Scripts/Input_Generator
        python3 generateScenario.py $INPUT_DIR $PROGRESS
        ErrorCheck $? # Check for build errors
        cd $HOME_DIR
    }

    # $1: Gen Per Region?
    # $2: Gen Aggregate?
    # $3: Path to logs folder
    GenerateGraphs()
    {
        echo "Generating Graphs"

        LOG_FOLDER=$3
        GEN_FOLDER=Scripts/Graph_Generator/

        if [[ $3 == "" ]]; then
            rm -rf logs/stats
            mkdir -p logs/stats
            LOG_FOLDER="logs"
        fi

        # Per region
        if [[ $1 == "Y" ]]; then
            python3 ${GEN_FOLDER}graph_per_regions.py $PROGRESS -ld=$LOG_FOLDER
            ErrorCheck $? # Check for build errors
        fi

        # Aggregate
        if [[ $2 == "Y" ]]; then
            python3 ${GEN_FOLDER}graph_aggregates.py $PROGRESS -ld=$LOG_FOLDER
            ErrorCheck $? # Check for build errors
        fi
    }

    BuildTime()
    {
        BUILD_TIME=$SECONDS
        echo; echo -en "${GREEN}${1} Complete ["
        if [[ $BUILD_TIME -ge 3600 ]]; then
            echo -en "$((BUILD_TIME / 3600))h"
            BUILD_TIME=$((BUILD_TIME % 3600))
        fi
        if [[ $BUILD_TIME -ge 60 ]]; then echo -en "$((BUILD_TIME / 60))m"; fi
        if [[ $((BUILD_TIME % 60)) > 0 ]]; then echo -en "$((BUILD_TIME % 60))s"; fi
        echo -e "]${RESET}"
    }

    # Helps clean up past simulation runs
    Clean()
    {
        # Delete all the sims for the selected area if no number specified
        if [[ $RUN == -1 ]]; then
            echo -e "Removing ${YELLOW}all${RESET} runs for ${YELLOW}${INPUT_DIR}${RED}"
            rm -rfv $VISUALIZATION_DIR
        # Otherwise delete the run that matches the number passed in
        else
            echo -e "Removing ${YELLOW}${RUN}${RESET} for ${YELLOW}${INPUT_DIR}${RED}"
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
            cd $INVOKE_DIR
            exit -1 # And exit if any exist
        fi

        if [[ -f "log" ]]; then
            echo -en "$YELLOW"
            cat $2;
            echo -en "$RESET"
            rm -f log;
        fi
    }

    # Dependency Check
    DependencyCheck()
    {
        echo "Checking Dependencies..."

        # Dependencies exclusive to the python scripts
        if [[ `python --version` == *"Python 3"* ]]; then
            echo -e "Python ${GREEN}[FOUND]${RESET}"
        else
            echo -e "Python ${RED}[NOT FOUND]${RESET}"
            exit -1
        fi

        if [[ `conda --version` == *"conda 4"* ]]; then
            echo -e "Conda ${GREEN}[FOUND]${RESET}"
        else
            echo -e "Conda ${RED}[NOT FOUND]${RESET}"
            exit -1
        fi

        # Python dependencies
        Libs=("numpy" "matplotlib" "geopandas")
        condaList=`conda list`
        for lib in "${Libs[@]}"; do
            if [[ "${condaList}" == *"${lib}"* ]]; then
                echo -e "$lib ${GREEN}[FOUND]${RESET}"
            else
                echo -e "$lib ${RED}[NOT FOUND]${RESET}"
                export PyCheck=0
                exit -1
            fi
        done

        echo -e "${GREEN}Done.\n${RESET}"
    }

    # Displays the help
    Help()
    {
        if [[ $1 == 1 ]]; then
            echo -e "${YELLOW}Flags:${RESET}"
            echo -e " ${YELLOW}--area=*|-a=*${RESET} \t\t\t Sets the area to run a simulation"
            echo -e " ${YELLOW}--clean|-c|--clean=*|-c=*${RESET} \t Cleans all simulation runs for the selected area if no name is set, \n \t\t\t\t otherwise cleans the specified run using the folder name inputed such as 'clean=run1'"
            echo -e " ${YELLOW}--days=#|-d=#${RESET} \t\t\t Sets the number of days to run a simulation (default=500)"
            echo -e " ${YELLOW}--flags, -f${RESET}\t\t\t Displays this message"
            echo -e " ${YELLOW}--gen-region-graphs=*, -grg=*${RESET}\t Generates graphs per region for previously completed simulation. Folder name set after '=' and area flag needed \n \t\t\t\t ex: ./RunSimulation.sh -a=ontario -grg=run1"
            echo -e " ${YELLOW}--graph-region, -gr${RESET}\t\t Generates graphs per region during the whole process (default=off). Essentially this is turned off by default because it really slows down the whole process, \n \t\t\t\t but this flag turns on the generator while the previous one only generates them on an already completed simulation"
            echo -e " ${YELLOW}--help, -h${RESET}\t\t\t Displays the help"
            echo -e " ${YELLOW}--no-progress, -np${RESET}\t\t Turns off the progress bars and loading animations"
        else
            echo -e "${BOLD}Usage:${RESET}"
            echo -e " ./run_simulation.sh ${ITALIC}<area flag>${RESET}"
            echo -e " where ${ITALIC}<area flag>${RESET} is either --Ottawa ${BOLD}OR${RESET} --Ontario"
            echo -e " example: ./run_simulation.sh --Ottawa"
            echo -e "Use \033[1;33m--flags${RESET} to see a list of all the flags and their meanings"
        fi
    }
# </Helpers> #

# Runs the model and saves the results in the Results directory
Main()
{
    # Defining directory to save results.
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
    echo; echo "Executing Model for $DAYS Days"
    ./pandemic-geographical_model ../Scripts/Input_Generator/output/scenario_${INPUT_DIR}.json $DAYS $PROGRESS
    ErrorCheck $? # Check for build errors
    cd $HOME_DIR
    echo

    # Generate SEVIRDS graphs
    GenerateGraphs $GRAPH_REGIONS "Y"

    # Copy the message log + scenario to message log parser's input
    # Note this deletes the contents of input/output folders of the message log parser before executing
    mkdir -p Scripts/Msg_Log_Parser/input
    mkdir -p Scripts/Msg_Log_Parser/output
    cp Scripts/Input_Generator/output/scenario_${INPUT_DIR}.json Scripts/Msg_Log_Parser/input
    cp logs/pandemic_messages.txt Scripts/Msg_Log_Parser/input

    # Run the message log parser
    echo; echo "Prepping GIS Viewer Files"
    cd Scripts/Msg_Log_Parser
    java -jar sim.converter.glenn.jar "input" "output" > log 2>&1
    ErrorCheck $? log # Check for build errors
    unzip "output\pandemic_messages.zip" -d output
    cd $HOME_DIR

    # Copy the converted message logs to GIS Web Viewer Folder
    mv Scripts/Msg_Log_Parser/output/messages.log $VISUALIZATION_DIR
    mv Scripts/Msg_Log_Parser/output/structure.json $VISUALIZATION_DIR
    rm -rf Scripts/Msg_Log_Parser/input
    rm -rf Scripts/Msg_Log_Parser/output
    rm -f Scripts/Msg_Log_Parser/*.zip
    cp cadmium_gis/${AREA}/${AREA}.geojson $VISUALIZATION_DIR
    cp cadmium_gis/${AREA}/visualization.json $VISUALIZATION_DIR
    mv logs $VISUALIZATION_DIR

    BuildTime "Simulation"
    echo -e "View results using the files in ${BOLD}${BLUE}${VISUALIZATION_DIR}${RESET} and this web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
}

if [[ $1 == "" ]]; then Help;
else
    CLEAN="N" # Default to not clean the sim runs
    NAME=""
    DAYS="500"
    GRAPH_REGIONS="N"
    HOME_DIR="`dirname \"$0\"`"
    HOME_DIR="`( cd \"$HOME_DIR\" && pwd )`"
    INVOKE_DIR=$PWD
    INPUT_DIR=""
    cd $HOME_DIR

    # Loop through the flags
    while test $# -gt 0; do
        case "$1" in
            --area=*|-a=*)
                if [[ $1 == *"="* ]]; then
                    INPUT_DIR=`echo $1 | sed -e 's/^[^=]*=//g'`;
                    AREA=`echo $INPUT_DIR | sed -r 's/_.+//g'`;
                fi
                shift
            ;;
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
            --gen-region-graphs=*|-grg=*)
                if [[ $1 == *"="* ]]; then
                    NAME=`echo $1 | sed -e 's/^[^=]*=//g'`; # Set custom folder name
                fi
                GENERATE="R"
                shift
            ;;
            --graph-regions|-gr)
                GRAPH_REGIONS="Y"
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
            *)
                echo -e "${RED}Unknown parameter: ${YELLOW}${1}${RESET}"
                Help;
                exit -1;
            ;;
        esac
    done

    # If not are is set or is set incorrectly, then exit
    if [[ $INPUT_DIR == "" ]]; then echo -e "${RED}Please set a valid area flag... ${RESET}Use ${YELLOW}--flags${RESET} to see them"; exit -1; fi

    # Used both in Clean() and Main() so we set it here
    VISUALIZATION_DIR="Results/${INPUT_DIR}/"

    if [[ $CLEAN == "Y" ]]; then Clean;
    elif [[ $GENERATE == "R" ]]; then
        VISUALIZATION_DIR="${VISUALIZATION_DIR}${NAME}"
        if [[ ! -f "${VISUALIZATION_DIR}/messages.log" ]]; then echo -e "${RED}${BOLD}${NAME}${RESET}${RED} doesn't exist or is invalid${RESET}"; exit -1; fi
        DependencyCheck
        GenerateGraphs "Y" "N" "$VISUALIZATION_DIR/logs";
    else
        if [[ $NAME != "" && -d ${VISUALIZATION_DIR}${NAME} ]]; then
            echo -e "${RED}'${NAME}' Already exists!${RESET}"
            exit -1
        fi

        DependencyCheck
        Main;
    fi

    cd $INVOKE_DIR
fi
