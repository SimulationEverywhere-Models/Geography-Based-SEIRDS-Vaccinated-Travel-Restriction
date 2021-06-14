# Original author: Glenn - 24/03/2021
# Edited by Eric - 14/Jun/2021

#!/usr/bin/env python
# coding: utf-8

# In[1]:


import os
import re
import itertools, threading, time, sys

# Loading animation
import itertools, threading, time, sys
done = False
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
    sys.stdout.write('\r\033[32mDone.                   \033[0m\n')

# Don't forget to thread it!
t = threading.Thread(target=animate)
t.start()

# In[2]:

log_file_folder = "../../logs"
log_filename = log_file_folder + "/pandemic_state.txt"

# regex str to find underscore and one or more characters after the underscore (model id)
regex_model_id = "_\w+"
# regex str to read all state contents between <>
regex_state = "<.+>"

# state log structure
sIndex      = 1
eIndex      = 2
vd1Index    = 3
vd2Index    = 4
iIndex      = 5
rIndex      = 6
neweIndex   = 7
newiIndex   = 8
newrIndex   = 9
dIndex      = 10

# In[4]:

def state_to_percent_df(sim_time, region_state, line_num):

    # read the percentages of each state
    percent_S   = region_state[sIndex]
    percent_E   = region_state[eIndex]
    percent_VD1 = region_state[vd1Index]
    percent_VD2 = region_state[vd2Index]
    percent_I   = region_state[iIndex]
    percent_R   = region_state[rIndex]
    percent_D   = region_state[dIndex]

    percent_new_E = region_state[neweIndex]
    percent_new_I = region_state[newiIndex]
    percent_new_R = region_state[newrIndex]

    psum = percent_S + percent_E + percent_I + percent_R + percent_D
    assert 0.95 <= psum < 1.05, ("at time" + str(curr_time))
    
    # return the info in desired format
    return [int(sim_time), percent_S, percent_E, percent_VD1, percent_VD2, percent_I, percent_R, percent_new_E, percent_new_I, percent_new_R, percent_D]

# In[5]:

def state_to_cumulative_df(sim_time, region_state, line_num):
    
    # read the percentages of each state
    cell_population = region_state[0]
    percent_S   = region_state[sIndex]
    percent_E   = region_state[eIndex]
    percent_VD1 = region_state[vd1Index]
    percent_VD2 = region_state[vd2Index]
    percent_I   = region_state[iIndex]
    percent_R   = region_state[rIndex]
    percent_D   = region_state[dIndex]

    percent_new_E = region_state[neweIndex]
    percent_new_I = region_state[newiIndex]
    percent_new_R = region_state[newrIndex]
    
    # convert from percentages to cumulative totals
    total_S     = round(cell_population*percent_S)
    total_E     = round(cell_population*percent_E)
    total_VD1   = round(cell_population*percent_VD1)
    total_VD2   = round(cell_population*percent_VD2)
    total_I     = round(cell_population*percent_I)
    total_R     = round(cell_population*percent_R)
    total_D     = round(cell_population*percent_D)

    total_new_E = round(cell_population*percent_new_E)
    total_new_I = round(cell_population*percent_new_I)
    total_new_R = round(cell_population*percent_new_R)
    
    psum = percent_S + percent_E + percent_I + percent_R + percent_D
    ptotal = total_S + total_E + total_I + total_R + total_D
    assert 0.95 <= psum < 1.05, ("at time" + str(curr_time))
    
    # return the info in desired format
    return [int(sim_time), total_S, total_E, total_VD1, total_VD2, total_I, total_R, total_new_E, total_new_I, total_new_R, total_D]

# In[5]:


states = ["sus", "expos", "infec", "rec"]
curr_time = None
curr_states = {}
initial_pop = {}
total_pop = 0

# read the initial populations of all regions and their names in time step 0
with open(log_filename, "r") as log_file:
    line_num = 0
    # for each line, read a line then:
    for line in log_file:
        
        # strip leading and trailing spaces
        line = line.strip()
        
        # if a time marker is found that is not the current time
        if line.isnumeric() and line != curr_time:
            
            # if time step 1 is found, then break
            if curr_time == "1":
                break
            # update new simulation time
            curr_time = line
            continue

        # create an re match objects from the current line
        state_match = re.search(regex_state,line)
        id_match = re.search(regex_model_id,line)
        if not (state_match and id_match):
            #print(line)
            continue
            
        # parse the state and id and insert into initial_pop
        cid = id_match.group().lstrip('_')
        state = state_match.group().strip("<>")
        state = state.split(",")
        initial_pop[cid] = float(state[0])
        line_num += 1
        
# In[]

