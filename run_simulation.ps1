<#
.SYNOPSIS
    Written by Eric - Summer 2021
        ###
    Runs a simulation on the inputed area (see -Example), while
    offering many options such as recompiling the simulator before running,
    cleaning specific simulation run folders, only running the scenario generation
    script, ...
.DESCRIPTION
    Running a simple simulation (see example 1) performs these steps:
        1. Generate scenario file from config file (Python script)
        2. Runs simulator for inputed number of days (default=500)
        3. Generates graphs per region in the area if the flag is set (default=off)
        4. Generates graphs for the area
        5. Parses data to be used by webviewer
        6. Clean ups

    This script is dependent on:
        - geopandas
        - cmake
        - matplotlib
        - numpy
        - java
        - gcc (g++)
        - Cadmium repo in the same directory as SEVIRDS
.INPUTS
    None. Once you set the flags you want, everything is handled for you
.EXAMPLE
    ./run_simulation.ps1 -Area on
        Runs a simulation on the Ontario config
.EXAMPLE
    ./run_simulation.ps1 -Area on -Rebuild
        Re-compiles the simulator then runs a simulation on the Ontario config
.EXAMPLE
    ./run_simulation.ps1 -GenScenario -Area on
        Generates the scenario file used by the simulator using the Ontario config. Running a sim
        like in examples 1 and 2 does this automatically and this is for when you just want the
        scenario file re-done (useful when debugging)
.EXAMPLE
    ./run_simulation.ps1 -GraphRegion -Area on
        Runs a simulation on the Ontario config and generates graphs per region since
        by default this is turned off (it takes a longer time to do then Aggregated graphs)
        and isn't always that useful)
.Link
    https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated
#>

[CmdletBinding()]
param(
    # Sets the Area to run a simulation on (Currently supports Ontario and Ottawa
    [string]$Area = "",

    # Cleans the simulation run with the inputed folder name such as '-Clean run1' (Needs to set the Area)
    [string]$Clean = "",

    # Cleans all simulation runs (Needs to set the Area)
    [switch]$CleanAll = $False,

    # Sets the number of days to run a simulation (default=500)
    [int32]$Days = 500,

    # Generates a scenario json file (an area flag needs to be set)
    [switch]$GenScenario = $False,

    # Will generate graphs per region after running the simulation (default=off)
    [switch]$GraphPerRegions = $False,

    # Generates graphs per region for the specified run
    [string]$GenRegionsGraphs = "",

    # Sets the name of the folder to save the run
    [string]$Name = "",

    # Turns off progress and loading animations
    [switch]$NoProgress = $False,

    # Re-Compiles the simulator
    [switch]$Rebuild = $False
)

# Check if any of the above params were set
$params = "Area", "Clean", "CleanAll", "Days", "GenScenario", "GraphPerRegions", "GenRegionsGraphs", "Name", "NoProgress", "Rebuild"
$ParamsNotNull = $False
foreach($param in $params) { if ($PSBoundParameters.keys -like "*"+$param+"*") { $ParamsNotNull = $True; break; } }

# <Colors> #
    $RESET  = "[0m"
    $RED    = "[31m"
    $GREEN  = "[32m"
    $YELLOW = "[33m"
    $BLUE   = "[36m"
    $BOLD   = "[1m"
# </Colors> #

