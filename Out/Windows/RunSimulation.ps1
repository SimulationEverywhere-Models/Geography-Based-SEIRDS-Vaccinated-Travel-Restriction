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
        - Python 3
        - geopandas (Python library installed through Conda)
        - matplotlib (Python library installed through Conda)
        - numpy (Python library installed through Conda)
        - java
        - Anaconda
        - PowerShell 7
.INPUTS
    None. Once you set the flags you want, everything is handled for you
.EXAMPLE
    .\RunSimulation.ps1 -Config ontario
        Runs a simulation on the Ontario config
.EXAMPLE
    .\RunSimulation.ps1 -GenScenario -Config ontario
        Generates the scenario file used by the simulator using the Ontario config. Running a sim
        like in examples 1 and 2 does this automatically and this is for when you just want the
        scenario file re-done (useful when debugging)
.EXAMPLE
    .\RunSimulation.ps1 -GraphRegion -Config ontario
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

    # Generates graphs per region for an already completed run
    [string]$GenRegionGraphs = "",

    # Specificy a name to the directory under which the results will be saved (default is a numbered folder name)
    [string]$DirName = "",

    # Cleans the simulation run with the inputed folder name such as '-Clean run1' (Needs to set the Config).
    # Setting the run name to _ cleans all the runs under the set config
    # Ex: -Clean _ ontario
    #   Will clean all runs under GIS_Viewer\ontario\
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


$private:Params = "Config", "Days", "NoProgress", "GraphPerRegions", "GenRegionGraphs", "DirName", "Clean"
$private:ParamsNotNull = $False
foreach ($Param in $Params) { if ($PSBoundParameters.keys -like "*" + $Param + "*") { $ParamsNotNull = $True; break; } }
$Script:InvokeDir = Get-Location | Select-Object -ExpandProperty Path
$Script:HomeDir   = Split-Path -Parent $Script:MyInvocation.MyCommand.Path