data_percents = {}
data_totals = {}

# initialize data strucutres with region keys
for region_id in initial_pop:
    data_percents[region_id] = list()
    data_totals[region_id] = list()

# In[]

with open(log_filename, "r") as log_file:
    line_num = 0
    
    # for each line, read a line then:
    for line in log_file:
        
        # strip leading and trailing spaces
        line = line.strip()
        
        # if a time marker is found that is not the current time
        if line.isnumeric() and line != curr_time:
            
            # if state is ready to write then write it to data of each region before starting on the new time step
            if curr_states:
                for region_id in curr_states:
                    state_percentages = state_to_percent_df(curr_time, curr_states[region_id], line_num)
                    state_totals = state_to_cumulative_df(curr_time, curr_states[region_id], line_num)
                    data_percents[region_id].append(state_percentages)
                    data_totals[region_id].append(state_totals)
            # update new simulation time
            curr_time = line
            continue

        # create an re match objects from the current line
        state_match = re.search(regex_state,line)
        id_match = re.search(regex_model_id,line)
        if not (state_match and id_match):
            #print(line)
            continue
            
        # parse the state and id and insert into curr_states
        cid = id_match.group().lstrip('_')
        state = state_match.group().strip("<>")
        state = list(map(float,state.split(",")))
        curr_states[cid] = state
        line_num += 1
    
    # append final timestep   
    for region_id in curr_states:
        state_percentages = state_to_percent_df(curr_time, curr_states[region_id], line_num)
        state_totals = state_to_cumulative_df(curr_time, curr_states[region_id], line_num)
        data_percents[region_id].append(state_percentages)
        data_totals[region_id].append(state_totals)
    
# In[7]:

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib
import shutil

# In[]:

COLOR_SUSCEPTIBLE   = 'xkcd:blue'
COLOR_INFECTED      = 'xkcd:red'
COLOR_EXPOSED       = 'xkcd:sienna'
COLOR_DOSE1         = '#42b395' # xkcd:greenyblue
COLOR_DOSE2         = '#0b8b87' # xkcd:greenishblue
COLOR_RECOVERED     = 'xkcd:green'
COLOR_DEAD          = 'xkcd:black'

font = {'family' : 'DejaVu Sans',
        'weight' : 'normal',
        'size'   : 16}

matplotlib.rc('font', **font)
columns = ["time", "susceptible", "exposed", "vaccinatedD1", "vaccinatedD2", "infected", "recovered", "new_exposed", "new_infected", "new_recovered", "deaths"]

# In[6]: # create an empty stats folder
    
path = log_file_folder + "/stats"
shutil.rmtree(path, ignore_errors=True)
try: 
    os.mkdir(path) 
except OSError as error: 
    print(error)
    done = True
