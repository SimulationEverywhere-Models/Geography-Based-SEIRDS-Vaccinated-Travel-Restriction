# Created by Eric (Jun 2021)
# and is a combination of two scripts originally created by Kevin

import sys
import pandas as pd
import geopandas as gpd
from collections import OrderedDict
from copy import deepcopy
import json
import math

if (len(sys.argv) < 2):
    print("\033[33mgenerateScenario -- Usage")
    print(" \033[36m$ python3 generateScenario <area> <progress=Y>\033[33m")
    print(" where \033[3m<area>\033[0;33m is either \033[1mOttawa\033[0;33m OR \033[1mOntario\033[0;33m")
    print(" and \033[3m<progress=Y>\033[0;33m is a toggle for the progress updates (it defaults to on and a 'N' turns them off\033[0m")
    sys.exit(-1)
#if

no_progress = len(sys.argv) > 2 and sys.argv[2] == "N"

# Setup variables that handle the area
input_area  = str(sys.argv[1]).lower()
cadmium_dir = "../../cadmium_gis/"
input_dir   = "input_"

# Set the data based on the area passed in
if input_area == "ottawa":
    area_id         = "dauid"
    cadmium_dir     += "Ottawa_DAs/"
    clean_csv       = "DA Ottawa Clean.csv"
    adj_csv         = "DA Ottawa Adjacency.csv"
    gpkg_file       = "DA Ottawa.gpkg"
    input_dir       += "ottawa_da/"
    rows            = "DApop_2016"
    region_id_type  = "DAuid"
    region_id       = "dauid"
    neighbor_id     = "Neighbor_dauid"
    rel_row_col     = "DAarea"
    progress_freq   = 1000
elif input_area == "ontario":
    area_id         = "PHU_ID"
    cadmium_dir     += "Ontario_PHUs/"
    clean_csv       = "ontario_phu_clean.csv"
    adj_csv         = "ontario_phu_adjacency.csv"
    gpkg_file       = "ontario_phu.gpkg"
    input_dir       += "ontario_phu/"
    rows            = "population"
    region_id_type  = "phu_id"
    region_id       = "region_id"
    neighbor_id     = "neighbor_id"
    rel_row_col     = "area_epsg4326"
    progress_freq   = 10
# Incorrect or unknown area
else:
    print("\033[31mOnly accepts 'Ottawa' or 'Ontario' as input areas\033[0m")
    print("\033[33m" + input_area + "\033[0m")
    sys.exit(-1)

def shared_boundaries(gdf, id1, id2):
    g1 = gdf[gdf[area_id] == str(id1)].geometry.iloc[0]
    g2 = gdf[gdf[area_id] == str(id2)].geometry.iloc[0]
    return g1.length, g2.length, g1.boundary.intersection(g2.boundary).length
#shared_boundaries()

def get_boundary_length(gdf, id1):
    g1 = gdf[gdf[area_id] == str(id1)].geometry.iloc[0]
    return g1.boundary.length
#get_boundary_length

df     = pd.read_csv(cadmium_dir + clean_csv)   # General information (id, population, area...)
df_adj = pd.read_csv(cadmium_dir + adj_csv)     # Pair of adjacent territories
gdf    = gpd.read_file(cadmium_dir + gpkg_file) # GeoDataFrame with the territories poligons
df.head()
gdf.head()

# Read default state from input json
default_cell  = json.loads( open(input_dir + "default.json", "r").read()      )
fields        = json.loads( open(input_dir + "fields.json", "r").read()       )
infectedCells = json.loads( open(input_dir + "infectedCell.json", "r").read() )

default_state              = default_cell["default"]["state"]
default_vicinity           = default_cell["default"]["neighborhood"]["default_cell_id"]
default_correction_factors = default_vicinity["infection_correction_factors"]
default_correlation        = default_vicinity["correlation"]
df_adj.head()