# <Helpers> #
    <#
        .SYNOPSIS
        Handles quitting the scripts and setting the correct location in the terminla
        .PARAMETER code
        Error code to exit with
    #>
    function Quit([int] $private:code=0) {
        Set-Location $InvokeDir
        exit $code
    }

    <#
        .SYNOPSIS
        Verifies ALL dependencies are met
    #>
    function DependencyCheck() {
        # Don't need to check again in the current shell
        if ($PythonCheck) {
            Write-Verbose "${GREEN}Dependencies already met`n${RESET}"
            return
        }

        Write-Verbose "Checking Dependencies..."

        # Setup dependency specific data
        $private:Dependencies = [Ordered]@{
            # dependency=version, website
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

            $Global:PythonCheck = $True
            Write-Verbose $GREEN"Completed Dependency Check.`n"$RESET
        }
        catch {
            Write-Verbose "$Depends ${RED}[NOT FOUND]" -Verbose

            # Dependency Prints
            if ($Error[0].Exception.Message -eq 1) {
                $private:MinVersion = $Dependencies.$Depends[0]
                Write-Verbose $YELLOW"Check that '$Depends --version' contains this version $MinVersion"
                $private:Website = $Dependencies.$Depends[1]
                Write-Verbose $YELLOW"$Depends for Windows can be installed from here: ${BLUE}$Website"
            }
            # Python Library Dependency Prints
            elseif ($Error[0].Exception.Message -eq 2) {
                Write-Verbose $YELLOW"Verify you have activated the correct conda environment"
                Write-Verbose $YELLOW'Check `conda list  | Select-String "'"$Depends"'"`'
                Write-Verbose $YELLOW'It can be installed using `conda install '$Depends'`'
            }
            else { Write-Error $Error[0].Exception.Message }

            $Global:PythonCheck = $False
        }

        if (!$PythonCheck) { Quit(-1) }
    } #DependencyCheck()

    <#
        .SYNOPSIS
        Computes and diplas whether the build was a success
        and how much time is took using the the stopwatch
        .PARAMETER private:Success
        True if the simulation was a success
        .PARAMETER private:StopWatch
        Stopwartch object containing how much time has passed
        .EXAMPLE
        ComputeBuildTime $True $null
            Displays success message but no execution time
    #>
    function ComputeBuildTime([bool] $private:Success=$False, [System.Diagnostics.Stopwatch] $private:StopWatch=$null) {
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
    }

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
            Quit($LASTEXITCODE)
        }
    }

    <#
        .SYNOPSIS
        Generates the scenario file for a region and places it in the config directory
        .PARAMETER private:Config
        Config to generate scenario. Case sensitive
    #>
    function GenerateScenario([string] $private:Config) {
        # Create output directory if non-existant
        if (!(Test-Path ".\Scripts\Input_Generator\output")) { New-Item ".\Scripts\Input_Generator\output\" -ItemType Directory | Out-Null }

        Write-Output "Generating ${BLUE}$Config${RESET} Scenario:"
        Set-Location .\Scripts\Input_Generator\
        python.exe generateScenario.py $Config $PROGRESS
        ErrorCheck
        Set-Location $HomeDir
    }

    <#
        .SYNOPSIS
        Generates all the graphs for a single run, provided the correct variables are set
        .PARAMETER private:LogFolder
        Path to the log folder (can be relative to where the script's directory)
        .PARAMETER private:GenAggregate
        True is the aggregated graphs should be generated
        .PARAMETER private:GenRegions
        True if the regional graphs should be generated
        .EXAMPLE
        GenerateGraphs "logs" $True $False
            Will generate the aggregated graphs using the data in the Geographical-Based-SEIRDS-Vaccinated/logs folder
    #>
    function GenerateGraphs([string] $private:LogFolder="", [bool] $private:GenAggregate=$True, [bool] $private:GenRegions=$False) {
        Write-Output "Generating Graphs:"
        $GenFolder = ".\Scripts\Graph_Generator"

        if ($LogFolder -eq "") {
            if (Test-Path "logs\stats") { Remove-Item logs\stats -Recurse }
            New-Item logs\stats -ItemType Directory | Out-Null
            $LogFolder = ".\logs"
        }

        if ($GenRegions) {
            python ${GenFolder}\graph_per_regions.py $Progress "-ld=$LogFolder"
            ErrorCheck
        }

        if ($GenAggregate) {
            python ${GenFolder}\graph_aggregates.py $Progress "-ld=$LogFolder"
            ErrorCheck
        }
    }

    <#
        .SYNOPSIS
        Cleans either all runs or a specified run
        .PARAMETER private:VisualizationDir
        Path to the simulations runs
        .PARAMETER private:Run
        Name of specific run to clean
        .EXAMPLE
        Clean "GIS_Viewer/ontario/simulation_runs/" "run1"
            Cleans run1 found in "GIS_Viewer/ontario/simulation_runs/"
    #>
    function Clean([string] $private:VisualizationDir, [string] $private:Run) {
        [string] $private:Dir=$VisualizationDir
        [string] $private:RunName="all$RESET runs"

        if ($Run -ne "_") { $Dir="$Dir$Run";  $RunName="$Run$RESET" }

        Write-Output "Removing ${YELLOW}${RunName} from the ${YELLOW}${Config}${RESET} config"
        if (Test-Path $Dir) {
            Remove-Item $Dir -Recurse -Verbose 4>&1 |
            ForEach-Object{ `Write-Host ($_.Message -replace'(.*)Target "(.*)"(.*)',' $2') -ForegroundColor Red}
        }
        Write-Output ${GREEN}"Done."${RESET}
    }
# </Helpers> #

if ($ParamsNotNull) {
    Set-Location $HomeDir

    # Setup global variables
    $Script:Progress = (($NoProgress) ? "-np" : "")

    # Setup Config variables
    if (Test-Path ".\Scripts\Input_Generator\${Config}") {
        $VisualizationDir = ".\Results\${Config}\"

        # Retrieve the area from the config variable
        # Ex: ontario_booster2 -> $Area = 'ontario'
        $Area = $Config.Split("_")[0]
    }
    else {
        Write-Output "${RED}Could not find ${BOLD}'${Config}'${RESET}${RED}. Check the spelling and verify that the directory is under ${YELLOW}'Scripts\Input_Generator\'${RESET}"
        Quit(-1)
    }

    # If clean then do this before the dependency check
    # so it's quicker and we don't have to worry about having
    # things installed like Python
    if ($Clean -ne "") {
        if ($Config -eq "") { Write-Output "${RED}Config must be set${RESET}"; Quit(-1) }
        Clean $VisualizationDir $Clean
        Quit
    }

    # Check for required dependencies
    DependencyCheck

    if ($GenRegionGraphs) {
        # A region must be set
        if ($Config -eq "") { Write-Output "${RED}Config must be set${RESET}"; Quit(-1) }
        if ( !(Test-Path "${VisualizationDir}${GenRegionGraphs}") ) { Write-Output "${RED}${BOLD}${GenRegionGraphs}${RESET}${RED} does not exist!${RESET}"; Quit(-1) }
        GenerateGraphs  "${VisualizationDir}${GenRegionGraphs}\logs" $False $True
        Quit
    }

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

    # Currently the GIS Viewer is dependent on Java
    # But this will change at some point
    try { $Private:Version = java --version }
    catch { $Version = "" }
    if ( ($Version -clike "*java 16*") ) {
        if ( !(Test-Path .\Scripts\Msg_Log_Parser\input)  ) { New-Item .\Scripts\Msg_Log_Parser\input  -ItemType Directory | Out-Null }
        if ( !(Test-Path .\Scripts\Msg_Log_Parser\output) ) { New-Item .\Scripts\Msg_Log_Parser\output -ItemType Directory | Out-Null }
        Copy-Item .\Scripts\Input_Generator\output\scenario_${Config}.json .\Scripts\Msg_Log_Parser\input
        Copy-Item .\logs\pandemic_messages.txt .\Scripts\Msg_Log_Parser\input

        # Run message log parser
        Write-Output "`nRunning message log parser"
        Set-Location .\Scripts\Msg_Log_Parser
        java -jar sim.converter.glenn.jar "input" "output"
        Expand-Archive -LiteralPath output\pandemic_messages.zip -DestinationPath output
        Set-Location $HomeDir

        Move-Item   .\Scripts\Msg_Log_Parser\output\messages.log   $VisualizationDir
        Move-Item   .\Scripts\Msg_Log_Parser\output\structure.json $VisualizationDir
        Remove-Item .\Scripts\Msg_Log_Parser\input  -Recurse
        Remove-Item .\Scripts\Msg_Log_Parser\output -Recurse
        Remove-Item .\Scripts\Msg_Log_Parser\*.zip
        Write-Output "${GREEN}Done.${RESET}"
    }

    Copy-Item .\cadmium_gis\${Area}\${Area}.geojson $VisualizationDir
    Copy-Item .\cadmium_gis\${Area}\visualization.json $VisualizationDir
    Move-Item logs $VisualizationDir

    ComputeBuildTime $True $Stopwatch
    Write-Host -NoNewline "View results using the files in ${BOLD}${BLUE}${VisualizationDir}${RESET}"
    if ( ($Version -clike "*java 16*") ) {
        Write-Host " and through the web viewer: ${BOLD}${BLUE}http://206.12.94.204:8080/arslab-web/1.3/app-gis-v2/index.html${RESET}"
    }
    else { Write-Host "" }
    Quit
} else {
    # Show the help if no params were inputed
    if ($VerbosePreference) { Get-Help ${HomeDir}\RunSimulation.ps1 -full }
    else { Get-Help ${HomeDir}\RunSimulation.ps1 }
}