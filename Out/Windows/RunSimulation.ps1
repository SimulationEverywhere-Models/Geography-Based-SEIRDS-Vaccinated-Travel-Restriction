<#
.SYNOPSIS
    Written by Eric - Summer 2021
        ###
    Runs a simulation on the inputed area (see -Example)
.DESCRIPTION
    Running a simple simulation (see example 1) performs these steps:
        1. Generate scenario file from config file (Python script)
        2. Runs simulator for inputed number of days (default=500)
        3. Generates graphs per region in the area if the flag is set (default=off)
        4. Generates graphs for the area (2 Python Scripts)
        5. Parses data to be used by webviewer (Java)
        6. Clean ups

    This script is dependent on:
        - geopandas (Python library installed through Conda)
        - matplotlib (Python library installed through Conda)
        - numpy (Python library installed through Conda)
        - java
        - Anaconda
        - PowerShell 7
.INPUTS
    None. Once you set the flags you want, everything is handled for you
.EXAMPLE
    ./run_simulation.ps1 -Config ontario
        Runs a simulation on the Ontario config
.EXAMPLE
    ./run_simulation.ps1 -GenScenario -Config ontario
        Generates the scenario file used by the simulator using the Ontario config. Running a sim
        like in examples 1 and 2 does this automatically and this is for when you just want the
        scenario file re-done (useful when debugging)
.EXAMPLE
    ./run_simulation.ps1 -GraphRegion -Config ontario
        Runs a simulation on the Ontario config and generates graphs per region since
        by default this is turned off (it takes a longer time to do then Aggregated graphs)
        and isn't always that useful)
.Link
    https://github.com/SimulationEverywhere-Models/Geography-Based-SEIRDS-Vaccinated
#>
[CmdletBinding()]
param(
    # The name of the config directory to run a simulation on (ex: ontario)
    [string]$Config= "",

    # The length of days to run the simulation (default=500)
    [string]$Days = 500,

    # Turns off all progress meters and loading animations
    [switch]$NoProgress = $False,

    # Generates graphs per region (default=off). Aggregated graphs of the whole are are always generated
    # Ex: Running a simulation on the 'ottawa' config will generate graphs relevant to Ottawa as a whole and if this flag is set
    # each region (Nepean, Kanata...) as well
    [switch]$GraphPerRegions = $False,

    # Specificy a name to the directory under which the results will be saved (default is a numbered folder name)
    [string]$DirName = "",

    # Cleans all results for a specified config
    [switch]$CleanAll = $False,

    # Cleans the results folder with the given name (Use in concordance with -Config)
    [string]$Clean = ""
)

# <Colors> #
    $RESET  = "[0m"
    $RED    = "[31m"
    $GREEN  = "[32m"
    $YELLOW = "[33m"
    $BLUE   = "[36m"
    $BOLD   = "[1m"
# </Colors> #


$private:Params = "Config", "Days", "NoProgress", "GraphPerRegions", "GenerateGraphsPerRegions", "DirName", "CleanAll", "Clean"
$private:ParamsNotNull = $False
foreach ($Param in $Params) { if ($PSBoundParameters.keys -like "*" + $Param + "*") { $ParamsNotNull = $True; break; } }
$Script:InvokeDir = Get-Location | Select-Object -ExpandProperty Path
$Script:HomeDir   = Split-Path -Parent $Script:MyInvocation.MyCommand.Path

