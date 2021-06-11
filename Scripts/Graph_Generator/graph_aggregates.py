# Original author: Kevin
# Modified by: Binyamin
# Modified further by : Glenn - 24/03/2021

#!/usr/bin/env python
# coding: utf-8

# In[1]:


import os
import re
from collections import defaultdict
from copy import copy

import itertools, threading, time, sys

done = False
def animate():
    for c in itertools.cycle(['|', '/', '-', '\\']):
        if done:
            break
        sys.stdout.write('\r\033[33maggregating graphs ' + c + '\033[0m')
        sys.stdout.flush()
        time.sleep(0.1)
    sys.stdout.write('\r\033[32mDone.                   \033[0m\n')

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
sIndex = 1
eIndex = 2
iIndex = 3
rIndex = 4
neweIndex = 5
newiIndex = 6
newrIndex = 7
dIndex = 8


# In[3]:

COLOR_SUSCEPTIBLE = 'xkcd:blue'
COLOR_INFECTED = 'xkcd:red'
COLOR_EXPOSED = 'xkcd:sienna'
COLOR_RECOVERED = 'xkcd:green'
COLOR_DEAD = 'xkcd:black'

# In[4]:

def curr_states_to_df_row(sim_time, curr_states, total_pop, line_num):
    total_S = 0
    total_E = 0
    total_I = 0
    total_R = 0
    total_D = 0
    new_E = 0
    new_I = 0
    new_R = 0

    # sum the number of S,E,I,R,D persons in all cells
    for key in curr_states:
        cell_population = curr_states[key][0]
        total_S += round(cell_population*(curr_states[key][sIndex]))
        total_E += round(cell_population*(curr_states[key][eIndex]))
        total_I += round(cell_population*(curr_states[key][iIndex]))
        total_R += round(cell_population*(curr_states[key][rIndex]))
        total_D += round(cell_population*(curr_states[key][dIndex]))

        new_E += round(cell_population*(curr_states[key][neweIndex]))
        new_I += round(cell_population*(curr_states[key][newiIndex]))
        new_R += round(cell_population*(curr_states[key][newrIndex]))
            
    # then divide each by the total population to get the percentage of population in each state
    percent_S = total_S / total_pop
    percent_E = total_E / total_pop
    percent_I = total_I / total_pop
    percent_R = total_R / total_pop
    percent_D = total_D / total_pop

    percent_new_E = new_E / total_pop
    percent_new_I = new_I / total_pop
    percent_new_R = new_R / total_pop
    psum = percent_S + percent_E + percent_I + percent_R + percent_D
    
    assert 0.95 <= psum < 1.05, ("at time" + str(curr_time))
    
    return [int(sim_time), percent_S, percent_E, percent_I, percent_R, percent_new_E, percent_new_I, percent_new_R, percent_D, psum]

# In[5]:


states = ["sus", "expos", "infec", "rec"]
data = []
curr_data = []
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
        
# iterate over all regions found and sum their initial populations
for region_id in initial_pop:
    total_pop += initial_pop[region_id]

# In[]:

with open(log_filename, "r") as log_file:
    line_num = 0
    
    # for each line, read a line then:
    for line in log_file:
        
        # strip leading and trailing spaces
        line = line.strip()
        
        # if a time marker is found that is not the current time
        if line.isnumeric() and line != curr_time:
            
            # if state is ready to write - write it to data before new time step
            if curr_states:
                data.append(curr_states_to_df_row(curr_time, curr_states, total_pop, line_num))
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
        
    data.append(curr_states_to_df_row(curr_time, curr_states, total_pop, line_num))

# In[7]:

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.colors as mcolors
import matplotlib
import shutil

# In[8]:

font = {'family' : 'DejaVu Sans',
        'weight' : 'normal',
        'size'   : 16}

matplotlib.rc('font', **font)

# In[6]: # create a folder for aggregates within stats, delete it if it already exists
    
path = log_file_folder + "/stats/aggregate"
shutil.rmtree(path, ignore_errors=True)
try: 
    os.mkdir(path) 
except OSError as error: 
    print(error)

base_name = path + "/"

# In[6]:
with open(base_name+"aggregate_timeseries.csv", "w") as out_file:
    out_file.write("sim_time, S, E, I, R, New_E, New_I, New_R, D, pop_sum\n")
    for timestep in data:
        out_file.write(str(timestep).strip("[]")+"\n")
    
# In[9]:

columns = ["time", "susceptible", "exposed", "infected", "recovered", "new_exposed", "new_infected", "new_recovered", "deaths", "error"]
df_vis = pd.DataFrame(data, columns=columns)
df_vis = df_vis.set_index("time")
df_vis.to_csv("states.csv")
df_vis.head()

# In[ ]:

# draw SEIR lines
fig, ax = plt.subplots(figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
ax.plot(x, 100*df_vis["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
ax.plot(x, 100*df_vis["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
ax.plot(x, 100*df_vis["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
ax.plot(x, 100*df_vis["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
plt.legend(loc='upper right')
plt.margins(0,0)
plt.title('Epidemic Aggregate SEIR Percentages')
plt.xlabel("Time (days)")
plt.ylabel("Population (%)")
plt.savefig(base_name + "SEIR.png")

# In[ ]:

# draw new EIR
fig, ax = plt.subplots(figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
ax.plot(x, 100*df_vis["new_exposed"], label="New exposed", color=COLOR_INFECTED, linewidth=linewidth)
ax.plot(x, 100*df_vis["new_infected"], label="New infected", color=COLOR_INFECTED, linewidth=linewidth)
ax.plot(x, 100*df_vis["new_recovered"], label="New recovered", color=COLOR_RECOVERED, linewidth=linewidth)
plt.legend(loc='upper right')
plt.margins(0,0)
plt.title('Epidemic Aggregate New EIR Percentages')
plt.xlabel("Time (days)")
plt.ylabel("Population (%)")
plt.savefig(base_name + "New_EIR.png")

# In[ ]:

# draw SEIRD

fig, axs = plt.subplots(2, figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
axs[0].plot(x, 100*df_vis["susceptible"], label="Susceptible", color=COLOR_SUSCEPTIBLE, linewidth=linewidth)
axs[0].plot(x, 100*df_vis["exposed"], label="Exposed", color=COLOR_EXPOSED, linewidth=linewidth)
axs[0].plot(x, 100*df_vis["infected"], label="Infected", color=COLOR_INFECTED, linewidth=linewidth)
axs[0].plot(x, 100*df_vis["recovered"], label="Recovered", color=COLOR_RECOVERED, linewidth=linewidth)
axs[0].set_ylabel("Population (%)")
axs[0].legend()
axs[0].margins(0,0)
axs[0].set_title('Epidemic Aggregate SEIR+D Percentages')

axs[1].plot(x, 100*df_vis["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
axs[1].set_xlabel("Time (days)")
axs[1].set_ylabel("Population (%)")
axs[1].set_ylim([0,6])
axs[1].legend()
axs[1].margins(0,0)

plt.savefig(base_name + "SEIR+D.png")

# In[ ]:

# draw new Deaths
fig, ax = plt.subplots(figsize=(15,6))
linewidth = 2

x = list(df_vis.index)
ax.plot(x, 100*df_vis["deaths"], label="Deaths", color=COLOR_DEAD, linewidth=linewidth)
ax.set_ylim([0,6])
plt.legend(loc='upper right')
plt.margins(0,0)
plt.title('Epidemic Aggregate Deaths')
plt.xlabel("Time (days)")
plt.ylabel("Population (%)")
plt.savefig(base_name + "Deaths.png")

done = True