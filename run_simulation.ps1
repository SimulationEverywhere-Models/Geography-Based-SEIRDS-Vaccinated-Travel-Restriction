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
    [switch]$Rebuild = $False,

    # Builds a debug version of the siomulator
    [switch]$DebugSim = $False,

    # Enables on build warnings
    [switch]$Wall = $False
)

# Check if any of the above params were set
$params = "Area", "Clean", "CleanAll", "Days", "GenScenario", "GraphPerRegions", "GenRegionsGraphs", "Name", "NoProgress", "Rebuild", "DebugSim", "Wall"
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
        Move-Item output\scenario_${AreaFile}.json ..\..\config -Force
        Remove-Item output -Recurse
        Set-Location $HomeDir
    }

    function Clean
    {
        if ($CleanAll)
        {
            Write-Output "Removing ${YELLOW}all${RESET} runs for ${YELLOW}${AREA}${RESET}"
            if (Test-Path $VisualizationDir) {
                Remove-Item $VisualizationDir -Recurse -Verbose 4>&1 |
                ForEach-Object{ `Write-Host ($_.Message -replace'(.*)Target "(.*)"(.*)',' $2') -ForegroundColor Red}
            }
            Write-Output ${GREEN}"Done."${RESET}
        }
        else
        {
            Write-Output "Removing ${YELLOW}${Clean}${RESET} simulation run from ${YELLOW}${AREA}${RESET}"
            if (Test-Path ${VisualizationDir}${Clean}) {
                Remove-Item $VisualizationDir$Clean -Recurse -Verbose 4>&1 |
                ForEach-Object { `Write-Host ($_.Message -replace '(.*)Target "(.*)"(.*)', ' $2') -ForegroundColor Red }
            }
            Write-Output ${GREEN}"Done."${RESET}
        }
    }

    function ErrorCheck()
    {
        if ($LASTEXITCODE -ne 0) {
            Set-Location $HomeDir
            break
        }
    }

    function DependencyCheck()
    {
        Write-Verbose "Checking Dependencies..."

        # Test Cadmium
        if ( !(Test-Path "../cadmium") )
        {
            $parent = Split-Path -Path $HomeDir -Parent
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

        Write-Verbose $GREEN"Completed Dependency Check.`n"$RESET
    }

    function BuildSimulator()
    {
        # Remove the current executable
        if ($Rebuild -and (Test-Path ".\bin\${BuildFolder}\pandemic-geographical_model.exe")) {
            Remove-Item .\bin\${BuildFolder}\pandemic-geographical_model.exe -Recurse
        }

        # Build the executable if it doesn't exist
        if ( !(Test-Path ".\bin\$BuildFolder\pandemic-geographical_model.exe") ) {
            Write-Verbose "Building Model"
            cmake .\CMakeCache.txt -B .\bin "-DVERBOSE=$Verbose -DWALL=$W"
            ErrorCheck
            cmake --build .\bin --config $BuildFolder
            ErrorCheck
            Write-Verbose "${GREEN}Done."
        }
    }

    function GenerateGraphs([string] $LogFolder="", [bool] $GenAggregate=$True, [bool] $GenRegions=$False)
    {
        Write-Output "Generating Graphs:"
        $GenFolder = ".\Scripts\Graph_Generator\"

        if ($LogFolder -eq "") {
            if (Test-Path logs/stats) { Remove-Item logs/stats -Recurse }
            New-Item logs/stats -ItemType Directory | Out-Null
            $LogFolder = "logs"
        }

        if ($GenRegions) {
            python ${GenFolder}graph_per_regions.py "-ld=$LogFolder"
            ErrorCheck
        }

        if ($GenAggregate) {
            python ${GenFolder}graph_aggregates.py "-ld=$LogFolder"
        }
    }
# </Helpers> #

function Main()
{
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    if ($Name -ne "") { $VisualizationDir=$VisualizationDir+$Name }
    else {
        $i = 1
        while ($True) {
            if ( !(Test-Path "${VisualizationDir}run${i}") ) {
                $VisualizationDir=$VisualizationDir+"run"+$i
                break
            }
            $i++
        }
    }

    if ( !(Test-Path "logs") ) { New-Item "logs" -ItemType Directory | Out-Null }
    if ( !(Test-Path $VisualizationDir) ) { New-Item $VisualizationDir -ItemType Directory | Out-Null }

    # Generate Scenario file
    GenerateScenario

    # Run simulation
    Set-Location bin
    Write-Output "`nExecuting model for $days days:"
    cmd /c ${BuildFolder}\\pandemic-geographical_model.exe ../config/scenario_${AreaFile}.json $Days Y
    ErrorCheck
    Set-Location $HomeDir
    Write-Output "" # New line

    # Generate SEVIRDS graphs
    GenerateGraphs

    # Copy the message log + scenario to message log parser's input
    # Note this deletes the contents of input/output folders of the message log parser before executing
    Write-Verbose "Copying simulation results to message log parser:"
    # if ( !(Test-Path .\Scripts\Msg_Log_Parser\input) )  { New-Item .\Scripts\Msg_Log_Parser\input  -ItemType Directory | Out-Null }
    # if ( !(Test-Path .\Scripts\Msg_Log_Parser\output) ) { New-Item .\Scripts\Msg_Log_Parser\output -ItemType Directory | Out-Null }
    # Copy-Item config/scenario_${AreaFile}.json .\Scripts\Msg_Log_Parser\input
    # Copy-Item .\logs\pandemic_messages.txt .\Scripts\Msg_Log_Parser\input
    Write-Verbose "${GREEN}Done."

    # Run message log parser
    Write-Verbose "Running message log parser"
    # Set-Location .\Scripts\Msg_Log_Parser
    # java -jar sim.converter.glenn.jar "input" "output"
    # Expand-Archive -LiteralPath output\pandemic_messages.zip -DestinationPath output
    # Set-Location $HomeDir
    Write-Verbose "${GREEN}Done."

    Write-Verbose "Copying converted files to:${BLUE}${VisualizationDir}"
    # Move-Item .\Scripts\Msg_Log_Parser\output\messages.log $VisualizationDir
    # Move-Item .\Scripts\Msg_Log_Parser\output\structure.json $VisualizationDir
    # Remove-Item .\Scripts\Msg_Log_Parser\input
    # Remove-Item .\Scripts\Msg_Log_Parser\output
    # Remove-Item .\Scripts\Msg_Log_Parser/*.zip
    Copy-Item .\GIS_Viewer\${Area}\${AreaFile}.geojson $VisualizationDir
    Copy-Item .\GIS_Viewer\${Area}\visualization.json $VisualizationDir
    Move-Item logs $VisualizationDir
    Write-Verbose "${GREEN}Done.`n"

    $Hours   = $stopwatch.Elapsed.Hours
    $Minutes = $stopwatch.Elapsed.Minutes
    $Seconds = $stopwatch.Elapsed.Seconds
    $stopwatch.Stop()

    Write-Host -NoNewline "${GREEN}Simulation Completed ("
    if ( $Hours -gt 0) { Write-Host -NoNewline "${GREEN}${Hours}h" }
    if ( $Minutes -gt 0 ) { Write-Host -NoNewline "${GREEN}${Minutes}m" }
    Write-Output "${GREEN}${Seconds}s)${RESET}"
    Write-Output "View results using the files in ${BOLD}${BLUE}run${i}${RESET} and this web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
}

if ($ParamsNotNull) {
    $Progress = (($NoProgress) ? "N" : "Y")
    $BuildFolder = (($DebugSim) ? "Debug" : "Release")
    $W = (($Wall) ? "Y" : "N")
    $Verbose  = (($VerbosePreference -eq "SilentlyContinue" ? "N" : "Y"))
    $HomeDir = Get-Location

    if ($Area -ne "") {
        if ($Area -eq "ontario" -or $Area -eq "on") {
            $Area = "ontario"
            $AreaFile = "ontario_phu"
        } elseif ($Area -eq "ottawa" -or $Area -eq "ot") {
            $Area = "ottawa"
            $AreaFile = "ottawa_da"
        } else {
            Write-Output "${RED}Unknown Area ${BOLD}${Area}${RESET}"
            exit -1
        }

        $VisualizationDir = "GIS_Viewer/${Area}/simulation_runs/"
    }

    if ($CleanAll -or $Clean -ne "") {
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        Clean
        break
    }

    DependencyCheck

    if ($GenScenario) {
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        GenerateScenario
    } elseif ($GenRegionsGraphs) {
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        GenerateGraphs "" $False $True
    } else {
        BuildSimulator

        if ($Area -ne "") { Main }
    }
}
# Display the help if no params were set
else {
    if ($VerbosePreference) { Get-Help .\run_simulation.ps1 -full }
    else { Get-Help .\run_simulation.ps1 }
}