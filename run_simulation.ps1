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
    [switch]$DebugSim = $False
) #params()

# Check if any of the above params were set
$private:params        = "Area", "Clean", "CleanAll", "Days", "GenScenario", "GraphPerRegions", "GenRegionsGraphs", "Name", "NoProgress", "Rebuild", "DebugSim"
$private:ParamsNotNull = $False
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
    <#
    .SYNOPSIS
    Generates the scenario file for a region and places it in the config directory
    .PARAMETER Private:Area
    Area to generate scenario. Case sensitive
    .EXAMPLE
    GenerateScenario ontario
    .NOTES
    General notes
    #>
    function GenerateScenario([string] $Private:Area)
    {
        # Create output directory if non-existant
        if (!(Test-Path "Scripts/Input_Generator/output")) {
            New-Item "Scripts/Input_Generator/output" -ItemType Directory | Out-Null
        }

        Write-Output "Generating $BLUE$Area$RESET Scenario:"
        Set-Location Scripts\Input_Generator
        python.exe generateScenario.py $Area $PROGRESS
        ErrorCheck
        Move-Item output\scenario_${Area}.json ..\..\config -Force
        Remove-Item output -Recurse
        Set-Location $HomeDir
    } #GenerateScenario()

    <#
    .SYNOPSIS
    Cleans either all runs or a specified run
    .PARAMETER private:VisualizationDir
    Path to the simulations runs
    .PARAMETER private:Run
    Name of specific run to clean
    .PARAMETER private:CleanAll
    True cleans all simulation runs
    .EXAMPLE
    Clean "GIS_Viewer/ontario/simulation_runs/" "run1"
        Cleans run1 found in "GIS_Viewer/ontario/simulation_runs/"
    #>
    function Clean([string] $Private:VisualizationDir, [string] $Private:Run, [bool] $Private:CleanAll=$False)
    {
        if ($CleanAll)
        {
            Write-Output "Removing ${YELLOW}all${RESET} runs for ${YELLOW}${Area}${RESET}"
            if (Test-Path $VisualizationDir) {
                Remove-Item $VisualizationDir -Recurse -Verbose 4>&1 |
                ForEach-Object{ `Write-Host ($_.Message -replace'(.*)Target "(.*)"(.*)',' $2') -ForegroundColor Red}
            }
            Write-Output ${GREEN}"Done."${RESET}
        }
        else
        {
            Write-Output "Removing ${YELLOW}${Run}${RESET} simulation run from ${YELLOW}${AREA}${RESET}"
            if (Test-Path ${VisualizationDir}${Run}) {
                Remove-Item $VisualizationDir$Run -Recurse -Verbose 4>&1 |
                ForEach-Object { `Write-Host ($_.Message -replace '(.*)Target "(.*)"(.*)', ' $2') -ForegroundColor Red }
            }
            Write-Output ${GREEN}"Done."${RESET}
        }
    } #Clean()

    <#
    .SYNOPSIS
    Computes and diplas whether the build was a success
    and how much time is took using the the stopwatch
    .PARAMETER success
    True if the simulation was a success
    .PARAMETER private:stopWatch
    Stopwartch object containing how much time has passed
    .EXAMPLE
    ComputeBuildTime $True $null
        Displays success message but no execution time
    #>
    function ComputeBuildTime([bool] $success, [System.Diagnostics.Stopwatch] $private:stopWatch=$null)
    {
        if ($stopWatch)
        {
            $Hours = $stopWatch.Elapsed.Hours
            $Minutes = $stopWatch.Elapsed.Minutes
            $Seconds = $stopWatch.Elapsed.Seconds
            $stopWatch.Stop()
        }

        $Color = (($success) ? $GREEN : $RED)

        if ($success) { Write-Host -NoNewline "`n${GREEN}Simulation Completed" }
        else { Write-Host -NoNewline "`n${RED}Simulation Failed" }

        if ($stopWatch)
        {
            Write-Host -NoNewline $Color" ("
            if ( $Hours -gt 0) { Write-Host -NoNewline "${Color}${Hours}h" }
            if ( $Minutes -gt 0 ) { Write-Host -NoNewline "${Color}${Minutes}m" }
            Write-Output "${Color}${Seconds}s)${RESET}"
        }

        if ($success) {
            Write-Output "View results using the files in ${BOLD}${BLUE}run${i}${RESET} and this web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
        }
    } #ComputeBuildTime()

    <#
    .SYNOPSIS
    Checks if any errors have been returned and stops scripts if so.
    Should be placed directly after a program call.
    .EXAMPLE
    python generate_scenario.py
    ErrorCheck
        If the generate_scenario returns an error the script will exit.
    #>
    function ErrorCheck()
    {
        # 0 => All is good
        if ($LASTEXITCODE -ne 0) {
            ComputeBuildTime $False $stopwatch # Display failed message and time
            Set-Location $HomeDir # Go back to the default location
            break
        }
    }

    <#
    .SYNOPSIS
    Verifies ALL dependencies are met
    #>
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
        $private:Dependencies = @{
            # dependency=version, website
            cmake="3.21.0","https://cmake.org/";
            gcc="x86_64-posix-seh-rev0", "https://www.msys2.org/";
            python="Python 3", "https://www.python.org/downloads/";
            conda="conda 4", "https://www.anaconda.com/products/individual#windows"
        }

        # Python Depedencies
        $private:Libs = "numpy", "geopandas", "matplotlib"
        try {
            # Loop through each dependency
            foreach($private:depends in $Dependencies.keys) {
                # Try and get the version
                $private:version = cmd /c "$depends --version"

                # If the version is incorrect or non-existant, throw an error
                if ( !($version -like "*"+${Dependencies}.${depends}[0]+"*") ) { throw 1 }

                # It was found
                Write-Verbose "$depends ${GREEN}[FOUND]"
            }

            # Loop through each python dependency
            $private:condaList = conda list
            foreach($private:depends in $Libs) {
                if ( !($condaList -like "*${depends}*") ) { throw 2 }
                Write-Verbose "$depends ${GREEN}[FOUND]"
            }
        } catch {
            Write-Verbose "$depends ${RED}[NOT FOUND]" -Verbose

            # Dependency Prints
            if ($Error[0].Exception.Message -eq 1) {
                $private:minVersion = $Dependencies.$depends[0]
                Write-Verbose $YELLOW"Check that '$depends --version' contains this version $minVersion"
                $private:website = $Dependencies.$depends[1]
                Write-Verbose $YELLOW"$depends for Windows can be installed from here: ${BLUE}$website"
            # Python Dependency Prints
            } else {
                Write-Verbose $YELLOW'Check `conda list  | Select-String "'"$depends"'"`'
                Write-Verbose $YELLOW'It can be installed using `conda install '$depends'`'
            }

            break
        }

        Write-Verbose $GREEN"Completed Dependency Check.`n"$RESET
    } #DependencyCheck()

    <#
    .SYNOPSIS
    Builds the simulator if it is not and can also be set to rebuild it. Can also build Debug if $BuildFolder set correctly
    .PARAMETER Rebuild
    True rebuilds the simulator. Use with Verbose to do a complete rebuild
    .PARAMETER BuildType
    The type of build (e.g., Release or Debug)
    .PARAMETER Verbose
    True prints out all warnings while building and completely
    rebuilds the simulator if Rebuild is set to true
    .EXAMPLE
    BuiSimulator $True "Debug" "Y" "Y"
        Completely rebuilds the simulator and creates a debug executable
    #>
    function BuildSimulator([bool] $Private:Rebuild=$False, [string] $Private:BuildType="Release", [string] $Private:Verbose="N")
    {
        # Remove the current executable
        if ($Rebuild) {
            # Clean everything for a complete rebuild
            if ($Verbose -eq "Y" -and (Test-Path ".\bin\")) {
                Remove-Item .\bin\ -Recurse
            # Otherwise just clean the executable for a quick rebuild
            } elseif ( (Test-Path ".\bin\$BuildType\pandemic-geographical_model.exe") ) {
                Remove-Item .\bin\$BuildType\pandemic-geographical_model.exe
            }
        }

        # Build the executable if it doesn't exist
        if ( !(Test-Path ".\bin\$BuildType\pandemic-geographical_model.exe") ) {
            Write-Verbose "Building Model"
            cmake .\CMakeCache.txt -B .\bin "-DVERBOSE=$Verbose"
            ErrorCheck
            cmake --build .\bin --config $BuildType
            ErrorCheck
            Write-Verbose "${GREEN}Done."
        }
    }

    <#
    .SYNOPSIS
    Generates all the graphs for a single run, provided the correct variables are set
    .PARAMETER LogFolder
    Path to the log folder (can be relative to where the script's directory)
    .PARAMETER GenAggregate
    True is the aggregated graphs should be generated
    .PARAMETER GenRegions
    True if the regional graphs should be generated
    .EXAMPLE
    GenerateGraphs "logs" $True $False
        Will generate the aggregated graphs using the data in the Geographical-Based-SEIRDS-Vaccinated/logs folder
    #>
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
            python ${GenFolder}graph_per_regions.py  $Progress "-ld=$LogFolder"
            ErrorCheck
        }

        if ($GenAggregate) {
            python ${GenFolder}graph_aggregates.py $Progress "-ld=$LogFolder"
        }
    }
