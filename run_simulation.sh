# Written by Eric and based on scripts made by Glenn
# This script compiles the model (if it is not already) and assumes the environment running this script includes python and python geopandas

# Runs the model and saves the results in the GIS_Viewer directory
Main()
{
    # Defining commands used
    SIMULATE="./pandemic-geographical_model ../config/scenario_${AREA_FILE}.json 500"
    PARSE_MSG_LOGS="java -jar sim.converter.glenn.jar "input" "output""

    # Defining directory to save results
    # Always creates a new directory instead of replacing a previous one
    declare -i index=1
    while true; do
        # Creates a new run
        if [[ ! -d "${VISUALIZATION_DIR}/run${index}" ]]; then
            VISUALIZATION_DIR="${VISUALIZATION_DIR}run${index}"
            break;
        fi
        index=$(( index + 1 ))
    done

    # Make directories if they don't exist
    mkdir -p Scripts/Input_Generator/output
    mkdir -p Scripts/Msg_Log_Parser/input
    mkdir -p Scripts/Msg_Log_Parser/output
    mkdir -p ${VISUALIZATION_DIR}

    # Generate a scenario json file for model input, save it in the config folder
    echo "Generating Scenario:"
    cd Scripts/Input_Generator
    python3 generate_${AREA_FILE}_json.py
    cp output/scenario_${AREA_FILE}.json ../../config
    cd ../..

    # Run the model
    cd bin
    echo
    echo "Executing model:"
    echo ${SIMULATE}
    ${SIMULATE}

    # Generate SIRDS graphs
    echo
    echo "Generating graphs and stats (will be found in logs folder):"
    cd ../Scripts/Graph_Generator/
    python3 graph_per_regions.py
    python3 graph_aggregates.py
    cd ../..

    # Copy the message log + scenario to message log parser's input
    # Note this deletes the contents of input/output folders of the message log parser before executing
    echo
    echo "Copying simulation results to message log parser:"
    rm -f Scripts/Msg_Log_Parser/input/*
    rm -rf Scripts/Msg_Log_Parser/output/pandemic_messages
    rm -f Scripts/Msg_Log_Parser/*.zip
    rm -f Scripts/Msg_Log_Parser/output/*
    cp config/scenario_${AREA_FILE}.json Scripts/Msg_Log_Parser/input
    cp logs/pandemic_messages.txt Scripts/Msg_Log_Parser/input

    # Run the message log parser
    echo "Running message log parser:"
    cd Scripts/Msg_Log_Parser
    echo ${PARSE_MSG_LOGS}
    ${PARSE_MSG_LOGS}
    echo
    unzip "output\pandemic_messages.zip" -d output
    cd ../..

    # Copy the converted message logs to GIS Web Viewer Folder
    echo
    echo "Copying converted files to: ${VISUALIZATION_DIR}"
    cp Scripts/Msg_Log_Parser/output/messages.log ${VISUALIZATION_DIR}
    cp Scripts/Msg_Log_Parser/output/structure.json ${VISUALIZATION_DIR}
    if [[ $AREA == "ottawa" ]]; then
        cp GIS_Viewer/${AREA}/ottawaDA.geojson ${VISUALIZATION_DIR}
    else
        cp GIS_Viewer/${AREA}/${AREA_FILE}.geojson ${VISUALIZATION_DIR}; fi
    cp GIS_Viewer/${AREA}/visualization.json ${VISUALIZATION_DIR}

    echo -e "View results using the files in \033[1;32mrun1\033[0m and this web viewer: \033[1;36mhttp://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html\033[0m"
}

# Helps clean up past simulation runs
Clean()
{
    # Delete all the sims for the selected area if no number specified
    if [[ $RUN == -1 ]]; then
        echo -e "Removing \033[33mall\033[0m runs for \033[33m${AREA}\033[31m"
        rm -rfv ${VISUALIZATION_DIR}
    # Otherwise delete the run that matches the number passed in
    else
        echo -e "Removing \033[33mrun${RUN}\033[0m for \033[33m${AREA}\033[31m"
        rm -rfdv ${VISUALIZATION_DIR}run${RUN}
    fi

    echo -en "\033[0m" # Reset the colors
}

# Displays the help
Help()
{
    echo -e "\033[1mUsage:\033[0m"
    echo -e " ./run_simulation.sh \033[3m<Area>\033[0m"
    echo -e " where \033[3m<Area>\033[0m is either Ottawa \033[1mOR\033[0m Ontario"
    echo -e " example: ./run_simulation.sh Ottawa"
    echo -e "Use \033[1;33m--flags\033[0m to see a list of all the flags and their meanings"
}

# Display the flags
Flags()
{
    echo -e "\033[1mFlags:\033[0m"
    echo -e " \033[33m--flags, -f\033[0m \t\t\t Displays all flags"
    echo -e " \033[33m--Help, -h\033[0m \t\t\t Displays the help"
    echo -e " \033[33m--Ottawa, --ottawa, -Ot, -ot\033[0m \t Runs a simulation in Ottawa"
    echo -e " \033[33m--Ontario, --ontario, -On, -on\033[0m  Runs a simulation in Ontario"
    echo -e "\033[33m --rebuild, -r\033[0m \t\t\t Rebuilds the model"
}

# Displays the help if no flags were set
if [[ $1 == "" ]]; then Help;
else
    CLEAN=N # Default to not clean the sim runs

    # Loop through the flags
    while test $# -gt 0; do
        case "$1" in
            --help|-h)
                Help;
                exit 1;
            ;;
            --flags|-f)
                Flags;
                exit 1;
            ;;
            --clean*|-c*)
                if [[ $1 == *"="* ]]; then
                    RUN=`echo $1 | sed -e 's/^[^=]*=//g'`; # Get the run to remove
                else RUN="-1"; fi # -1 => Delete all runs
                CLEAN=Y
                shift
            ;;
            --Ottawa|--ottawa|-Ot|-ot)
                # Set the global variables used in other parts of the script like Main()
                AREA="ottawa"
                AREA_FILE="${AREA}_da"
                shift
            ;;
            --Ontario|--ontario|-On|-on)
                AREA="ontario"
                AREA_FILE="${AREA}_phu"
                shift;
            ;;
            --rebuild|-r)
                # Delete old model and it will be built further down
                rm -f bin/pandemic-geographical_model
                shift;
            ;;
            *)
                echo -e "\033[31mUnknown parameter: \033[33m${1}\033[0m"
                Help;
                exit -1;
            ;;
        esac
    done

    # If not are is set or is set incorrectly, then exit
    if [[ ${AREA} == "" || ${AREA_FILE} == "" ]]; then echo -e "\033[31mPlease set a valid area flag..\033[0m Use \033[33m--flags\033[0m to see them"; exit -1; fi

    # Compile the model if it does not exist
    if [[ ! -f "bin/pandemic-geographical_model" ]]; then
        cmake CMakeLists.txt
        make > log 2>&1

        # Catch any build errors
        if [ "$?" -ne 0 ]; then
            cat log
            echo -e "\033[31mBuild Failed\033[0m"
            exit -1 # And exit if any exist
        fi

        echo -e "\033[32mBuild Completed\033[0m"
    fi

    # Used both in Clean() and Main() so we set it here
    VISUALIZATION_DIR="GIS_Viewer/${AREA}/simulation_runs/"

    if [[ $CLEAN == "Y" ]]; then Clean;
    else Main; fi
fi