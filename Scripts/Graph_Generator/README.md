These scripts generate graphs and per region statistics from the log files

To use it, run the simulation, which will create output files in the logs folder.

Then run the scripts by using:

~~~
python3 graph_per_regions.py
python3 graph_aggregates.py
~~~

_Note_: python 3 is required and the scripts should be run in this order because the first script creates the stats folder

The output graphs will be written to logs/stats