# Original author: Glenn - 24/03/2021
# Edited by Eric - Jun-August 2021

#!/usr/bin/env python
# coding: utf-8

import multiprocessing
from multiprocessing.context import Process
from multiprocessing import Pool
import itertools, threading, time, sys, os, re
import pandas as pd
import matplotlib
import shutil
from functools import partial

no_progress = False
log_file_folder = ""

# Handles command line flags
for word in sys.argv:
    lowered = word.lower()

    if lowered == "--no-progress" or lowered == "-np":
        no_progress = True
    elif "-log-dir" in lowered or "-ld" in lowered:
        log_file_folder = word.split("=",1)[1]
#for

if len(log_file_folder) == 0:
    print("\n\033[31mASSERT:\033[m Must set a log folder path using the flag -log-dir=<path to logs folder>\033[0m")
    exit(-1)

# Loading animation
done = False
success = True
def animate():
    # Loop through the animation cycles
    for c in itertools.cycle(['|', '/', '-', '\\']):
        # When the global variable is set at the eof
        #  then the infinite loop will break
        if done:
            break
        # Using stdout makes it easier to deal with the newlines inherent in print()
        # '\r' replaces the previous line so we don't crowd the terminal
        sys.stdout.write('\r\033[33mgenerating graphs ' + c + '\033[0m')
        sys.stdout.flush()
        time.sleep(0.1)
    if success:
        sys.stdout.write('\r\033[1;32mDone.                   \033[0m\n')
#animate()