# </Helpers> #

function Main()
{
    # Used for execution time at the end of Main 
    $local:stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

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
    GenerateScenario $Area

    # Run simulation
    Set-Location bin
    Write-Output "`nExecuting model for $Days days:"
    cmd /c ${BuildFolder}\\pandemic-geographical_model.exe ../config/scenario_${Area}.json $Days $Progress
    ErrorCheck
    Set-Location $HomeDir
    Write-Output "" # New line

    # Generate SEVIRDS graphs
    GenerateGraphs "" $True $GraphPerRegions

    # Copy the message log + scenario to message log parser's input
    # Note this deletes the contents of input/output folders of the message log parser before executing
    Write-Verbose "Copying simulation results to message log parser:"
    # if ( !(Test-Path .\Scripts\Msg_Log_Parser\input) )  { New-Item .\Scripts\Msg_Log_Parser\input  -ItemType Directory | Out-Null }
    # if ( !(Test-Path .\Scripts\Msg_Log_Parser\output) ) { New-Item .\Scripts\Msg_Log_Parser\output -ItemType Directory | Out-Null }
    # Copy-Item config/scenario_${Area}.json .\Scripts\Msg_Log_Parser\input
    # Copy-Item .\logs\pandemic_messages.txt .\Scripts\Msg_Log_Parser\input
    Write-Verbose "${GREEN}Done."

    # Run message log parser
    Write-Verbose "Running message log parser"
    # Set-Location .\Scripts\Msg_Log_Parser
    # java -jar sim.converter.glenn.jar "input" "output"
    # Expand-Archive -LiteralPath output\pandemic_messages.zip -DestinationPath output
    # Set-Location $HomeDir
    Write-Verbose "${GREEN}Done."

    Write-Verbose "Copying converted files to: ${BLUE}${VisualizationDir}"
    # Move-Item .\Scripts\Msg_Log_Parser\output\messages.log $VisualizationDir
    # Move-Item .\Scripts\Msg_Log_Parser\output\structure.json $VisualizationDir
    # Remove-Item .\Scripts\Msg_Log_Parser\input
    # Remove-Item .\Scripts\Msg_Log_Parser\output
    # Remove-Item .\Scripts\Msg_Log_Parser/*.zip
    Copy-Item .\GIS_Viewer\${Area}\${AreaFile}.geojson $VisualizationDir
    Copy-Item .\GIS_Viewer\${Area}\visualization.json $VisualizationDir
    Move-Item logs $VisualizationDir
    Write-Verbose "${GREEN}Done.`n"

    ComputeBuildTime $True $stopwatch
}