nan_rows           = df[ df[rows].isnull() ]
zero_pop_rows      = df[ df[rows] == 0 ]
invalid_region_ids = list( pd.concat([nan_rows, zero_pop_rows])[region_id_type] )

adj_full = OrderedDict()

for ind, row, in df_adj.iterrows():
    row_region_id           = row[region_id]
    row_neighborhood_id     = row[neighbor_id]
    row_region_id_str       = str(row_region_id)
    row_neighborhood_id_str = str(row_neighborhood_id)

    if row_region_id in invalid_region_ids:
        print("Invalid region ID found:", row_region_id)
        continue
    elif row_neighborhood_id in invalid_region_ids:
        print("Invalid neighborhood region ID found:", row_neighborhood_id)
        continue
    elif row_region_id_str not in adj_full:
        rel_row = df[ df[region_id_type] == row[region_id] ].iloc[0, :]
        pop     = int(rel_row[rows])
        area    = rel_row[rel_row_col]

        state = deepcopy(default_state)
        state["population"] = pop
        expr = dict()
        expr[row_region_id_str]     = {"state": state, "neighborhood": {}}
        adj_full[row_region_id_str] = expr
    #elif

    l1, l2, shared  = shared_boundaries(gdf, row_region_id, row_neighborhood_id)
    correlation     = (shared/l1 + shared/l2) / 2  # Equation extracted from Zhong paper (boundaries only, we don't have roads info for now)
    if correlation == 0:
        continue

    expr = {"correlation": correlation, "infection_correction_factors": default_correction_factors}
    adj_full[row_region_id_str][row_region_id_str]["neighborhood"][row_neighborhood_id_str]=expr

    if not(no_progress) and ind % progress_freq == 0:
        print("\033[1;33m", ind, "\t\033[0;33m%.2f%%" % (100*ind/len(df_adj)), "\033[0m")
#for:

if not no_progress:
    print("\033[1;32m", ind, "\t\033[0;32m%.1f%%" % math.ceil(100*ind/len(df_adj)), "\033[0m")
else:
    print("\033[1;32mDone.\033[0m")

for key, value in adj_full.items():
    # Insert every cell into its own neighborhood, a cell is -> cell = adj_full[key][key]
    adj_full[key][key]["neighborhood"][key] = {"correlation": default_correlation, "infection_correction_factors": default_correction_factors}
#for

# Insert cells from ordered dictionary into index "cells" of a new OrderedDict
template = OrderedDict()
template["cells"] = {}
template["cells"]["default"] = default_cell["default"]

infected_index = list()
for key in infectedCells:
    infected_index.append(key)

for key, value in adj_full.items():
    # Write cells in cadmium master format
    template["cells"][key] = value[key]

    # Overwrite the state variables of the infected cell
    # This should be modified to support any number of infected cells contained in the infectedCell.json file
    if key in infected_index:
        template["cells"][key]["state"]["susceptible"] = infectedCells[key]["state"]["susceptible"]
        template["cells"][key]["state"]["exposed"]     = infectedCells[key]["state"]["exposed"]

        if "infected" in infectedCells[key]["state"]:
            template["cells"][key]["state"]["infected"]   = infectedCells[key]["state"]["infected"]
        if "recovered" in infectedCells[key]["state"]:
            template["cells"][key]["state"]["recovered"]  = infectedCells[key]["state"]["recovered"]
        if "fatalities" in infectedCells[key]["state"]:
            template["cells"][key]["state"]["fatalities"] = infectedCells[key]["state"]["fatalities"]
    #if
#for

# Insert fields object at the end of the json for use with the GIS Webviewer V2
template["fields"] = fields["fields"]
adj_full_json = json.dumps(template, indent=4, sort_keys=False)  # Dictionary to string (with indentation=4 for better formatting)

with open("output/scenario_"+input_area+".json", "w") as f:
    f.write(adj_full_json)
#with
