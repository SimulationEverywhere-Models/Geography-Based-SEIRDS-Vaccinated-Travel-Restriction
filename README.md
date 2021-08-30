Cell-DEVS Geographical SEIRDS-Vaccinated Pandemic Model
===
Overview
----
This model is an extension of a previous [SEIRDS model](https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS) and
is based on the [Zhong model](https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7088542/pdf/11430_2009_Article_44.pdf).
<br>_Follow below for steps on just running the simulator. Consult the [Wiki](https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated/wiki) for compiling and running it_

Requirements
---
* The latest [release](https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated/releases)
* [PowerShell 7](https://github.com/PowerShell/PowerShell/releases) - _make sure to only get one that's tagged as 'latest release'_ (**Windows Only**)  
* [Anaconda](#downloading-conda)
* [Python 3](https://www.python.org/downloads/) (`sudo apt install python3` - **Linux**)
* [Java Runtime Environment](https://www.oracle.com/java/technologies/javase-downloads.html) (`sudo apt install default-jre` - **Linux**)
* [CMake](https://cmake.org/download/) (`sudo apt install cmake` - **Linux**)

Downloading Conda
---
This project uses several python scripts that are dependent on the libraries: geopandas, numpy, and matplotlib. geopandas requires specific versions of many dependent libraries and installing them manually is difficult. The easiest way to download the right packages is to use conda. It is also recommended to install these packages in a seperate conda environment. Attemping to install in conda base may fail.
1. https://www.anaconda.com/products/individual
2. select 64-Bit (x86) Installer (Linux/Windows)
3. run the installer
   3.1 On Windows, once the installer completes, open an Anaconda shell and run `conda init`
4.
~~~ 
conda create --name geo_env
conda activate geo_env
conda install geopandas numpy matplotlib
~~~

[Windows Only] PowerShell 7 Colors
---
If you see a lot of `←[033m` being printed in the shell, then PowerShell doesn't have terminal colors enabled and a good amount of colors are used to make it clearer to spot important information. Luckily it's pretty easy to turn on. Paste the following command in the shell:
1. ```Set-ItemProperty HKCU:\Console VirtualTerminalLevel -Type DWORD 1```
2. Now restart the shell.
3. Navigate back to SEVIRDS. `conda activate geo_env`
4. `.\run_simulation.ps1 -GenScenario ontario`

You should see the progress meters and loading animations in color and not like `←[033m`

Running a Simulation
----
1. Extract the .zip folder and an open a terminal there (PowerShell 7 on **Windows**)
3. Enter the following command (based on the appropriate OS)
   * `./RunSimulation.sh -a=ontario`
   * `.\RunSimulation.ps1 ontario`
4. If it completes you can view the results using the path displayed in the terminal
  
On Windows use `Get-Help ./RunSimulation` and `./RunSimulation.sh -h` to get more details on flags and parameters

Viewing Results in GIS Web Viewer V2
---
When a simulation completes the results folder will contain a logs folder, with graphs, and 4 files: .geojson, messages.log, structure.json, and visualization.json. Upload these 4 to the  [GIS_Viewer](http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html) to view simulation results on a map of the region
