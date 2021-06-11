These scripts generate graphs and per region statistics from the log files

To use it, run the simulation, which will create output files in the logs folder.

Then run the scripts by using: 
python graph_per_regions.py
python graph_aggregates.py

note: python 3 may be required, and they should be run in this order becuase the first script creates the stats folder

The output graphs will be written to logs/stats