# <Helpers> #
    function GenerateScenario
    {
        if (!(Test-Path "Scripts/Input_Generator/output")) {
            New-Item "Scripts/Input_Generator/output" -ItemType Directory | Out-Null
        }

        Write-Output "Generating $BLUE$AREA$RESET Scenario:"
        Set-Location Scripts\Input_Generator
        python.exe generateScenario.py $AREA $PROGRESS
        ErrorCheck
        Move-Item output\scenario_${AREA_FILE}.json ..\..\config -Force
        Remove-Item output -Recurse
        Set-Location ..\..
    }

    function Clean
    {
        if ($CleanAll)
        {
            Write-Output "Removing ${YELLOW}all${RESET} runs for ${YELLOW}${AREA}${RESET}"
            if (Test-Path $visualization_dir) {
                Remove-Item $visualization_dir -Recurse -Verbose 4>&1 |
                ForEach-Object{ `Write-Host ($_.Message -replace'(.*)Target "(.*)"(.*)',' $2') -ForegroundColor Red}
            }
            Write-Output ${GREEN}"Done."${RESET}
        }
        else
        {
            Write-Output "Removing ${YELLOW}${Clean}${RESET} simulation run from ${YELLOW}${AREA}${RESET}"
            if (Test-Path ${visualization_dir}${Clean}) {
                Remove-Item $visualization_dir$Clean -Recurse -Verbose 4>&1 |
                ForEach-Object { `Write-Host ($_.Message -replace '(.*)Target "(.*)"(.*)', ' $2') -ForegroundColor Red }
            }
            Write-Output ${GREEN}"Done."${RESET}
        }
    }

    function ErrorCheck()
    {
        if ($LASTEXITCODE -ne 0) {
            Set-Location $Home_Dir
            break
        }
    }

    function DependencyCheck()
    {
        Write-Verbose "Checking Dependencies..."

        # Test Cadmium
        if ( !(Test-Path "../cadmium") )
        {
            $parent = Split-Path -Path $Home_Dir -Parent
            Write-Verbose "Cadmium ${RED}[NOT FOUND]" -Verbose
            Write-Verbose ${YELLOW}"Make sure it's in "${YELLOW}${parent}
            break
        } else { Write-Verbose "Cadmium ${GREEN}[FOUND]" }

        # Setup dependency specific data
        $Dependencies = @{
            # dependency=version, website
            cmake="3.21.0","https://cmake.org/";
            gcc="x86_64-posix-seh-rev0", "https://www.msys2.org/";
            python="Python 3", "https://www.python.org/downloads/";
            conda="conda 4", "https://www.anaconda.com/products/individual#windows"
        }

        # Python Depedencies
        $Libs = "numpy", "geopandas", "matplotlib"
        try {
            # Loop through each dependency
            foreach($depends in $Dependencies.keys) {
                # Try and get the version
                $version = cmd /c "$depends --version"

                # If the version is incorrect or non-existant, throw an error
                if ( !($version -like "*"+${Dependencies}.${depends}[0]+"*") ) { throw 1 }

                # It was found
                Write-Verbose "$depends ${GREEN}[FOUND]"
            }

            $condaList = conda list
            foreach($depends in $Libs) {
                if ( !($condaList -like "*${depends}*") ) { throw 2 }
                Write-Verbose "$depends ${GREEN}[FOUND]"
            }
        } catch {
            Write-Verbose "$depends ${RED}[NOT FOUND]" -Verbose

            # Dependency Prints
            if ($Error[0].Exception.Message -eq 1) {
                $minVersion = $Dependencies.$depends[0]
                Write-Verbose $YELLOW"Check that '$depends --version' contains this version $minVersion"
                $website = $Dependencies.$depends[1]
                Write-Verbose $YELLOW"$depends for Windows can be installed from here: ${BLUE}$website"
            # Python Dependency Prints
            } else {
                Write-Verbose ${YELLOW}'Check `conda list  | Select-String "'"$depends"'"`'
                Write-Verbose ${YELLOW}'It can be installed using `conda install '$depends'`'
            }

            break
        }

        Write-Verbose $GREEN"Completed Dependency Check."$RESET
    }

    function BuildSimulator()
    {
        # Remove the current executable
        if ($Rebuild -and (Test-Path .\bin)) { Remove-Item .\bin -Recurse }

        # Build the executable if it doesn't exist
        if ( !(Test-Path .\bin\cmake_install.cmake) ) {
            Write-Verbose "Building Model"
            cmake .\CMakeCache.txt -B .\bin "-DVERBOSE=$Verbose"
            ErrorCheck
            cmake --build .\bin --config Release
            ErrorCheck
            Write-Verbose "${GREEN}Done."
        }
    }

    function GenerateAggregatedGraphs()
    {
        Write-Output "Generating Graphs:"
        python .\Scripts\Graph_Generator\graph_aggregates.py "-ld=C:/Users/erme2/Documents/GitHub/Geography-Based-SEIRDS-Vaccinated/logs"
    }
# </Helpers> #

function Main()
{
    $visualization_dir=$visualization_dir+"run/"

    if ( !(Test-Path "logs") ) { New-Item "logs" -ItemType Directory | Out-Null }
    if ( !(Test-Path $visualization_dir) ) { New-Item $visualization_dir -ItemType Directory | Out-Null }

    GenerateScenario

    Set-Location bin
    Write-Output "`nExecuting model for $days days:"
    ./Release/pandemic-geographical_model.exe ../config/scenario_${Area_File}.json $Days "Y"
    ErrorCheck
    Set-Location ..

    GenerateAggregatedGraphs
}

if ($ParamsNotNull) {
    $Progress = (($NoProgress) ? "N" : "Y")
    $Verbose  = (($VerbosePreference -eq "SilentlyContinue" ? "N" : "Y"))
    $Home_Dir = Get-Location

    DependencyCheck
    BuildSimulator

    if ($Area -ne "") {
        if ($Area -eq "ontario" -or $Area -eq "on") {
            $Area = "ontario"
            $Area_File = "ontario_phu"
        } elseif ($Area -eq "ottawa" -or $Area -eq "ot") {
            $Area = "ottawa"
            $Area_File = "ottawa_da"
        } else {
            Write-Output "${RED}Unknown Area ${BOLD}${Area}${RESET}"
            exit -1
        }

        $visualization_dir = "GIS_Viewer/${Area}/simulation_runs/"
    }

    if ($GenScenario) {
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        GenerateScenario
    } elseif($CleanAll -or $Clean -ne "") {
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        Clean
    } else { Main }
}
# Display the help if no params were set
else {
    if ($VerbosePreference) { Get-Help .\run_simulation.ps1 -full }
    else { Get-Help .\run_simulation.ps1 }
}