if ($ParamsNotNull) {
    # Setup global variables
    $Script:Progress   = (($NoProgress) ? "-np" : "")
    $local:BuildFolder = (($DebugSim) ? "Debug" : "Release")
    $local:Verbose     = (($VerbosePreference -eq "SilentlyContinue" ? "N" : "Y"))
    $Script:HomeDir    = Get-Location

    # Setup Area variables
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

    # If clean then do this before the dependency check
    # so it's quicker and we don't have to worry about having
    # things installed like Python
    if ($CleanAll -or $Clean -ne "") {
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        Clean $VisualizationDir $Clean
        break
    }

    # Check all dependencies are met
    DependencyCheck

    # Only generate the scenario
    if ($GenScenario) {
        # A region must be set
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        GenerateScenario $Area
    # Only generate the graphs per region on a specified run
    } elseif ($GenRegionsGraphs) {
        # A region must be set
        if ($Area -eq "") { Write-Output "${RED}Area must be set${RESET}"; exit -1 }
        GenerateGraphs  "${VisualizationDir}${GenRegionsGraphs}/logs" $False $True
    } else {
        BuildSimulator $Rebuild $BuildFolder $Verbose

        if ($Area -ne "") { Main }
    }
}
# Display the help if no params were set
else {
    if ($VerbosePreference) { Get-Help .\run_simulation.ps1 -full }
    else { Get-Help .\run_simulation.ps1 }
}