# In[6]:
# for every region, make a timeseries and serid graphs in the stats folder from percent data
for region_key in data_percents:

    # make a folder for the region in that stats folder
    region_data = data_percents[region_key]
    region_totals = data_totals[region_key]
    foldername = "region_" + region_key
    try:
        os.mkdir(path + "/" + foldername)
    except OSError as error:
        print(error)
        done = True

    percents_filename = foldername +"_percentage_timeseries.csv"
    totals_filename = foldername +"_totals_timeseries.csv"
    base_name = path + "/" + foldername + "/"
    percentages_filepath = base_name + percents_filename
    totals_filepath = base_name + totals_filename

    cell_population = initial_pop[region_key]

    # write the timeseries percent file inside stats/region_id
    with open(percentages_filepath, "w") as out_file:
        #out_file.write("initial cell population : " + str(cell_population) + ",\n")
        out_file.write("sim_time, S, E, VD1, VD2, I, R, New_E, New_I, New_R, D\n")
        for timestep in region_data:
            out_file.write(str(timestep).strip("[]")+"\n")

    # write the timeseries cumulative file inside stats/region_id
    with open(totals_filepath, "w") as out_file:
        #out_file.write("initial cell population : " + str(cell_population) + ",\n")
        out_file.write("sim_time, S, E, VD1, VD2, I, R, New_E, New_I, New_R, D\n")
        for timestep in region_totals:
            out_file.write(str(timestep).strip("[]")+"\n")

    # initialize graphing dfs (percents)
    df_vis_p = pd.DataFrame(region_data, columns=columns)
    df_vis_p = df_vis_p.set_index("time")

    ### --- SEIR --- ###
    fig, ax = plt.subplots(figsize=(15,6))
    linewidth = 2

    x = list(df_vis_p.index)
    ax.plot(x, 100*df_vis_p["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
    ax.plot(x, 100*df_vis_p["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
    ax.plot(x, 100*df_vis_p["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
    ax.plot(x, 100*df_vis_p["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
    plt.legend(loc='upper right')
    plt.margins(0,0)
    plt.title('Epidemic SEIR Percentages for ' + foldername)
    plt.xlabel("Time (days)")
    plt.ylabel("Population (%)")
    plt.savefig(base_name + "percentages_SEIR.png")
    plt.close(fig)

    ### --- SEIRD --- ###
    fig, axs = plt.subplots(2, figsize=(15,6))
    linewidth = 2

    x = list(df_vis_p.index)
    axs[0].plot(x, 100*df_vis_p["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
    axs[0].set_ylabel("Population (%)")
    axs[0].legend()
    axs[0].margins(0,0)
    axs[0].set_title('Epidemic SEIRD Percentages for ' + foldername)

    axs[1].plot(x, 100*df_vis_p["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
    axs[1].set_xlabel("Time (days)")
    axs[1].set_ylabel("Population (%)")
    axs[1].set_ylim([0,6])
    axs[1].legend()
    axs[1].margins(0,0)

    plt.savefig(base_name + "percentages_SEIR+D.png")
    plt.close(fig)

    ### --- SEVIRD --- ###
    fig, axs = plt.subplots(2, figsize=(15,6))
    linewidth = 2

    x = list(df_vis_p.index)
    axs[0].plot(x, 100*df_vis_p["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["vaccinatedD1"], label="Vaccinated 1 dose", color=COLOR_DOSE1, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["vaccinatedD2"], label="Vaccinated 2 dose", color=COLOR_DOSE2, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
    axs[0].plot(x, 100*df_vis_p["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
    axs[0].set_ylabel("Population (%)")
    axs[0].legend()
    axs[0].margins(0,0)
    axs[0].set_title('Epidemic SEVIRD Percentages for ' + foldername)
    axs[1].plot(x, 100*df_vis_p["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
    axs[1].set_xlabel("Time (days)")
    axs[1].set_ylabel("Population (%)")
    axs[1].set_ylim([0,6])
    axs[1].legend()
    axs[1].margins(0,0)

    plt.savefig(base_name + "percentages_SEVIRD.png")
    plt.close(fig)

    # initialize graphing dfs (totals)
    df_vis_t = pd.DataFrame(region_totals, columns=columns)
    df_vis_t = df_vis_t.set_index("time")

    ### --- SEIR --- ###
    fig, ax = plt.subplots(figsize=(15,6))
    linewidth = 2

    x = list(df_vis_t.index)
    ax.plot(x, df_vis_t["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
    ax.plot(x, df_vis_t["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
    ax.plot(x, df_vis_t["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
    ax.plot(x, df_vis_t["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
    plt.legend(loc='upper right')
    plt.margins(0,0)
    plt.title('Epidemic SEIR Totals for ' + foldername)
    plt.xlabel("Time (days)")
    plt.ylabel("# of People")
    plt.savefig(base_name + "totals_SEIR.png")
    plt.close(fig)

    ### --- SEIRD --- ###
    fig, axs = plt.subplots(2, figsize=(15,6))
    linewidth = 2

    x = list(df_vis_t.index)
    axs[0].plot(x, df_vis_t["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
    axs[0].set_ylabel("Population (%)")
    axs[0].legend()
    #axs[0].margins(0,0)
    axs[0].set_title('Epidemic SEIRD Totals for ' + foldername)

    axs[1].plot(x, df_vis_t["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
    axs[1].set_xlabel("Time (days)")
    axs[1].set_ylabel("# of People")
    axs[1].legend()
    #axs[1].margins(0,0)

    plt.savefig(base_name + "totals_SEIR+D.png")
    plt.close(fig)

    ### --- SEVIRD --- ###
    fig, axs = plt.subplots(2, figsize=(15,6))
    linewidth = 2

    x = list(df_vis_t.index)
    axs[0].plot(x, df_vis_t["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["vaccinatedD1"], label="Vaccinated 1 dose", color=COLOR_DOSE1, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["vaccinatedD2"], label="Vaccinated 2 dose", color=COLOR_DOSE2, linewidth=linewidth)
    axs[0].plot(x, df_vis_t["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
    axs[0].set_ylabel("Population (%)")
    axs[0].legend()
    #axs[0].margins(0,0)
    axs[0].set_title('Epidemic SEVIRD Totals for ' + foldername)

    axs[1].plot(x, df_vis_t["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
    axs[1].set_xlabel("Time (days)")
    axs[1].set_ylabel("# of People")
    axs[1].legend()
    #axs[1].margins(0,0)

    plt.savefig(base_name + "totals_SEVIR+D.png")
    plt.close(fig)

done = True