# <Helpers> #
    <#
        .SYNOPSIS
        Verifies ALL dependencies are met
    #>
    function DependencyCheck() {
        Write-Verbose "Checking Dependencies..."

        # Setup dependency specific data
        $private:Dependencies = [Ordered]@{
            # dependency=version, website
            pwsh   = "PowerShell 7", "https://github.com/PowerShell/PowerShell/releases";
            python = "Python 3", "https://www.python.org/downloads/";
            conda  = "conda 4", "https://www.anaconda.com/products/individual#windows"
        }

        # Python Depedencies
        $private:Libs = "numpy", "geopandas", "matplotlib"
        try {
            # Loop through each dependency
            foreach ($private:Depends in $Dependencies.keys) {
                # Try and get the version
                $private:Version = cmd /c "$Depends --version"

                # If the version is incorrect or non-existant, throw an error
                if ( !($Version -clike "*" + ${Dependencies}.${Depends}[0] + "*") ) { throw 1 }

                # It was found
                Write-Verbose "$Depends ${GREEN}[FOUND]"
            }

            # Loop through each python dependency
            $private:CondaList = conda list
            foreach ($private:Depends in $Libs) {
                if ( !($CondaList -like "*${Depends}*") ) { throw 2 }
                Write-Verbose "$Depends ${GREEN}[FOUND]"
            }
        }
        catch {
            Write-Verbose "$Depends ${RED}[NOT FOUND]" -Verbose

            # Dependency Prints
            if ($Error[0].Exception.Message -eq 1) {
                $private:MinVersion = $Dependencies.$Depends[0]
                Write-Verbose $YELLOW"Check that '$Depends --version' contains this version $MinVersion"
                $private:Website = $Dependencies.$Depends[1]
                Write-Verbose $YELLOW"$Depends for Windows can be installed from here: ${BLUE}$Website"
                # Python Dependency Prints
            }
            elseif ($Error[0].Exception.Message -eq 2) {
                Write-Verbose $YELLOW'Check `conda list  | Select-String "'"$Depends"'"`'
                Write-Verbose $YELLOW'It can be installed using `conda install '$Depends'`'
            }
            else {
                Write-Error $Error[0].Exception.Message
            }

            break
        }

        Write-Verbose $GREEN"Completed Dependency Check.`n"$RESET
    } #DependencyCheck()

    <#
        .SYNOPSIS
        Computes and diplas whether the build was a success
        and how much time is took using the the stopwatch
        .PARAMETER Success
        True if the simulation was a success
        .PARAMETER private:StopWatch
        Stopwartch object containing how much time has passed
        .EXAMPLE
        ComputeBuildTime $True $null
            Displays success message but no execution time
    #>
    function ComputeBuildTime([bool] $Success, [System.Diagnostics.Stopwatch] $private:StopWatch = $null) {
        if ($stopWatch) {
            $Hours = $StopWatch.Elapsed.Hours
            $Minutes = $StopWatch.Elapsed.Minutes
            $Seconds = $StopWatch.Elapsed.Seconds
            $StopWatch.Stop()
        }

        $Color = (($Success) ? $GREEN : $RED)

        if ($Success) { Write-Host -NoNewline "`n${GREEN}Simulation Completed" }
        else { Write-Host -NoNewline "`n${RED}Simulation Failed" }

        if ($StopWatch) {
            Write-Host -NoNewline $Color" ("
            if ( $Hours -gt 0) { Write-Host -NoNewline "${Color}${Hours}h" }
            if ( $Minutes -gt 0 ) { Write-Host -NoNewline "${Color}${Minutes}m" }
            Write-Output "${Color}${Seconds}s)${RESET}"
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
    function ErrorCheck() {
        # 0 -> All is good
        if ($LASTEXITCODE -ne 0) {
            ComputeBuildTime $False $Stopwatch # Display failed message and time
            Set-Location $InvokeDir # Go back to the default location
        }
    }

    <#
        .SYNOPSIS
        Generates the scenario file for a region and places it in the config directory
        .PARAMETER Private:Config
        Config to generate scenario. Case sensitive
        .EXAMPLE
        GenerateScenario ontario
        .NOTES
        General notes
    #>
    function GenerateScenario([string] $Private:Config) {
        # Create output directory if non-existant
        if (!(Test-Path ".\Scripts\Input_Generator\output")) { New-Item ".\Scripts\Input_Generator\output\" -ItemType Directory | Out-Null }

        Write-Output "Generating ${BLUE}$Config${RESET} Scenario:"
        Set-Location .\Scripts\Input_Generator\
        python.exe generateScenario.py $Config $PROGRESS
        Set-Location $HomeDir
        ErrorCheck
    } #GenerateScenario()

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
    function GenerateGraphs([string] $LogFolder = "", [bool] $GenAggregate = $True, [bool] $GenRegions = $False) {
        Write-Output "Generating Graphs:"
        $GenFolder = ".\Scripts\Graph_Generator\"

        if ($LogFolder -eq "") {
            if (Test-Path logs/stats) { Remove-Item logs/stats -Recurse }
            New-Item logs/stats -ItemType Directory | Out-Null
            $LogFolder = "logs"
        }

        if ($GenRegions) {
            python ${GenFolder}graph_per_regions.py $Progress "-ld=$LogFolder"
            ErrorCheck
        }

        if ($GenAggregate) {
            python ${GenFolder}graph_aggregates.py $Progress "-ld=$LogFolder"
        }
    }

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
    function Clean([string] $Private:VisualizationDir, [string] $Private:Run, [bool] $Private:CleanAll = $False) {
            if ($CleanAll) {
                Write-Output "Removing ${YELLOW}all${RESET} runs for ${YELLOW}${Config}${RESET}"
                if (Test-Path $VisualizationDir) {
                    Remove-Item $VisualizationDir -Recurse -Verbose 4>&1 |
                    ForEach-Object { `Write-Host ($_.Message -replace '(.*)Target "(.*)"(.*)', ' $2') -ForegroundColor Red }
                }
                Write-Output ${GREEN}"Done."${RESET}
            }
            else {
                Write-Output "Removing ${YELLOW}${Run}${RESET} simulation run from ${YELLOW}${Config}${RESET}"
                if (Test-Path ${VisualizationDir}${Run}) {
                    Remove-Item ${VisualizationDir}${Run} -Recurse -Verbose 4>&1 |
                    ForEach-Object { `Write-Host ($_.Message -replace '(.*)Target "(.*)"(.*)', ' $2') -ForegroundColor Red }
                }
                Write-Output ${GREEN}"Done."${RESET}
            }
    } #Clean()
# </Helpers> #

if ($ParamsNotNull) {
    Set-Location $HomeDir

    # Setup global variables
    $Script:Progress = (($NoProgress) ? "-np" : "")

    # Setup Config variables
    if (Test-Path ".\Scripts\Input_Generator\${Config}") {
        $VisualizationDir = ".\Results\${Config}\"
        $Area = $Config.Split("_")[0]
    }
    else {
        Write-Output "${RED}Could not find ${BOLD}'${Config}'${RESET}${RED}. Check the spelling and verify that the directory is under ${YELLOW}'Scripts\Input_Generator\'${RESET}"
        exit -1
    }

    # Handles either of the clean flags
    if ($CleanAll -or $Clean -ne "") {
        if ($Config -eq "") { Write-Output "${RED}Config must be set${RESET}"; exit -1 }
        Clean $VisualizationDir $Clean $CleanAll
        Set-Location $InvokeDir
        break
    }

    # Check for required dependencies
    DependencyCheck

    # Used for execution time at the end of Main 
    $Local:Stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

    if ($DirName -ne "") { $VisualizationDir = $VisualizationDir + $DirName }
    else {
        $i = 1
        while ($True) {
            if ( !(Test-Path "${VisualizationDir}run${i}") ) {
                $VisualizationDir = $VisualizationDir + "run" + $i
                break
            }
            $i++
        }
    }

    if ( !(Test-Path "logs") ) { New-Item "logs" -ItemType Directory | Out-Null }
    if ( !(Test-Path $VisualizationDir) ) { New-Item $VisualizationDir -ItemType Directory | Out-Null }

    GenerateScenario $Config

    Write-Output "`nExecuting model for $Days days:"
    Set-Location bin
    .\pandemic-geographical_model.exe ..\Scripts\Input_Generator\output\scenario_${Config}.json $Days $Progress
    ErrorCheck
    Set-Location $HomeDir
    Write-Output "" # New line

    # Generate SEVIRDS graphs
    GenerateGraphs "" $True $GraphPerRegions

    try { $Private:Version = java --version }
    catch { $Version = "" }
    if ( ($Version -clike "*java 16*") ) {
        if ( !(Test-Path .\Scripts\Msg_Log_Parser\input) ) { New-Item .\Scripts\Msg_Log_Parser\input  -ItemType Directory | Out-Null }
        if ( !(Test-Path .\Scripts\Msg_Log_Parser\output) ) { New-Item .\Scripts\Msg_Log_Parser\output -ItemType Directory | Out-Null }
        Copy-Item .\Scripts\Input_Generator\output\scenario_${Config}.json .\Scripts\Msg_Log_Parser\input
        Copy-Item .\logs\pandemic_messages.txt .\Scripts\Msg_Log_Parser\input

        # Run message log parser
        Write-Output "`nRunning message log parser"
        Set-Location .\Scripts\Msg_Log_Parser
        java -jar sim.converter.glenn.jar "input" "output"
        Expand-Archive -LiteralPath output\pandemic_messages.zip -DestinationPath output
        Set-Location $HomeDir

        Move-Item .\Scripts\Msg_Log_Parser\output\messages.log $VisualizationDir
        Move-Item .\Scripts\Msg_Log_Parser\output\structure.json $VisualizationDir
        Remove-Item .\Scripts\Msg_Log_Parser\input -Recurse
        Remove-Item .\Scripts\Msg_Log_Parser\output -Recurse
        Remove-Item .\Scripts\Msg_Log_Parser/*.zip
        Write-Output "${GREEN}Done."
    }

    Copy-Item .\cadmium_gis\${Area}\${Area}.geojson $VisualizationDir
    Copy-Item .\cadmium_gis\${Area}\visualization.json $VisualizationDir
    Move-Item logs $VisualizationDir

    ComputeBuildTime $True $Stopwatch
    Write-Output "View results using the files in ${BOLD}${BLUE}${VisualizationDir}${RESET} and this web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
    Set-Location $InvokeDir
} else {
    if ($VerbosePreference) { Get-Help ${HomeDir}\RunSimulation.ps1 -full }
    else { Get-Help ${HomeDir}\RunSimulation.ps1 }
}