try:
    def state_to_percent_df(sim_time, region_state, line_num):
        # Read the percentages of each state
        percent_S = region_state[sIndex]
        percent_E = region_state[eIndex]
        percent_VD1 = region_state[vd1Index]
        percent_VD2 = region_state[vd2Index]
        percent_I = region_state[iIndex]
        percent_R = region_state[rIndex]
        percent_D = region_state[dIndex]

        percent_new_E = region_state[neweIndex]
        percent_new_I = region_state[newiIndex]
        percent_new_R = region_state[newrIndex]

        psum = percent_S + percent_E + percent_VD1 + \
            percent_VD2 + percent_I + percent_R + percent_D
        assert 0.995 <= psum < 1.005, ("at time " + str(curr_time))

        # return the info in desired format
        return [int(sim_time), percent_S, percent_E, percent_VD1, percent_VD2, percent_I, percent_R, percent_new_E, percent_new_I, percent_new_R, percent_D]
    #state_to_percent_df

    def state_to_cumulative_df(sim_time, region_state, line_num):
        # Read the percentages of each state
        cell_population = region_state[0]
        percent_S = region_state[sIndex]
        percent_E = region_state[eIndex]
        percent_VD1 = region_state[vd1Index]
        percent_VD2 = region_state[vd2Index]
        percent_I = region_state[iIndex]
        percent_R = region_state[rIndex]
        percent_D = region_state[dIndex]

        percent_new_E = region_state[neweIndex]
        percent_new_I = region_state[newiIndex]
        percent_new_R = region_state[newrIndex]

        # Convert from percentages to cumulative totals
        total_S = round(cell_population*percent_S)
        total_E = round(cell_population*percent_E)
        total_VD1 = round(cell_population*percent_VD1)
        total_VD2 = round(cell_population*percent_VD2)
        total_I = round(cell_population*percent_I)
        total_R = round(cell_population*percent_R)
        total_D = round(cell_population*percent_D)

        total_new_E = round(cell_population*percent_new_E)
        total_new_I = round(cell_population*percent_new_I)
        total_new_R = round(cell_population*percent_new_R)

        psum = percent_S + percent_E + percent_VD1 + \
            percent_VD2 + percent_I + percent_R + percent_D
        assert 0.995 <= psum < 1.005, ("at time " + str(curr_time))

        # Return the info in desired format
        return [int(sim_time), total_S, total_E, total_VD1, total_VD2, total_I, total_R, total_new_E, total_new_I, total_new_R, total_D]
    #state_to_percent_df

    # Generates graph for one region_key
    def generate_graph(path, data_percents, data_totals, region_key):
        import matplotlib.pyplot as plt # Needs this for multi-processing
        COLOR_SUSCEPTIBLE = 'xkcd:blue'
        COLOR_INFECTED    = 'xkcd:red'
        COLOR_EXPOSED     = 'xkcd:sienna'
        COLOR_DOSE1       = '#B91FDE'
        COLOR_DOSE2       = '#680D5A'
        COLOR_RECOVERED   = 'xkcd:green'
        COLOR_DEAD        = 'xkcd:black'

        STYLE_SUSCEPTIBLE = '-'
        STYLE_INFECTED    = '--'
        STYLE_EXPOSED     = '-.'
        STYLE_DOSE1       = 'solid'
        STYLE_DOSE2       = 'dashed'
        STYLE_RECOVERED   = ':'
        STYLE_DEAD        = 'solid'

        font = {'family': 'DejaVu Sans',
                'weight': 'normal',
                'size': 16}

        matplotlib.rc('font', **font)
        matplotlib.rc('lines', linewidth=2)

        columns = ["time", "susceptible", "exposed", "vaccinatedD1", "vaccinatedD2",
                "infected", "recovered", "new_exposed", "new_infected", "new_recovered", "deaths"]

        # Make a folder for the region in that stats folder
        foldername = "region_" + region_key

        try:
            os.mkdir(path + "/" + foldername)
        except OSError as error:
            print('Region ID - ' + region_key + ": ")
            raise error

        percents_filename    = foldername +"_percentage_timeseries.csv"
        totals_filename      = foldername +"_totals_timeseries.csv"
        base_name            = path + "/" + foldername + "/"
        percentages_filepath = base_name + percents_filename
        totals_filepath      = base_name + totals_filename

        # write the timeseries percent file inside stats/region_id
        with open(percentages_filepath, "w") as out_file:
            out_file.write("sim_time, S, E, VD1, VD2, I, R, New_E, New_I, New_R, D\n")
            for timestep in data_percents[region_key]:
                out_file.write(str(timestep).strip("[]")+"\n")

        # write the timeseries cumulative file inside stats/region_id
        with open(totals_filepath, "w") as out_file:
            out_file.write("sim_time, S, E, VD1, VD2, I, R, New_E, New_I, New_R, D\n")
            for timestep in data_totals[region_key]:
                out_file.write(str(timestep).strip("[]")+"\n")

        # initialize graphing dfs (percents)
        df_vis_p = pd.DataFrame(data_percents[region_key], columns=columns)
        df_vis_p = df_vis_p.set_index("time")

        # Determine whether to render vaccines
        vaccines = not (sum(df_vis_p['vaccinatedD1']) == 0 and sum(df_vis_p['vaccinatedD2']) == 0)

        # initialize graphing dfs (totals)
        df_vis_t = pd.DataFrame(data_percents[region_key], columns=columns)
        df_vis_t = df_vis_t.set_index("time")

        x = list(df_vis_p.index)
        t = list(df_vis_t.index)

        ### --- SEIR --- ###
        fig, axs = plt.subplots(2, figsize=(15,6))

        axs[0].plot(x, 100*df_vis_p["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linestyle=STYLE_SUSCEPTIBLE)
        axs[0].plot(x, 100*df_vis_p["exposed"],     label="Exposed",     color=COLOR_EXPOSED,     linestyle=STYLE_EXPOSED)
        axs[0].plot(x, 100*df_vis_p["infected"],    label="Infected",    color=COLOR_INFECTED,    linestyle=STYLE_INFECTED)
        axs[0].plot(x, 100*df_vis_p["recovered"],   label="Recovered",   color=COLOR_RECOVERED,   linestyle=STYLE_RECOVERED)
        axs[0].legend(loc='upper right')
        axs[0].set_ylim(0, 100)
        axs[0].set_title('Epidemic SEIR Percentages and Total for ' + foldername)
        axs[0].set_ylabel("Population (%)")

        axs[1].plot(t, df_vis_t["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linestyle=STYLE_SUSCEPTIBLE)
        axs[1].plot(t, df_vis_t["exposed"],     label="Exposed",     color=COLOR_EXPOSED,     linestyle=STYLE_EXPOSED)
        axs[1].plot(t, df_vis_t["infected"],    label="Infected",    color=COLOR_INFECTED,    linestyle=STYLE_INFECTED)
        axs[1].plot(t, df_vis_t["recovered"],   label="Recovered",   color=COLOR_RECOVERED,   linestyle=STYLE_RECOVERED)
        axs[1].legend(loc='upper right')
        axs[1].set_xlabel("Time (days)")
        axs[1].set_ylabel("Population (#)")

        plt.savefig(base_name + "SEIR.png")
        plt.close(fig)

        ### --- SEIRD --- ###
        fig, axs = plt.subplots(2, figsize=(15,6))

        axs[0].plot(x, 100*df_vis_p["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linestyle=STYLE_SUSCEPTIBLE)
        axs[0].plot(x, 100*df_vis_p["exposed"],     label="Exposed",     color=COLOR_EXPOSED,     linestyle=STYLE_EXPOSED)
        axs[0].plot(x, 100*df_vis_p["infected"],    label="Infected",    color=COLOR_INFECTED,    linestyle=STYLE_INFECTED)
        axs[0].plot(x, 100*df_vis_p["recovered"],   label="Recovered",   color=COLOR_RECOVERED,   linestyle=STYLE_RECOVERED)
        axs[0].plot(x, 100*df_vis_p["deaths"],      label="Deaths",      color=COLOR_DEAD,        linestyle=STYLE_DEAD)
        if vaccines:
            axs[0].plot(x, 100*df_vis_p["vaccinatedD1"], label="Vaccinated 1 dose", color=COLOR_DOSE1, linestyle=STYLE_DOSE1)
            axs[0].plot(x, 100*df_vis_p["vaccinatedD2"], label="Vaccinated 2 dose", color=COLOR_DOSE2, linestyle=STYLE_DOSE2)
            axs[0].set_title('Epidemic SEVIRD Percentages and Totals for ' + foldername)
        else:
            axs[0].set_title('Epidemic SEIRD Percentages and Totals for ' + foldername)
        axs[0].set_ylabel("Population (%)")
        axs[0].set_ylim(0, 100)
        axs[0].legend(loc="upper right")

        axs[1].plot(t, df_vis_t["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linestyle=STYLE_SUSCEPTIBLE)
        axs[1].plot(t, df_vis_t["exposed"],     label="Exposed",     color=COLOR_EXPOSED,     linestyle=STYLE_EXPOSED)
        axs[1].plot(t, df_vis_t["infected"],    label="Infected",    color=COLOR_INFECTED,    linestyle=STYLE_INFECTED)
        axs[1].plot(t, df_vis_t["recovered"],   label="Recovered",   color=COLOR_RECOVERED,   linestyle=STYLE_RECOVERED)
        axs[1].plot(t, df_vis_t["deaths"],      label="Deaths",      color=COLOR_DEAD,        linestyle=STYLE_DEAD)
        if vaccines:
            axs[1].plot(t, df_vis_t["vaccinatedD1"], label="Vaccinated 1 Dose",  color=COLOR_DOSE1, linestyle=STYLE_DOSE1)
            axs[1].plot(t, df_vis_t["vaccinatedD2"], label="Vaccinated 2 Doses", color=COLOR_DOSE2, linestyle=STYLE_DOSE2)
            axs[1].set_ylim(0,150000) # TODO: Remove this once vaccinated modelling is implemented correctly
        axs[1].set_ylabel("Population (#)")
        axs[1].set_xlabel("Time (days)")
        axs[1].legend(loc="upper right")

        if vaccines:
            plt.savefig(base_name + "SEVIRD.png")
        else:
            plt.savefig(base_name + "SEIRD.png")

        plt.close(fig)

        # Deaths + Vaccinated
        fig, axs = plt.subplots(2, figsize=(15,6))

        if vaccines:
            axs[0].plot(x, 100*df_vis_p["vaccinatedD1"], label="Vaccinated 1 Dose", color=COLOR_DOSE1, linestyle=STYLE_DOSE1)
            axs[0].plot(x, 100*df_vis_p["vaccinatedD2"], label="Vaccinated 2 Dose", color=COLOR_DOSE2, linestyle=STYLE_DOSE2)
            axs[0].set_title("Epidemic Death and Vaccinated Percentages for " + foldername)
            axs[0].set_ylabel("Population (%)")
        else:
            axs[0].plot(t, df_vis_t["deaths"], label="Deaths", color=COLOR_DEAD, linestyle=STYLE_DEAD)
            axs[0].set_title("Epidemice Death Totals and Percentages for " + foldername)
            axs[0].set_ylabel("Population (#)")
        axs[0].legend(loc="upper right")

        axs[1].plot(x, 100*df_vis_p["deaths"], label="Deaths", color=COLOR_DEAD, linestyle=STYLE_DEAD)
        axs[1].set_ylabel("Population (%)")
        axs[1].set_xlabel("Time (days)")
        axs[1].legend(loc="upper right")

        if vaccines:
            plt.savefig(base_name + "Deaths+Vaccianted.png")
        else:
            plt.savefig(base_name + "Deaths.png")
        plt.close(fig)
    #generate_graph()

    # Multiprocess each cell to produce their respective threads
    #with
    if __name__ == '__main__':
        # Don't forget to thread it!
        if not no_progress:
            t = threading.Thread(target=animate)
            t.start()

        # Setup paths, filenames, and folders
        log_filename = log_file_folder + "/pandemic_state.txt"
        path = log_file_folder + "/stats/per-region"
        shutil.rmtree(path, ignore_errors=True)

        # Regex str to find underscore and one or more characters after the underscore (model id)
        regex_model_id = "_\w+"
        # Regex str to read all state contents between <>
        regex_state = "<.+>"

        # State log structure
        sIndex = 1
        eIndex = 2
        vd1Index = 3
        vd2Index = 4
        iIndex = 5
        rIndex = 6
        neweIndex = 7
        newiIndex = 8
        newrIndex = 9
        dIndex = 10

        states = ["sus", "expos", "infec", "rec"]
        curr_time = None
        curr_states = {}
        initial_pop = {}
        total_pop = 0
        data_percents = {}
        data_totals = {}

        # Read the initial populations of all regions and their names in time step 0
        with open(log_filename, "r") as log_file:
            line_num = 0

            # For each line, read a line then:
            for line in log_file:
                # Strip leading and trailing spaces
                line = line.strip()

                # If a time marker is found that is not the current time
                if line.isnumeric() and line != curr_time:
                    # Update new simulation time
                    curr_time = line
                    continue

                # Create an re match objects from the current line
                state_match = re.search(regex_state, line)
                id_match = re.search(regex_model_id, line)
                if not (state_match and id_match):
                    continue

                # Parse the state and id and insert into initial_pop
                cid = id_match.group().lstrip('_')
                state = state_match.group().strip("<>")
                state = state.split(",")
                initial_pop[cid] = float(state[0])

                # Initialize data strucutres with region keys
                if not cid in data_percents:
                    data_percents[cid] = list()
                    data_totals[cid] = list()

                state = state_match.group().strip("<>")
                state = list(map(float, state.split(",")))
                curr_states[cid] = state

                state_percentages = state_to_percent_df(
                    curr_time, curr_states[cid], line_num)
                state_totals = state_to_cumulative_df(
                    curr_time, curr_states[cid], line_num)
                data_percents[cid].append(state_percentages)
                data_totals[cid].append(state_totals)

                line_num += 1
            #for
        #with

        try:
            os.mkdir(path)
        except OSError as error:
            raise error

        with multiprocessing.get_context('spawn').Pool() as pool:
            func = partial(generate_graph, path, data_percents, data_totals)
            pool.map(func, data_percents)

        if no_progress:
            print("\033[1;32mDone.\033[0m")
        else:
            done = True
            t.join()
except AssertionError as assertion:
    success = False
    done = True
    if not no_progress:
        t.join()

    print("\n\033[31mASSERT:\033[0m 0.995 <= psum < 1.005", assertion)
    sys.exit(-1)
except KeyboardInterrupt as interrupt:
    success = False
    done = True
    if not no_progress:
        t.join()

    print("\n\033[33mStopped by user\033[0m")
    sys.exit(-1)
except Exception as error:
    success = False
    done = True
    if not no_progress:
        t.join()

    print("\n\033[31mException: " + str(error) + "\033[0m")
    sys.exit(-1)
